#!/usr/bin/env python3
"""比较国产视觉模型对 Roboflow 宠物粪便四分类测试集的准确率。"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import statistics
import time
from collections import Counter, defaultdict
from concurrent.futures import Future, ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Any

from cat_vision_benchmark import IMAGE_EXTENSIONS, load_env_file, request_model


LABELS = ("normal", "diare", "lack-of-water", "soft-poop")
PROMPT_VERSION = "pet-poop-four-class-v1"
PROMPT = """
你是严格的宠物粪便外观图片分类器。只依据图片中可见粪便的形态、含水程度和成形程度判断，不做兽医诊断，不输出建议。

必须从下面四个数据集标签中选择一个：
- normal：正常、成形且不过分干硬的粪便
- diare：明显水样、稀液状或严重不成形的腹泻粪便
- lack-of-water：明显干燥、坚硬、小颗粒或缺水状粪便
- soft-poop：偏软、不够成形，但未达到明显水样腹泻

只返回合法 JSON，禁止 Markdown 和额外文字：
{"label":"normal"}
""".strip()


def load_providers(paths: list[Path]) -> list[dict[str, Any]]:
    providers: list[dict[str, Any]] = []
    for path in paths:
        value = json.loads(path.read_text(encoding="utf-8"))
        if not isinstance(value, list):
            raise ValueError(f"{path} 顶层必须是数组")
        providers.extend(value)
    if not providers:
        raise ValueError("没有模型配置")
    return providers


def load_images(root: Path) -> list[dict[str, str]]:
    records: list[dict[str, str]] = []
    for label in LABELS:
        directory = root / label
        if not directory.is_dir():
            raise FileNotFoundError(f"缺少类别目录：{directory}")
        for path in sorted(directory.rglob("*")):
            if path.is_file() and path.suffix.casefold() in IMAGE_EXTENSIONS:
                name = path.name
                base_id = re.split(r"_jpg\.rf\.", name, maxsplit=1)[0]
                records.append(
                    {
                        "image": str(path.resolve()),
                        "relative_image": str(path.resolve().relative_to(Path.cwd().resolve())),
                        "expected_label": label,
                        "base_id": f"{label}/{base_id}",
                    }
                )
    if not records:
        raise ValueError(f"{root} 中没有图片")
    return records


def parse_label(text: str) -> str:
    cleaned = text.strip()
    cleaned = re.sub(r"^```(?:json)?\s*", "", cleaned, flags=re.I)
    cleaned = re.sub(r"\s*```$", "", cleaned)
    value = json.loads(cleaned)
    if not isinstance(value, dict):
        raise ValueError("输出不是 JSON 对象")
    label = str(value.get("label", "")).strip().casefold()
    if label not in LABELS:
        raise ValueError(f"label 不在允许列表：{label!r}")
    return label


def run_id(provider: dict[str, Any], records: list[dict[str, str]]) -> str:
    material = {
        "prompt_version": PROMPT_VERSION,
        "prompt": PROMPT,
        "provider": {
            key: provider.get(key)
            for key in ("name", "model", "api_url", "image_encoding", "extra_body")
        },
        "records": [
            {
                "image": row["image"],
                "size": Path(row["image"]).stat().st_size,
                "label": row["expected_label"],
            }
            for row in records
        ],
    }
    return hashlib.sha256(
        json.dumps(material, ensure_ascii=False, sort_keys=True).encode("utf-8")
    ).hexdigest()


def safe(value: str) -> str:
    return re.sub(r"[^a-zA-Z0-9._-]+", "_", value).strip("_") or "model"


def load_latest(path: Path, fingerprint: str) -> dict[str, dict[str, Any]]:
    rows: dict[str, dict[str, Any]] = {}
    if not path.exists():
        return rows
    for line in path.read_text(encoding="utf-8").splitlines():
        try:
            row = json.loads(line)
            if (
                row.get("run_id") == fingerprint
                and row.get("http_success")
                and row.get("predicted_label") in LABELS
            ):
                rows[row["image"]] = row
        except (json.JSONDecodeError, KeyError):
            continue
    return rows


def load_final(path: Path, fingerprint: str) -> dict[str, dict[str, Any]]:
    """读取每张图片的最后一条记录，包括需要进入报告的失败记录。"""
    rows: dict[str, dict[str, Any]] = {}
    if not path.exists():
        raise FileNotFoundError(f"结果文件不存在：{path}")
    for line in path.read_text(encoding="utf-8").splitlines():
        row = json.loads(line)
        if row.get("run_id") == fingerprint:
            rows[row["image"]] = row
    return rows


def result_path_for(
    provider: dict[str, Any], records: list[dict[str, str]], output_dir: Path
) -> tuple[str, Path]:
    fingerprint = run_id(provider, records)
    return fingerprint, output_dir / (
        f"{safe(provider['name'])}__{safe(provider['model'])}__{fingerprint[:12]}.jsonl"
    )


def append(path: Path, row: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a", encoding="utf-8") as stream:
        stream.write(json.dumps(row, ensure_ascii=False) + "\n")
        stream.flush()


def compact(path: Path, rows: dict[str, dict[str, Any]]) -> None:
    temporary = path.with_suffix(".jsonl.tmp")
    with temporary.open("w", encoding="utf-8") as stream:
        for image in sorted(rows):
            stream.write(json.dumps(rows[image], ensure_ascii=False) + "\n")
    temporary.replace(path)


def usage_tokens(row: dict[str, Any], key: str) -> int:
    usage = row.get("usage") or {}
    return int(usage.get(key, 0) or 0)


def summarize(rows: list[dict[str, Any]], provider: dict[str, Any]) -> dict[str, Any]:
    correct = sum(row.get("predicted_label") == row["expected_label"] for row in rows)
    valid = sum(row.get("predicted_label") in LABELS for row in rows)
    per_class: dict[str, Any] = {}
    confusion: dict[str, dict[str, int]] = {}
    for expected in LABELS:
        members = [row for row in rows if row["expected_label"] == expected]
        per_class[expected] = {
            "total": len(members),
            "correct": sum(row.get("predicted_label") == expected for row in members),
        }
        confusion[expected] = dict(
            Counter(str(row.get("predicted_label") or "ERROR") for row in members)
        )
    by_base: dict[str, list[bool]] = defaultdict(list)
    for row in rows:
        by_base[row["base_id"]].append(
            row.get("predicted_label") == row["expected_label"]
        )
    base_macro = statistics.fmean(
        sum(values) / len(values) for values in by_base.values()
    )
    latencies = [float(row["latency_ms"]) for row in rows if row.get("latency_ms")]
    ordered_latency = sorted(latencies)
    p95 = (
        ordered_latency[max(0, int(len(ordered_latency) * 0.95 + 0.999) - 1)]
        if ordered_latency
        else None
    )
    input_tokens = sum(usage_tokens(row, "prompt_tokens") for row in rows)
    output_tokens = sum(usage_tokens(row, "completion_tokens") for row in rows)
    cost = (
        input_tokens * float(provider.get("input_cny_per_million_tokens", 0))
        + output_tokens * float(provider.get("output_cny_per_million_tokens", 0))
    ) / 1_000_000
    return {
        "provider": provider["name"],
        "model": provider["model"],
        "total": len(rows),
        "valid": valid,
        "correct": correct,
        "accuracy": correct / len(rows),
        "source_macro_accuracy": base_macro,
        "unique_source_ids": len(by_base),
        "per_class": per_class,
        "confusion_matrix": confusion,
        "mean_latency_ms": statistics.fmean(latencies) if latencies else None,
        "p95_latency_ms": p95,
        "input_tokens": input_tokens,
        "output_tokens": output_tokens,
        "estimated_cost_cny": cost,
        "errors": dict(Counter(row.get("error") or "未知错误" for row in rows if row.get("predicted_label") not in LABELS)),
    }


def evaluate_provider(
    provider: dict[str, Any],
    records: list[dict[str, str]],
    output_dir: Path,
    workers: int,
    delay: float,
    timeout: int,
    retries: int,
) -> tuple[list[dict[str, Any]], dict[str, Any], Path]:
    api_key = os.getenv(str(provider.get("api_key_env")), "")
    if not api_key:
        raise RuntimeError(f"未设置 {provider.get('api_key_env')}")
    fingerprint, result_path = result_path_for(provider, records, output_dir)
    existing = load_latest(result_path, fingerprint)
    pending = [row for row in records if row["image"] not in existing]
    print(f"{provider['model']}: 已完成 {len(existing)}，待处理 {len(pending)}")

    def evaluate(item: dict[str, str]) -> dict[str, Any]:
        row: dict[str, Any] = {
            "run_id": fingerprint,
            "prompt_version": PROMPT_VERSION,
            "provider": provider["name"],
            "model": provider["model"],
            **item,
            "http_success": False,
            "predicted_label": None,
            "correct": False,
            "raw_output": None,
            "latency_ms": None,
            "usage": {},
            "error": None,
        }
        try:
            raw, body, latency = request_model(
                provider, api_key, Path(item["image"]), PROMPT, timeout, retries
            )
            row["http_success"] = True
            row["raw_output"] = raw
            row["latency_ms"] = round(latency, 2)
            row["usage"] = body.get("usage", {})
            try:
                row["predicted_label"] = parse_label(raw)
                row["correct"] = row["predicted_label"] == item["expected_label"]
            except (json.JSONDecodeError, ValueError) as error:
                row["error"] = f"输出格式错误：{error}"
        except Exception as error:
            row["error"] = str(error)
        if delay:
            time.sleep(delay)
        return row

    executor = ThreadPoolExecutor(max_workers=workers)
    futures: dict[Future[dict[str, Any]], dict[str, str]] = {
        executor.submit(evaluate, item): item for item in pending
    }
    completed = len(existing)
    try:
        for future in as_completed(futures):
            row = future.result()
            existing[row["image"]] = row
            append(result_path, row)
            completed += 1
            state = "OK" if row.get("predicted_label") in LABELS else "ERROR"
            print(f"[{provider['name']} {completed}/{len(records)}] {state} {Path(row['image']).name}", flush=True)
    finally:
        executor.shutdown(wait=True)
    compact(result_path, existing)
    ordered = [existing[item["image"]] for item in records]
    summary = summarize(ordered, provider)
    return ordered, summary, result_path


def pct(value: float) -> str:
    return f"{value:.2%}"


def seconds(value: float | None) -> str:
    return f"{value / 1000:.2f}s" if value is not None else "—"


def generate_report(
    records: list[dict[str, str]],
    results: list[tuple[list[dict[str, Any]], dict[str, Any], Path]],
    output: Path,
) -> None:
    if len(results) != 2:
        raise ValueError("报告目前要求恰好两个模型")
    a_rows, a, a_path = results[0]
    b_rows, b, b_path = results[1]
    a_by_image = {row["image"]: row for row in a_rows}
    b_by_image = {row["image"]: row for row in b_rows}
    counts = Counter(row["expected_label"] for row in records)
    unique_ids = len({row["base_id"] for row in records})
    source_url = "https://universe.roboflow.com/poo-detection/pet-poop-classifier"
    lines = [
        "# Roboflow 宠物粪便四分类视觉模型评测报告",
        "",
        f"评测图片：{len(records)} 张（同源原图 ID：{unique_ids}）  ",
        f"模型：`{a['model']}`、`{b['model']}`  ",
        f"数据来源：[Pet Poop Classifier]({source_url})",
        "",
        "## 1. 结论摘要",
        "",
        "| 模型 | 总体准确率 | 同源图宏平均 | 有效输出 | 平均延迟 | P95延迟 | 输入Token | 输出Token | 估算费用 |",
        "|---|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for summary in (a, b):
        lines.append(
            f"| `{summary['model']}` | {summary['correct']}/{summary['total']}（{pct(summary['accuracy'])}） "
            f"| {pct(summary['source_macro_accuracy'])} | {summary['valid']}/{summary['total']} "
            f"| {seconds(summary['mean_latency_ms'])} | {seconds(summary['p95_latency_ms'])} "
            f"| {summary['input_tokens']:,} | {summary['output_tokens']:,} "
            f"| ¥{summary['estimated_cost_cny']:.3f} |"
        )
    winner = a if a["accuracy"] >= b["accuracy"] else b
    lines.extend(
        [
            "",
            f"- 本测试中总体准确率更高的是 `{winner['model']}`：{pct(winner['accuracy'])}。",
            f"- Roboflow 页面标称其数据集专用模型准确率为 96.6%；本次 `{a['model']}` 比该数字低 {96.6 - a['accuracy'] * 100:.2f} 个百分点，`{b['model']}` 低 {96.6 - b['accuracy'] * 100:.2f} 个百分点。该对比仅供参考，因为本地 test 有 307 张，而页面 v1 显示 204 张，且 Roboflow 模型是在该数据集上专门训练的。",
            "- 该数据集四类图片全部含有粪便，因此只能评估粪便状态分类，不能评估‘有没有粪便’的查准率、召回率或误报率。",
            "- 文件中存在增强图或同源变体；总体准确率按 307 个文件计算，同源图宏平均用于降低重复变体的权重。",
            "- 标签 `diare` 是数据集原始拼写，报告和模型输出均原样保留。结果只描述图片分类能力，不构成兽医诊断。",
            f"- 运行质量：`{a['model']}` 有效输出 {a['valid']}/{a['total']}；`{b['model']}` 有效输出 {b['valid']}/{b['total']}。GLM 的 2 张失败均为平台内容安全过滤，并按错误计入总体准确率。",
            "",
            "## 2. 数据分布",
            "",
            "| 类别 | 图片数 | 含义 |",
            "|---|---:|---|",
            f"| normal | {counts['normal']} | 正常、成形 |",
            f"| diare | {counts['diare']} | 水样或明显腹泻 |",
            f"| lack-of-water | {counts['lack-of-water']} | 干燥、坚硬或缺水状 |",
            f"| soft-poop | {counts['soft-poop']} | 偏软但非明显水样 |",
            "",
            "## 3. 各类别准确率",
            "",
            f"| 类别 | 图片数 | `{a['model']}` | `{b['model']}` |",
            "|---|---:|---:|---:|",
        ]
    )
    for label in LABELS:
        av = a["per_class"][label]
        bv = b["per_class"][label]
        lines.append(
            f"| {label} | {av['total']} | {av['correct']}/{av['total']}（{pct(av['correct']/av['total'])}） "
            f"| {bv['correct']}/{bv['total']}（{pct(bv['correct']/bv['total'])}） |"
        )
    lines.extend(["", "## 4. 混淆矩阵", ""])
    for summary in (a, b):
        lines.extend(
            [
                f"### {summary['model']}",
                "",
                "| 标准答案 \\ 预测 | normal | diare | lack-of-water | soft-poop | ERROR |",
                "|---|---:|---:|---:|---:|---:|",
            ]
        )
        for expected in LABELS:
            row = summary["confusion_matrix"][expected]
            lines.append(
                f"| {expected} | {row.get('normal',0)} | {row.get('diare',0)} "
                f"| {row.get('lack-of-water',0)} | {row.get('soft-poop',0)} | {row.get('ERROR',0)} |"
            )
        lines.append("")
    lines.extend(
        [
            "## 5. 每张图片结果",
            "",
            "`✓/✗` 表示与文件夹标准标签是否一致。",
            "",
            f"| 图片 | 标准标签 | `{a['model']}`预测 | 对错 | 延迟 | `{b['model']}`预测 | 对错 | 延迟 |",
            "|---|---|---|---|---:|---|---|---:|",
        ]
    )
    for item in records:
        ar = a_by_image[item["image"]]
        br = b_by_image[item["image"]]
        relative = item["relative_image"]
        display = Path(item["image"]).name
        ap = ar.get("predicted_label") or "ERROR"
        bp = br.get("predicted_label") or "ERROR"
        lines.append(
            f"| [`{display}`](../{relative}) | {item['expected_label']} | {ap} "
            f"| {'✓' if ar.get('correct') else '✗'} | {seconds(ar.get('latency_ms'))} "
            f"| {bp} | {'✓' if br.get('correct') else '✗'} | {seconds(br.get('latency_ms'))} |"
        )
    lines.extend(
        [
            "",
            "## 6. 原始结果",
            "",
            f"- `{a['model']}`：`{a_path}`",
            f"- `{b['model']}`：`{b_path}`",
            "",
        ]
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, default=Path("datasets/test"))
    parser.add_argument("--providers", type=Path, nargs="+", required=True)
    parser.add_argument("--env-file", type=Path, default=Path(".env.local"))
    parser.add_argument("--output-dir", type=Path, default=Path("vision_benchmark_results/poop-test"))
    parser.add_argument("--report", type=Path, default=Path("docs/poop-vision-benchmark-results.md"))
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--delay", type=float, default=0.2)
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--retries", type=int, default=2)
    parser.add_argument("--report-only", action="store_true", help="只从已有 JSONL 重建报告，不调用 API")
    args = parser.parse_args()
    load_env_file(args.env_file)
    records = load_images(args.data)
    providers = load_providers(args.providers)
    if args.report_only:
        results = []
        for provider in providers:
            fingerprint, path = result_path_for(provider, records, args.output_dir)
            existing = load_final(path, fingerprint)
            if len(existing) != len(records):
                raise RuntimeError(
                    f"{provider['model']} 结果不完整：{len(existing)}/{len(records)}"
                )
            ordered = [existing[item["image"]] for item in records]
            results.append((ordered, summarize(ordered, provider), path))
    else:
        results = [
            evaluate_provider(
                provider, records, args.output_dir, args.workers, args.delay, args.timeout, args.retries
            )
            for provider in providers
        ]
    generate_report(records, results, args.report)
    comparison = args.output_dir / "comparison.json"
    comparison.write_text(
        json.dumps([summary for _, summary, _ in results], ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    print(f"报告：{args.report}")
    print(f"汇总：{comparison}")


if __name__ == "__main__":
    main()
