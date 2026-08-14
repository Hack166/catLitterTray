"""用同一批 Roboflow 图片，比较 GPT、豆包等视觉 API 的分类准确率。

数据目录应使用 Roboflow 的 Classification 导出格式，例如：

dataset/test/
├── normal/*.jpg
├── diare/*.jpg
├── lack-of-water/*.jpg
└── soft-poop/*.jpg

安装：
    pip install requests

示例：
    python benchmark_vision_apis.py \
      --data ./dataset/test \
      --api-url "你的API地址/v1/chat/completions" \
      --api-key "你的API_KEY" \
      --model "你的视觉模型名称" \
      --output gpt_results

结果：
    gpt_results.jsonl   每张图片的详细结果
    gpt_results.json    总体准确率、各类别准确率、混淆矩阵
"""

import argparse
import base64
import json
import mimetypes
import re
import time
from collections import defaultdict
from pathlib import Path

import requests


LABELS = {"normal", "diare", "lack-of-water", "soft-poop"}
IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".webp", ".bmp"}

PROMPT = """
你正在参加一个图片分类测试。请只根据图片中可见的粪便外观进行分类，
不要进行兽医诊断，也不要补充医学建议。

只能从下面四个英文标签中选择一个，并且必须原样输出：
- normal
- diare
- lack-of-water
- soft-poop

请只返回合法 JSON，不要使用 Markdown 代码块：
{"label":"normal"}
"""


def image_data_url(path: Path) -> str:
    mime_type, _ = mimetypes.guess_type(path.name)
    mime_type = mime_type or "image/jpeg"
    encoded = base64.b64encode(path.read_bytes()).decode("utf-8")
    return f"data:{mime_type};base64,{encoded}"


def find_images(data_dir: Path):
    return sorted(
        path
        for path in data_dir.rglob("*")
        if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS
    )


def expected_label(path: Path):
    for part in path.parts:
        if part.lower() in LABELS:
            return part.lower()
    return None


def parse_label(text: str):
    """优先解析 JSON；必要时兼容模型多输出的一点额外文字。"""
    cleaned = text.strip()
    cleaned = re.sub(r"^```(?:json)?\s*|\s*```$", "", cleaned, flags=re.I)

    try:
        value = json.loads(cleaned)
        label = str(value.get("label", "")).strip().lower()
        if label in LABELS:
            return label
    except (json.JSONDecodeError, AttributeError, TypeError):
        pass

    # 仅作为容错；提示词仍要求模型只输出 JSON。
    lower = cleaned.lower()
    for label in LABELS:
        if label in lower:
            return label
    return None


def request_model(api_url, api_key, model, path, timeout):
    payload = {
        "model": model,
        "temperature": 0,
        "max_tokens": 100,
        "messages": [
            {
                "role": "user",
                "content": [
                    {"type": "text", "text": PROMPT},
                    {
                        "type": "image_url",
                        "image_url": {"url": image_data_url(path)},
                    },
                ],
            }
        ],
    }
    response = requests.post(
        api_url,
        headers={
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json",
        },
        json=payload,
        timeout=timeout,
    )
    response.raise_for_status()
    body = response.json()
    text = body["choices"][0]["message"]["content"]
    if isinstance(text, list):
        text = "".join(
            part.get("text", "") for part in text if isinstance(part, dict)
        )
    return str(text), body


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", required=True, help="Roboflow test 图片目录")
    parser.add_argument("--api-url", required=True, help="兼容 Chat Completions 的 API 地址")
    parser.add_argument("--api-key", required=True, help="API Key；不要写进公开代码")
    parser.add_argument("--model", required=True, help="视觉模型名称")
    parser.add_argument("--output", default="vision_results", help="输出文件名前缀")
    parser.add_argument("--limit", type=int, default=0, help="只测试前 N 张，0 表示全部")
    parser.add_argument("--delay", type=float, default=1.0, help="请求间隔秒数")
    parser.add_argument("--timeout", type=int, default=120, help="单次请求超时秒数")
    args = parser.parse_args()

    data_dir = Path(args.data)
    images = find_images(data_dir)
    if args.limit > 0:
        images = images[:args.limit]
    if not images:
        raise RuntimeError("没有找到图片；请确认 --data 指向 test/ 文件夹")

    labeled_images = [(path, expected_label(path)) for path in images]
    unlabeled = [str(path) for path, label in labeled_images if label is None]
    if unlabeled:
        raise RuntimeError(
            "无法从文件夹名称读取标准答案。请使用 test/normal、test/diare 等目录结构。"
        )

    detail_path = Path(f"{args.output}.jsonl")
    summary_path = Path(f"{args.output}.json")
    total = 0
    correct = 0
    failed = 0
    per_class = defaultdict(lambda: {"total": 0, "correct": 0})
    confusion = defaultdict(lambda: defaultdict(int))

    with detail_path.open("w", encoding="utf-8") as output:
        for index, (path, expected) in enumerate(labeled_images, 1):
            print(f"[{index}/{len(labeled_images)}] {path}")
            record = {
                "image": str(path),
                "expected_label": expected,
                "predicted_label": None,
                "correct": False,
                "raw_response": None,
                "error": None,
            }

            for attempt in range(3):
                try:
                    text, raw_body = request_model(
                        args.api_url,
                        args.api_key,
                        args.model,
                        path,
                        args.timeout,
                    )
                    predicted = parse_label(text)
                    record["predicted_label"] = predicted
                    record["correct"] = predicted == expected
                    record["raw_response"] = text
                    record["provider_response"] = raw_body
                    break
                except Exception as error:
                    record["error"] = str(error)
                    if attempt < 2:
                        time.sleep(2 ** attempt)
                    else:
                        failed += 1

            total += 1
            per_class[expected]["total"] += 1
            if record["correct"]:
                correct += 1
                per_class[expected]["correct"] += 1
            confusion[expected][record["predicted_label"] or "ERROR"] += 1

            output.write(json.dumps(record, ensure_ascii=False) + "\n")
            output.flush()
            time.sleep(args.delay)

    summary = {
        "model": args.model,
        "api_url": args.api_url,
        "data_dir": str(data_dir),
        "total": total,
        "correct": correct,
        "failed_requests": failed,
        "accuracy": correct / total if total else 0,
        "per_class": {},
        "confusion_matrix": {label: dict(row) for label, row in confusion.items()},
    }
    for label, values in sorted(per_class.items()):
        summary["per_class"][label] = {
            **values,
            "accuracy": values["correct"] / values["total"]
            if values["total"]
            else 0,
        }

    summary_path.write_text(
        json.dumps(summary, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    print(f"\n总体准确率：{summary['accuracy']:.2%}")
    print(f"请求失败：{failed}")
    print(f"详细结果：{detail_path}")
    print(f"统计结果：{summary_path}")


if __name__ == "__main__":
    main()
