#!/usr/bin/env python3
"""统一评测视觉 API：猫存在、猫排泄物存在、猫品种。

数据采用 JSONL 清单；没有标准答案的字段必须写 null，汇总时不会计分。
脚本只依赖 Python 标准库，支持 OpenAI-compatible Chat Completions 接口。
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import mimetypes
import os
import random
import re
import ssl
import statistics
import time
import urllib.error
import urllib.request
from collections import defaultdict
from concurrent.futures import Future, ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Any


PROMPT_VERSION = "cat-vision-v1"
IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".webp", ".bmp"}
TASK_FIELDS = ("has_cat", "has_cat_waste", "cat_breed")


def load_env_file(path: Path) -> None:
    """加载本地密钥文件；已有环境变量优先，避免意外覆盖。"""
    if not path.exists():
        return
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if "=" not in stripped:
            raise ValueError(f"{path}:{line_number} 必须使用 NAME=value 格式")
        name, value = stripped.split("=", 1)
        name = name.strip()
        value = value.strip().strip("\"").strip("'")
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
            raise ValueError(f"{path}:{line_number} 环境变量名无效")
        os.environ.setdefault(name, value)


def normalize_breed(value: str) -> str:
    return re.sub(r"\s+", " ", value.strip().replace("_", " ")).casefold()


def image_payload(path: Path, encoding: str) -> str:
    mime, _ = mimetypes.guess_type(path.name)
    mime = mime or "image/jpeg"
    encoded = base64.b64encode(path.read_bytes()).decode("ascii")
    if encoding == "bare_base64":
        return encoded
    if encoding == "data_url":
        return f"data:{mime};base64,{encoded}"
    raise ValueError(f"不支持的 image_encoding：{encoding}")


def load_manifest(paths: list[Path]) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    seen: set[str] = set()
    for manifest in paths:
        if not manifest.exists():
            raise FileNotFoundError(f"清单不存在：{manifest}")
        for line_number, line in enumerate(
            manifest.read_text(encoding="utf-8").splitlines(), 1
        ):
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError as error:
                raise ValueError(f"{manifest}:{line_number} JSON 错误：{error}") from error
            image = Path(str(record.get("image", "")))
            if not image.is_absolute():
                image = (manifest.parent / image).resolve()
            if not image.is_file() or image.suffix.casefold() not in IMAGE_EXTENSIONS:
                raise ValueError(f"{manifest}:{line_number} 图片无效：{image}")
            key = str(image)
            if key in seen:
                raise ValueError(f"图片重复：{image}")
            seen.add(key)

            truth = record.get("ground_truth", record)
            has_cat = truth.get("has_cat")
            has_waste = truth.get("has_cat_waste")
            breed = truth.get("cat_breed")
            if has_cat is not None and not isinstance(has_cat, bool):
                raise ValueError(f"{manifest}:{line_number} has_cat 必须是 bool 或 null")
            if has_waste is not None and not isinstance(has_waste, bool):
                raise ValueError(
                    f"{manifest}:{line_number} has_cat_waste 必须是 bool 或 null"
                )
            if breed is not None and not isinstance(breed, str):
                raise ValueError(f"{manifest}:{line_number} cat_breed 必须是字符串或 null")
            if breed is not None and has_cat is False:
                raise ValueError(f"{manifest}:{line_number} 无猫图片不能设置 cat_breed")

            records.append(
                {
                    "image": key,
                    "ground_truth": {
                        "has_cat": has_cat,
                        "has_cat_waste": has_waste,
                        "cat_breed": breed,
                    },
                    "group": str(record.get("group", "default")),
                    "source": str(record.get("source", manifest.name)),
                }
            )
    if not records:
        raise ValueError("清单中没有可评测图片")
    return records


def stratified_sample(
    records: list[dict[str, Any]], max_per_group: int, seed: int
) -> list[dict[str, Any]]:
    if max_per_group <= 0:
        return sorted(records, key=lambda item: item["image"])
    groups: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for record in records:
        groups[record["group"]].append(record)
    rng = random.Random(seed)
    selected: list[dict[str, Any]] = []
    for name in sorted(groups):
        group = groups[name][:]
        rng.shuffle(group)
        selected.extend(group[:max_per_group])
    return sorted(selected, key=lambda item: item["image"])


def allowed_breeds(records: list[dict[str, Any]]) -> dict[str, str]:
    values: dict[str, str] = {}
    for record in records:
        breed = record["ground_truth"]["cat_breed"]
        if breed:
            values[normalize_breed(breed)] = breed
    return values


def build_prompt(breeds: dict[str, str]) -> str:
    breed_text = ", ".join(sorted(breeds.values())) or "无预设品种"
    return f"""
你是严格的猫砂盆图像分类器。只依据图片像素判断，不做医学诊断，不解释。

返回三个字段：
1. has_cat：画面中是否能看到猫，必须为 true 或 false。
2. has_cat_waste：画面中是否能看到猫的粪便或尿液结团，必须为 true 或 false。普通猫砂、阴影、玩具和食物不算排泄物。
3. cat_breed：有猫时从允许列表中选择；无法可靠判断或不在列表中返回 "unknown"；没有猫时必须为 null。

允许的猫品种：{breed_text}

只返回一个合法 JSON 对象，禁止 Markdown 和额外文字：
{{"has_cat":true,"has_cat_waste":false,"cat_breed":"unknown"}}
""".strip()


def parse_prediction(
    text: str, breeds: dict[str, str]
) -> tuple[dict[str, Any], dict[str, str]]:
    cleaned = text.strip()
    cleaned = re.sub(r"^```(?:json)?\s*", "", cleaned, flags=re.IGNORECASE)
    cleaned = re.sub(r"\s*```$", "", cleaned)
    value = json.loads(cleaned)
    if not isinstance(value, dict):
        raise ValueError("模型输出不是 JSON 对象")

    prediction: dict[str, Any] = {}
    errors: dict[str, str] = {}
    for field in ("has_cat", "has_cat_waste"):
        candidate = value.get(field)
        if isinstance(candidate, bool):
            prediction[field] = candidate
        else:
            prediction[field] = None
            errors[field] = "必须是 JSON boolean"

    breed = value.get("cat_breed")
    if breed is None:
        prediction["cat_breed"] = None
    elif isinstance(breed, str):
        normalized = normalize_breed(breed)
        if normalized == "unknown":
            prediction["cat_breed"] = "unknown"
        elif normalized in breeds:
            prediction["cat_breed"] = breeds[normalized]
        else:
            prediction["cat_breed"] = breed
            errors["cat_breed"] = "不在允许品种列表中"
    else:
        prediction["cat_breed"] = None
        errors["cat_breed"] = "必须是字符串或 null"

    if prediction.get("has_cat") is False and prediction.get("cat_breed") is not None:
        errors["cat_breed"] = "has_cat=false 时必须为 null"
    if prediction.get("has_cat") is True and prediction.get("cat_breed") is None:
        errors["cat_breed"] = "has_cat=true 时必须是品种或 unknown"
    return prediction, errors


def extract_text(body: dict[str, Any]) -> str:
    content = body["choices"][0]["message"]["content"]
    if isinstance(content, str):
        return content
    if isinstance(content, list):
        return "".join(
            str(part.get("text", ""))
            for part in content
            if isinstance(part, dict) and part.get("type") in {None, "text"}
        )
    raise ValueError("API 响应中没有可识别的文本 content")


def request_model(
    provider: dict[str, Any],
    api_key: str,
    image: Path,
    prompt: str,
    timeout: int,
    retries: int,
) -> tuple[str, dict[str, Any], float]:
    payload: dict[str, Any] = {
        "model": provider["model"],
        "temperature": 0,
        "max_tokens": 120,
        "messages": [
            {
                "role": "user",
                "content": [
                    {"type": "text", "text": prompt},
                    {
                        "type": "image_url",
                        "image_url": {
                            "url": image_payload(
                                image, provider.get("image_encoding", "data_url")
                            )
                        },
                    },
                ],
            }
        ],
    }
    if provider.get("json_mode", True):
        payload["response_format"] = {"type": "json_object"}
    payload.update(provider.get("extra_body", {}))

    headers = {
        "Authorization": f"Bearer {api_key}",
        "Content-Type": "application/json",
        **provider.get("headers", {}),
    }
    request_data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    last_error: Exception | None = None
    started = time.perf_counter()
    for attempt in range(retries + 1):
        try:
            request = urllib.request.Request(
                provider["api_url"],
                data=request_data,
                headers=headers,
                method="POST",
            )
            ca_file = Path("/etc/ssl/cert.pem")
            ssl_context = ssl.create_default_context(
                cafile=str(ca_file) if ca_file.exists() else None
            )
            with urllib.request.urlopen(
                request, timeout=timeout, context=ssl_context
            ) as response:
                response_bytes = response.read()
            body = json.loads(response_bytes.decode("utf-8"))
            return extract_text(body), body, (time.perf_counter() - started) * 1000
        except urllib.error.HTTPError as error:
            detail = error.read(500).decode("utf-8", errors="replace").replace("\n", " ")
            last_error = RuntimeError(f"HTTP {error.code}: {detail}")
            if error.code != 429 and error.code < 500:
                raise last_error from error
            if attempt < retries:
                time.sleep(min(2**attempt, 8))
                continue
            raise last_error from error
        except (urllib.error.URLError, TimeoutError, json.JSONDecodeError) as error:
            last_error = error
            if attempt < retries:
                time.sleep(min(2**attempt, 8))
                continue
            raise RuntimeError(str(last_error)) from last_error
    raise RuntimeError(str(last_error))


def percentile_95(values: list[float]) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    return round(ordered[max(0, int(len(ordered) * 0.95 + 0.999) - 1)], 2)


def summarize(records: list[dict[str, Any]], provider: dict[str, Any]) -> dict[str, Any]:
    metrics: dict[str, Any] = {}
    for field in TASK_FIELDS:
        eligible = [r for r in records if r["ground_truth"].get(field) is not None]
        valid = [
            r
            for r in eligible
            if r.get("prediction") is not None
            and field not in r.get("field_errors", {})
        ]
        correct = sum(
            normalize_breed(str(r["prediction"][field]))
            == normalize_breed(str(r["ground_truth"][field]))
            if field == "cat_breed"
            else r["prediction"][field] == r["ground_truth"][field]
            for r in valid
        )
        metrics[field] = {
            "eligible": len(eligible),
            "valid_predictions": len(valid),
            "correct": correct,
            "accuracy_all": round(correct / len(eligible), 6) if eligible else None,
            "accuracy_valid": round(correct / len(valid), 6) if valid else None,
        }

    latencies = [float(r["latency_ms"]) for r in records if r.get("latency_ms")]
    prompt_tokens = sum(int(r.get("usage", {}).get("prompt_tokens", 0)) for r in records)
    completion_tokens = sum(
        int(r.get("usage", {}).get("completion_tokens", 0)) for r in records
    )
    input_price = provider.get("input_cny_per_million_tokens")
    output_price = provider.get("output_cny_per_million_tokens")
    cost = None
    if input_price is not None and output_price is not None:
        cost = round(
            prompt_tokens * float(input_price) / 1_000_000
            + completion_tokens * float(output_price) / 1_000_000,
            6,
        )
    return {
        "provider": provider["name"],
        "model": provider["model"],
        "images": len(records),
        "http_success": sum(r.get("http_success", False) for r in records),
        "json_success": sum(r.get("prediction") is not None for r in records),
        "tasks": metrics,
        "latency_ms": {
            "mean": round(statistics.fmean(latencies), 2) if latencies else None,
            "p95": percentile_95(latencies),
        },
        "usage": {
            "prompt_tokens": prompt_tokens,
            "completion_tokens": completion_tokens,
        },
        "estimated_cost_cny": cost,
    }


def fingerprint(
    provider: dict[str, Any], records: list[dict[str, Any]], prompt: str
) -> str:
    material = {
        "prompt_version": PROMPT_VERSION,
        "prompt": prompt,
        "provider": {
            key: provider.get(key)
            for key in ("name", "model", "api_url", "image_encoding", "extra_body")
        },
        "records": [
            {"image": r["image"], "ground_truth": r["ground_truth"]} for r in records
        ],
    }
    return hashlib.sha256(
        json.dumps(material, ensure_ascii=False, sort_keys=True).encode("utf-8")
    ).hexdigest()


def safe_name(value: str) -> str:
    return re.sub(r"[^a-zA-Z0-9._-]+", "_", value).strip("_") or "provider"


def load_results(path: Path, run_id: str) -> dict[str, dict[str, Any]]:
    existing: dict[str, dict[str, Any]] = {}
    if not path.exists():
        return existing
    for line in path.read_text(encoding="utf-8").splitlines():
        try:
            record = json.loads(line)
            if (
                record.get("run_id") == run_id
                and record.get("http_success")
                and record.get("prediction") is not None
            ):
                existing[record["image"]] = record
        except (json.JSONDecodeError, KeyError):
            continue
    return existing


def write_results(path: Path, records: dict[str, dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8") as file:
        for image in sorted(records):
            file.write(json.dumps(records[image], ensure_ascii=False) + "\n")
    temporary.replace(path)


def append_result(path: Path, record: dict[str, Any]) -> None:
    """追加单条结果作为运行日志；完成后再由 write_results 压缩去重。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a", encoding="utf-8") as file:
        file.write(json.dumps(record, ensure_ascii=False) + "\n")
        file.flush()


def run_provider(
    provider: dict[str, Any],
    records: list[dict[str, Any]],
    breeds: dict[str, str],
    output_dir: Path,
    timeout: int,
    retries: int,
    delay: float,
    workers: int,
) -> dict[str, Any]:
    key_env = provider.get("api_key_env")
    api_key = os.getenv(str(key_env), "")
    if not api_key:
        return {
            "provider": provider.get("name"),
            "model": provider.get("model"),
            "status": "skipped",
            "reason": f"未设置环境变量 {key_env}",
        }

    prompt = build_prompt(breeds)
    run_id = fingerprint(provider, records, prompt)
    stem = f"{safe_name(provider['name'])}__{safe_name(provider['model'])}__{run_id[:12]}"
    result_path = output_dir / f"{stem}.jsonl"
    summary_path = output_dir / f"{stem}.summary.json"
    existing = load_results(result_path, run_id)
    pending: list[tuple[int, dict[str, Any]]] = []
    for index, item in enumerate(records, 1):
        image = item["image"]
        if image in existing:
            continue
        pending.append((index, item))

    def evaluate(item: dict[str, Any]) -> dict[str, Any]:
        image = item["image"]
        record: dict[str, Any] = {
            "run_id": run_id,
            "provider": provider["name"],
            "model": provider["model"],
            **item,
            "http_success": False,
            "prediction": None,
            "field_errors": {},
            "raw_output": None,
            "error": None,
            "latency_ms": None,
            "usage": {},
        }
        try:
            raw, body, latency = request_model(
                provider,
                api_key,
                Path(image),
                prompt,
                timeout,
                retries,
            )
            record["http_success"] = True
            record["raw_output"] = raw
            record["latency_ms"] = round(latency, 2)
            record["usage"] = body.get("usage", {})
            try:
                prediction, field_errors = parse_prediction(raw, breeds)
                record["prediction"] = prediction
                record["field_errors"] = field_errors
            except (json.JSONDecodeError, ValueError) as error:
                record["error"] = f"输出格式错误：{error}"
        except Exception as error:  # 单张失败不终止整批
            record["error"] = str(error)
        if delay > 0:
            time.sleep(delay)
        return record

    completed = len(existing)
    executor = ThreadPoolExecutor(max_workers=workers)
    futures: dict[Future[dict[str, Any]], tuple[int, dict[str, Any]]] = {
        executor.submit(evaluate, item): (index, item) for index, item in pending
    }
    try:
        for future in as_completed(futures):
            _, item = futures[future]
            record = future.result()
            image = item["image"]
            existing[image] = record
            append_result(result_path, record)
            completed += 1
            state = "OK" if record.get("prediction") is not None else "ERROR"
            print(
                f"[{provider['name']} {completed}/{len(records)}] "
                f"{state} {Path(image).name}",
                flush=True,
            )
    except KeyboardInterrupt:
        for future in futures:
            future.cancel()
        executor.shutdown(wait=False, cancel_futures=True)
        raise
    else:
        executor.shutdown(wait=True)

    ordered = [existing[item["image"]] for item in records]
    write_results(result_path, existing)
    summary = summarize(ordered, provider)
    summary.update(
        {
            "status": "completed",
            "run_id": run_id,
            "prompt_version": PROMPT_VERSION,
            "result_file": str(result_path),
        }
    )
    summary_path.write_text(
        json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    return summary


def prepare_oxford(images: Path, annotations: Path, output: Path) -> None:
    labels: dict[str, tuple[bool, str]] = {}
    for line in annotations.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) < 4:
            continue
        stem, species_id = parts[0], int(parts[2])
        breed = re.sub(r"_[0-9]+$", "", stem).replace("_", " ")
        labels[stem] = (species_id == 1, breed)

    output.parent.mkdir(parents=True, exist_ok=True)
    count = 0
    with output.open("w", encoding="utf-8") as file:
        for image in sorted(images.iterdir()):
            if not image.is_file() or image.suffix.casefold() not in IMAGE_EXTENSIONS:
                continue
            if image.stem not in labels:
                continue
            is_cat, breed = labels[image.stem]
            record = {
                "image": str(image.resolve()),
                "ground_truth": {
                    "has_cat": is_cat,
                    "has_cat_waste": None,
                    "cat_breed": breed if is_cat else None,
                },
                "group": f"cat:{breed}" if is_cat else f"dog:{breed}",
                "source": "Oxford-IIIT Pet",
            }
            file.write(json.dumps(record, ensure_ascii=False) + "\n")
            count += 1
    print(f"已写入 {count} 张图片：{output}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    prepare = subparsers.add_parser("prepare-oxford", help="生成 Oxford 评测清单")
    prepare.add_argument("--images", type=Path, required=True)
    prepare.add_argument("--list", type=Path, required=True)
    prepare.add_argument("--output", type=Path, required=True)

    run = subparsers.add_parser("run", help="运行多模型评测")
    run.add_argument("--manifest", type=Path, action="append", required=True)
    run.add_argument("--providers", type=Path, required=True)
    run.add_argument("--output-dir", type=Path, default=Path("vision_benchmark_results"))
    run.add_argument("--max-per-group", type=int, default=0)
    run.add_argument(
        "--max-no-cat",
        type=int,
        default=0,
        help="最多保留 N 张 has_cat=false 图片；0 表示不限制",
    )
    run.add_argument("--smoke", action="store_true", help="仅取一张图片检查接口")
    run.add_argument("--seed", type=int, default=20260810)
    run.add_argument("--timeout", type=int, default=120)
    run.add_argument("--retries", type=int, default=2)
    run.add_argument("--delay", type=float, default=1.0)
    run.add_argument("--workers", type=int, default=1, help="单个模型并发请求数")
    run.add_argument(
        "--env-file", type=Path, default=Path(".env.local"), help="本地密钥文件"
    )
    args = parser.parse_args()

    if getattr(args, "workers", 1) < 1:
        parser.error("--workers 必须大于等于 1")

    if args.command == "prepare-oxford":
        prepare_oxford(args.images, args.list, args.output)
        return

    load_env_file(args.env_file)
    records = stratified_sample(
        load_manifest(args.manifest), args.max_per_group, args.seed
    )
    if args.max_no_cat > 0:
        no_cat = [r for r in records if r["ground_truth"]["has_cat"] is False]
        other = [r for r in records if r["ground_truth"]["has_cat"] is not False]
        rng = random.Random(args.seed)
        rng.shuffle(no_cat)
        records = sorted(other + no_cat[: args.max_no_cat], key=lambda item: item["image"])
    if args.smoke:
        records = records[:1]
    breeds = allowed_breeds(records)
    providers = json.loads(args.providers.read_text(encoding="utf-8"))
    if not isinstance(providers, list):
        raise SystemExit("providers 配置必须是 JSON 数组")
    args.output_dir.mkdir(parents=True, exist_ok=True)

    summaries = []
    for provider in providers:
        if not provider.get("enabled", True):
            summaries.append(
                {
                    "provider": provider.get("name"),
                    "model": provider.get("model"),
                    "status": "skipped",
                    "reason": provider.get("reason", "配置已禁用"),
                }
            )
            continue
        summaries.append(
            run_provider(
                provider,
                records,
                breeds,
                args.output_dir,
                args.timeout,
                args.retries,
                args.delay,
                args.workers,
            )
        )

    combined = args.output_dir / "comparison.json"
    combined.write_text(
        json.dumps(summaries, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print(json.dumps(summaries, ensure_ascii=False, indent=2))
    print(f"总对比：{combined}")


if __name__ == "__main__":
    main()
