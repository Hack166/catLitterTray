#!/usr/bin/env python3
"""将两个全量视觉评测 JSONL 生成包含逐图结果的 Markdown 报告。"""

from __future__ import annotations

import argparse
import json
import statistics
from collections import Counter, defaultdict
from datetime import date
from pathlib import Path
from typing import Any


def load_latest(path: Path) -> dict[str, dict[str, Any]]:
    records: dict[str, dict[str, Any]] = {}
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        try:
            record = json.loads(line)
            records[record["image"]] = record
        except (json.JSONDecodeError, KeyError) as error:
            raise ValueError(f"{path}:{line_number} 无效记录：{error}") from error
    return records


def p95(values: list[float]) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    index = max(0, int(len(ordered) * 0.95 + 0.999) - 1)
    return ordered[index]


def canonical(value: Any) -> str:
    return str(value or "").strip().replace("_", " ").casefold()


def is_valid(record: dict[str, Any], field: str) -> bool:
    return record.get("prediction") is not None and field not in record.get(
        "field_errors", {}
    )


def is_correct(record: dict[str, Any], field: str) -> bool:
    if not is_valid(record, field):
        return False
    truth = record["ground_truth"][field]
    prediction = record["prediction"][field]
    if field == "cat_breed":
        return canonical(truth) == canonical(prediction)
    return truth == prediction


def model_summary(
    records: dict[str, dict[str, Any]], input_price: float, output_price: float
) -> dict[str, Any]:
    rows = list(records.values())
    cats = [row for row in rows if row["ground_truth"]["has_cat"] is True]
    latency = [float(row["latency_ms"]) for row in rows if row.get("latency_ms")]
    prompt_tokens = sum(int(row.get("usage", {}).get("prompt_tokens", 0)) for row in rows)
    completion_tokens = sum(
        int(row.get("usage", {}).get("completion_tokens", 0)) for row in rows
    )
    return {
        "model": next((row.get("model") for row in rows), "unknown"),
        "total": len(rows),
        "http_ok": sum(bool(row.get("http_success")) for row in rows),
        "json_ok": sum(row.get("prediction") is not None for row in rows),
        "cat_correct": sum(is_correct(row, "has_cat") for row in rows),
        "breed_total": len(cats),
        "breed_correct": sum(is_correct(row, "cat_breed") for row in cats),
        "mean_latency": statistics.fmean(latency) if latency else None,
        "p95_latency": p95(latency),
        "prompt_tokens": prompt_tokens,
        "completion_tokens": completion_tokens,
        "cost": prompt_tokens * input_price / 1_000_000
        + completion_tokens * output_price / 1_000_000,
        "unknown_breed": sum(
            (row.get("prediction") or {}).get("cat_breed") == "unknown"
            for row in cats
        ),
    }


def percent(correct: int, total: int) -> str:
    return f"{correct}/{total}（{correct / total:.2%}）" if total else "—"


def seconds(value: float | None) -> str:
    return f"{value / 1000:.2f}s" if value is not None else "—"


def cat_text(value: Any) -> str:
    if value is True:
        return "有猫"
    if value is False:
        return "无猫"
    return "ERROR"


def waste_text(value: Any) -> str:
    if value is True:
        return "有"
    if value is False:
        return "无"
    return "ERROR"


def predicted(record: dict[str, Any], field: str) -> Any:
    prediction = record.get("prediction")
    return prediction.get(field) if prediction else None


def marked_cat(record: dict[str, Any]) -> str:
    value = predicted(record, "has_cat")
    return f"{cat_text(value)} {'✓' if is_correct(record, 'has_cat') else '✗'}"


def marked_breed(record: dict[str, Any]) -> str:
    value = predicted(record, "cat_breed")
    shown = "null" if value is None else str(value)
    if record["ground_truth"]["cat_breed"] is None:
        return shown
    return f"{shown} {'✓' if is_correct(record, 'cat_breed') else '✗'}"


def breed_rows(
    images: list[str], first: dict[str, dict[str, Any]], second: dict[str, dict[str, Any]]
) -> list[str]:
    grouped: dict[str, list[str]] = defaultdict(list)
    for image in images:
        breed = first[image]["ground_truth"]["cat_breed"]
        if breed:
            grouped[breed].append(image)
    rows = []
    for breed in sorted(grouped):
        members = grouped[breed]
        a = sum(is_correct(first[image], "cat_breed") for image in members)
        b = sum(is_correct(second[image], "cat_breed") for image in members)
        rows.append(f"| {breed} | {len(members)} | {a / len(members):.2%} | {b / len(members):.2%} |")
    return rows


def error_summary(records: dict[str, dict[str, Any]]) -> str:
    errors = Counter(
        (row.get("error") or "未知错误").replace("\n", " ")[:160]
        for row in records.values()
        if row.get("prediction") is None
    )
    if not errors:
        return "无"
    return "；".join(f"{message} × {count}" for message, count in errors.most_common(5))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--first", type=Path, required=True)
    parser.add_argument("--second", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--first-input-price", type=float, required=True)
    parser.add_argument("--first-output-price", type=float, required=True)
    parser.add_argument("--second-input-price", type=float, required=True)
    parser.add_argument("--second-output-price", type=float, required=True)
    args = parser.parse_args()

    first = load_latest(args.first)
    second = load_latest(args.second)
    if set(first) != set(second):
        only_first = len(set(first) - set(second))
        only_second = len(set(second) - set(first))
        raise SystemExit(
            f"两个结果集图片不一致：仅first={only_first}，仅second={only_second}"
        )
    images = sorted(first, key=lambda value: Path(value).name.casefold())
    if not images:
        raise SystemExit("没有结果")

    first_summary = model_summary(first, args.first_input_price, args.first_output_price)
    second_summary = model_summary(
        second, args.second_input_price, args.second_output_price
    )
    first_name = first_summary["model"]
    second_name = second_summary["model"]

    lines = [
        "# Oxford-IIIT Pet 全量视觉模型评测报告",
        "",
        f"生成日期：{date.today().isoformat()}  ",
        f"评测图片：{len(images):,} 张  ",
        f"模型：`{first_name}`、`{second_name}`",
        "",
        "## 1. 结论摘要",
        "",
        f"| 模型 | 是否有猫 | 猫品种 | HTTP成功 | JSON成功 | 平均延迟 | P95延迟 | 输入Token | 输出Token | 估算费用 |",
        "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for summary in (first_summary, second_summary):
        lines.append(
            f"| `{summary['model']}` | {percent(summary['cat_correct'], summary['total'])} "
            f"| {percent(summary['breed_correct'], summary['breed_total'])} "
            f"| {summary['http_ok']}/{summary['total']} | {summary['json_ok']}/{summary['total']} "
            f"| {seconds(summary['mean_latency'])} | {seconds(summary['p95_latency'])} "
            f"| {summary['prompt_tokens']:,} | {summary['completion_tokens']:,} "
            f"| ¥{summary['cost']:.2f} |"
        )

    breed_gap = (
        second_summary["breed_correct"] / second_summary["breed_total"]
        - first_summary["breed_correct"] / first_summary["breed_total"]
    )
    cost_saving = 1 - second_summary["cost"] / first_summary["cost"]
    dog_total = first_summary["total"] - first_summary["breed_total"]

    lines.extend(
        [
            "",
            f"- 猫/狗二分类：`{first_name}` 更高，为 {first_summary['cat_correct'] / first_summary['total']:.2%}；`{second_name}` 为 {second_summary['cat_correct'] / second_summary['total']:.2%}（API 失败按错计）。",
            f"- 猫品种：`{second_name}` 更高，为 {second_summary['breed_correct'] / second_summary['breed_total']:.2%}，领先 {breed_gap:.2%}；`{first_name}` 为 {first_summary['breed_correct'] / first_summary['breed_total']:.2%}。",
            f"- 速度与稳定性：`{first_name}` 平均 {seconds(first_summary['mean_latency'])}，且 {first_summary['json_ok']:,}/{first_summary['total']:,} 张均返回有效 JSON；`{second_name}` 平均 {seconds(second_summary['mean_latency'])}，有 {second_summary['total'] - second_summary['json_ok']} 张接口失败。",
            f"- 成本：本次估算 `{second_name}` ¥{second_summary['cost']:.2f}，比 `{first_name}` ¥{first_summary['cost']:.2f} 低约 {cost_saving:.1%}。",
            f"- 选型建议：优先保证‘是否有猫’、低延迟和接口成功率时选 `{first_name}`；更看重猫品种准确率和调用成本时选 `{second_name}`。生产环境仍应先用真实猫砂盆图片复测。",
            "",
            "## 2. 评测方法与口径",
            "",
            f"- 数据：Oxford-IIIT Pet 官方标注中可匹配的 {first_summary['total']:,} 张图片，包括 {first_summary['breed_total']:,} 张猫、{dog_total:,} 张狗。",
            "- 输入：两个模型使用相同图片、相同提示词和严格 JSON 输出结构，一次请求同时预测 `has_cat`、`has_cat_waste`、`cat_breed`。",
            "- 猫检测：全体图片计分；猫品种：只在标准答案为猫的图片上计分，品种名忽略大小写并将下划线视为空格。",
            "- API/格式失败按错误计入总体准确率；延迟和 Token 仅统计服务返回的数据。",
            "- GLM 全量测试型号为 `glm-4.6v`。此前小样本使用的 `glm-5v-turbo` 不参与本报告统计。",
            "- 费用依据配置中的公开按量单价估算，不包含免费额度、限时优惠和缓存折扣。参考：[阿里云百炼模型价格](https://help.aliyun.com/zh/model-studio/model-pricing)、[智谱开放平台价格](https://bigmodel.cn/pricing)。",
            "",
            "## 3. 数据限制",
            "",
            "Oxford 数据只有猫/狗和品种真值，没有猫砂盆排泄物真值；逐图表会保留排泄物预测，但不计算其准确率。该数据以宠物主体照片为主，不能代表夜视、遮挡、脏污猫砂、强反光等真实猫砂盆场景。",
            "",
            "该公开数据可能出现在模型训练语料中，因此结果适合做第一轮筛选，不应作为最终生产准确率。排泄物能力需要另建带 `有/无/类型/遮挡程度` 真值的真实场景测试集。",
            "",
            "## 4. 各猫品种准确率",
            "",
            f"| 品种 | 图片数 | `{first_name}` | `{second_name}` |",
            "|---|---:|---:|---:|",
            *breed_rows(images, first, second),
            "",
            "## 5. 运行质量",
            "",
            f"- `{first_name}` 格式/API错误：{error_summary(first)}；猫品种输出 `unknown`：{first_summary['unknown_breed']} 张。",
            f"- `{second_name}` 格式/API错误：{error_summary(second)}；猫品种输出 `unknown`：{second_summary['unknown_breed']} 张。",
            "",
            "## 6. 每张图片的完整结果",
            "",
            "`✓/✗` 表示有标准答案字段的对错；排泄物列仅记录输出。图片链接指向本地 Oxford 数据集。",
            "",
            f"| 图片 | 标准猫标签 | 标准品种 | `{first_name}`猫 | `{first_name}`品种 | `{first_name}`排泄物 | 延迟 | `{second_name}`猫 | `{second_name}`品种 | `{second_name}`排泄物 | 延迟 |",
            "|---|---|---|---|---|---|---:|---|---|---|---:|",
        ]
    )
    for image in images:
        a = first[image]
        b = second[image]
        truth = a["ground_truth"]
        filename = Path(image).name
        truth_breed = truth["cat_breed"] or "—"
        lines.append(
            f"| [`{filename}`](../datasets/oxford/images/{filename}) "
            f"| {cat_text(truth['has_cat'])} | {truth_breed} "
            f"| {marked_cat(a)} | {marked_breed(a)} "
            f"| {waste_text(predicted(a, 'has_cat_waste'))} | {seconds(a.get('latency_ms'))} "
            f"| {marked_cat(b)} | {marked_breed(b)} "
            f"| {waste_text(predicted(b, 'has_cat_waste'))} | {seconds(b.get('latency_ms'))} |"
        )

    lines.extend(
        [
            "",
            "## 7. 原始结果",
            "",
            f"- `{first_name}`：`{args.first}`",
            f"- `{second_name}`：`{args.second}`",
            "",
        ]
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines), encoding="utf-8")
    print(f"报告已生成：{args.output}（逐图 {len(images):,} 张）")


if __name__ == "__main__":
    main()
