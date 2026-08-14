#!/usr/bin/env python3
"""
用视觉大模型 API 评测 Oxford-IIIT Pet 数据集。

输出：
  1. results.jsonl：每张图片的真实标签、模型回答和原始输出
  2. summary.json：物种识别、猫品种识别、失败率等统计

默认目录正是用户当前的下载目录，也可以通过命令行参数覆盖。
API 请求采用 OpenAI-compatible Chat Completions 格式；如果你的服务商
格式不同，只需要修改 call_model() 中的 payload 和解析部分。
"""

from __future__ import annotations

import argparse
import base64
import json
import mimetypes
import os
import re
import time
from pathlib import Path
from typing import Any

import requests


DEFAULT_IMAGE_DIR = Path("/Users/mengqixiao/Downloads/images")
DEFAULT_ANNOTATIONS_LIST = Path(
    "/Users/mengqixiao/Downloads/annotations/list.txt"
)
DEFAULT_OUTPUT = Path("oxford_pet_results.jsonl")
DEFAULT_SUMMARY = Path("oxford_pet_summary.json")

CAT_BREEDS = {
    "Abyssinian",
    "Bengal",
    "Birman",
    "Bombay",
    "British Shorthair",
    "Egyptian Mau",
    "Maine Coon",
    "Persian",
    "Ragdoll",
    "Russian Blue",
    "Siamese",
    "Sphynx",
}

CAT_BREED_ALIASES = {
    "阿比西尼亚猫": "Abyssinian",
    "孟加拉猫": "Bengal",
    "伯曼猫": "Birman",
    "孟买猫": "Bombay",
    "英国短毛猫": "British Shorthair",
    "英短": "British Shorthair",
    "埃及 mau": "Egyptian Mau",
    "埃及猫": "Egyptian Mau",
    "缅因猫": "Maine Coon",
    "缅因库恩猫": "Maine Coon",
    "波斯猫": "Persian",
    "布偶猫": "Ragdoll",
    "俄罗斯蓝猫": "Russian Blue",
    "暹罗猫": "Siamese",
    "斯芬克斯猫": "Sphynx",
    "无毛猫": "Sphynx",
}


def normalize_name(value: str) -> str:
    """统一品种名称中的大小写、下划线和多余空格。"""
    value = value.strip().replace("_", " ").replace("-", " ")
    value = re.sub(r"\s+", " ", value)
    return value.casefold()


def breed_from_stem(stem: str) -> str:
    """从 Oxford 图片名，例如 British_Shorthair_10，得到品种名。"""
    prefix = re.sub(r"_[0-9]+$", "", stem)
    return prefix.replace("_", " ")


def infer_species_from_stem(stem: str) -> str:
    """Oxford-IIIT Pet 规则：品种首字母大写为猫，小写为狗。"""
    return "cat" if stem[:1].isupper() else "dog"


def load_ground_truth(annotation_list: Path) -> dict[str, dict[str, Any]]:
    """读取 list.txt；对不在 list.txt 中的图片使用官方文件名规则补齐。"""
    labels: dict[str, dict[str, Any]] = {}

    if annotation_list.exists():
        for line in annotation_list.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue

            parts = line.split()
            if len(parts) < 4:
                continue

            stem = parts[0]
            species_id = int(parts[2])
            labels[stem] = {
                "is_cat": species_id == 1,
                "breed": breed_from_stem(stem),
                "source": "list.txt",
            }

    return labels


def image_to_data_url(image_path: Path) -> str:
    mime_type, _ = mimetypes.guess_type(image_path.name)
    mime_type = mime_type or "image/jpeg"
    encoded = base64.b64encode(image_path.read_bytes()).decode("ascii")
    return f"data:{mime_type};base64,{encoded}"


def build_prompt() -> str:
    breeds = ", ".join(sorted(CAT_BREEDS))
    return f"""
你是一个严格的图像分类评测器。只依据图片内容回答，不要根据文件名猜测。

请完成两项任务：
1. 判断图片中是否有猫，字段 is_cat 必须是 true 或 false。
2. 如果 is_cat 为 true，从下面的 12 个品种中选择一个；如果不是猫，breed 必须为 null。

允许的猫品种（必须原样返回英文名称）：{breeds}

只返回合法 JSON，不要 Markdown，不要解释：
{{"is_cat": true, "breed": "Siamese"}}
""".strip()


def call_model(
    image_path: Path,
    api_url: str,
    api_key: str,
    model: str,
    timeout: int,
    max_retries: int,
) -> tuple[dict[str, Any], str]:
    """调用 OpenAI-compatible 视觉 API，返回解析后的预测和原始文本。"""
    payload = {
        "model": model,
        "temperature": 0,
        "messages": [
            {
                "role": "user",
                "content": [
                    {"type": "text", "text": build_prompt()},
                    {
                        "type": "image_url",
                        "image_url": {"url": image_to_data_url(image_path)},
                    },
                ],
            }
        ],
    }

    headers = {
        "Authorization": f"Bearer {api_key}",
        "Content-Type": "application/json",
    }

    last_error = ""
    for attempt in range(max_retries + 1):
        try:
            response = requests.post(
                api_url,
                headers=headers,
                json=payload,
                timeout=timeout,
            )
            response.raise_for_status()
            body = response.json()
            raw_text = extract_text(body)
            prediction = parse_prediction(raw_text)
            return prediction, raw_text
        except Exception as error:  # noqa: BLE001 - 记录单张图片失败并继续
            last_error = str(error)
            if attempt < max_retries:
                time.sleep(2**attempt)

    raise RuntimeError(last_error)


def extract_text(body: dict[str, Any]) -> str:
    """兼容常见的字符串 content 和分段 content。"""
    content = body["choices"][0]["message"]["content"]
    if isinstance(content, str):
        return content
    if isinstance(content, list):
        text_parts = []
        for part in content:
            if isinstance(part, dict) and part.get("type") == "text":
                text_parts.append(str(part.get("text", "")))
        return "".join(text_parts)
    return str(content)


def parse_json_object(text: str) -> dict[str, Any]:
    """解析 JSON；即使模型错误地包了一层 Markdown，也尽量提取对象。"""
    text = text.strip()
    text = re.sub(r"^```(?:json)?\s*", "", text, flags=re.IGNORECASE)
    text = re.sub(r"\s*```$", "", text)

    try:
        value = json.loads(text)
    except json.JSONDecodeError:
        start = text.find("{")
        end = text.rfind("}")
        if start < 0 or end <= start:
            raise ValueError(f"模型没有返回 JSON：{text}")
        value = json.loads(text[start : end + 1])

    if not isinstance(value, dict):
        raise ValueError(f"模型返回的不是 JSON 对象：{text}")
    return value


def parse_bool(value: Any) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        normalized = value.strip().casefold()
        if normalized in {"true", "1", "yes", "是", "有", "猫"}:
            return True
        if normalized in {"false", "0", "no", "否", "没有", "狗"}:
            return False
    raise ValueError(f"is_cat 不是可识别的布尔值：{value}")


def normalize_prediction_breed(value: Any) -> str | None:
    if value is None:
        return None

    text = str(value).strip()
    if not text or normalize_name(text) in {"null", "none", "无", "不适用"}:
        return None

    for alias, canonical in CAT_BREED_ALIASES.items():
        if alias.casefold() in text.casefold():
            return canonical

    normalized = normalize_name(text)
    for breed in CAT_BREEDS:
        if normalized == normalize_name(breed):
            return breed

    return text


def parse_prediction(text: str) -> dict[str, Any]:
    value = parse_json_object(text)
    is_cat = parse_bool(value.get("is_cat"))
    breed = normalize_prediction_breed(value.get("breed"))
    return {"is_cat": is_cat, "breed": breed}


def load_existing_results(path: Path) -> dict[str, dict[str, Any]]:
    results: dict[str, dict[str, Any]] = {}
    if not path.exists():
        return results

    for line in path.read_text(encoding="utf-8").splitlines():
        try:
            record = json.loads(line)
            results[record["image"]] = record
        except (json.JSONDecodeError, KeyError):
            continue
    return results


def calculate_summary(results: list[dict[str, Any]]) -> dict[str, Any]:
    successful = [record for record in results if record.get("success")]
    failed = len(results) - len(successful)

    summary: dict[str, Any] = {
        "total_images": len(results),
        "successful_api_calls": len(successful),
        "failed_api_calls": failed,
        "api_success_rate": round(len(successful) / len(results), 6)
        if results
        else 0,
    }

    if not successful:
        summary["message"] = "没有成功的 API 结果，暂时无法计算准确率。"
        return summary

    species_correct = 0
    actual_cats = 0
    actual_cats_predicted_as_cats = 0
    cat_breed_correct = 0
    detected_cat_breed_correct = 0
    detected_cat_count = 0

    for record in successful:
        truth = record["ground_truth"]
        prediction = record["prediction"]

        if prediction["is_cat"] == truth["is_cat"]:
            species_correct += 1

        if truth["is_cat"]:
            actual_cats += 1
            if prediction["is_cat"]:
                actual_cats_predicted_as_cats += 1
                detected_cat_count += 1
                if normalize_name(prediction["breed"] or "") == normalize_name(
                    truth["breed"]
                ):
                    cat_breed_correct += 1
                    detected_cat_breed_correct += 1

    summary.update(
        {
            "species_accuracy_all_images": round(
                species_correct / len(successful), 6
            ),
            "cat_recall": round(
                actual_cats_predicted_as_cats / actual_cats, 6
            )
            if actual_cats
            else None,
            "cat_breed_accuracy_among_actual_cats": round(
                cat_breed_correct / actual_cats, 6
            )
            if actual_cats
            else None,
            "cat_breed_accuracy_when_predicted_cat": round(
                detected_cat_breed_correct / detected_cat_count, 6
            )
            if detected_cat_count
            else None,
            "successful_actual_cat_images": actual_cats,
        }
    )
    return summary


def save_jsonl(path: Path, results: dict[str, dict[str, Any]]) -> None:
    with path.open("w", encoding="utf-8") as file:
        for image_name in sorted(results):
            file.write(json.dumps(results[image_name], ensure_ascii=False) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description="评测 Oxford-IIIT Pet 视觉模型 API")
    parser.add_argument("--images", type=Path, default=DEFAULT_IMAGE_DIR)
    parser.add_argument("--list", type=Path, default=DEFAULT_ANNOTATIONS_LIST)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    parser.add_argument("--limit", type=int, default=None, help="只处理前 N 张图片")
    parser.add_argument(
        "--api-url",
        default=os.getenv("MODEL_API_URL"),
        help="OpenAI-compatible API 完整地址",
    )
    parser.add_argument(
        "--model",
        default=os.getenv("MODEL_NAME"),
        help="支持视觉输入的模型名称",
    )
    parser.add_argument(
        "--timeout", type=int, default=120, help="单张图片请求超时时间（秒）"
    )
    parser.add_argument(
        "--retries", type=int, default=2, help="失败后的重试次数"
    )
    args = parser.parse_args()

    api_key = os.getenv("MODEL_API_KEY")
    if not args.api_url or not args.model or not api_key:
        raise SystemExit(
            "请设置 MODEL_API_URL、MODEL_NAME、MODEL_API_KEY 三个环境变量。"
        )

    if not args.images.exists():
        raise SystemExit(f"图片目录不存在：{args.images}")

    labels = load_ground_truth(args.list)
    image_paths = sorted(
        path
        for path in args.images.iterdir()
        if path.is_file() and path.suffix.casefold() in {".jpg", ".jpeg", ".png", ".webp"}
    )
    if args.limit is not None:
        image_paths = image_paths[: args.limit]

    existing = load_existing_results(args.output)
    print(f"图片数量：{len(image_paths)}")
    print(f"已存在结果：{sum(record.get('success') for record in existing.values())}")

    for index, image_path in enumerate(image_paths, start=1):
        image_name = image_path.name
        if image_name in existing and existing[image_name].get("success"):
            print(f"[{index}/{len(image_paths)}] 跳过已完成：{image_name}")
            continue

        stem = image_path.stem
        truth = labels.get(stem)
        if truth is None:
            truth = {
                "is_cat": infer_species_from_stem(stem) == "cat",
                "breed": breed_from_stem(stem),
                "source": "filename_rule",
            }

        print(f"[{index}/{len(image_paths)}] 识别：{image_name}")
        record: dict[str, Any] = {
            "image": image_name,
            "ground_truth": truth,
            "success": False,
            "prediction": None,
            "raw_output": None,
            "error": None,
        }

        try:
            prediction, raw_output = call_model(
                image_path=image_path,
                api_url=args.api_url,
                api_key=api_key,
                model=args.model,
                timeout=args.timeout,
                max_retries=args.retries,
            )
            record["success"] = True
            record["prediction"] = prediction
            record["raw_output"] = raw_output
        except Exception as error:  # noqa: BLE001 - 单张失败不影响后续图片
            record["error"] = str(error)
            print(f"  失败：{error}")

        existing[image_name] = record
        save_jsonl(args.output, existing)
        summary = calculate_summary(list(existing.values()))
        args.summary.write_text(
            json.dumps(summary, ensure_ascii=False, indent=2),
            encoding="utf-8",
        )
        time.sleep(1)

    summary = calculate_summary(list(existing.values()))
    args.summary.write_text(
        json.dumps(summary, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    print(f"详细结果：{args.output}")
    print(f"统计结果：{args.summary}")


if __name__ == "__main__":
    main()
