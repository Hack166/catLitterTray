#!/usr/bin/env python3
"""将视觉评测 Markdown 转为 Prince HTML/PDF。仅使用 Python 标准库。"""

from __future__ import annotations

import argparse
import html
import re
import subprocess
from pathlib import Path


LINK_RE = re.compile(r"\[([^\]]+)\]\(([^)]+)\)")
CODE_RE = re.compile(r"`([^`]+)`")


def inline(markdown: str) -> str:
    """转换本报告使用的链接和行内代码，同时转义其他 HTML。"""
    placeholders: list[str] = []

    def store(value: str) -> str:
        placeholders.append(value)
        return f"\x00{len(placeholders) - 1}\x00"

    def link(match: re.Match[str]) -> str:
        label = CODE_RE.sub(lambda item: html.escape(item.group(1)), match.group(1))
        href = html.escape(match.group(2), quote=True)
        return store(f'<a href="{href}">{label}</a>')

    value = LINK_RE.sub(link, markdown)
    value = CODE_RE.sub(
        lambda match: store(f"<code>{html.escape(match.group(1))}</code>"), value
    )
    value = html.escape(value)
    for index, placeholder in enumerate(placeholders):
        value = value.replace(f"\x00{index}\x00", placeholder)
    return value


def table_cells(line: str) -> list[str]:
    return [cell.strip() for cell in line.strip().strip("|").split("|")]


def is_separator(line: str) -> bool:
    cells = table_cells(line)
    return bool(cells) and all(re.fullmatch(r":?-{3,}:?", cell) for cell in cells)


def markdown_to_html(source: str, title: str, css_uri: str) -> str:
    lines = source.splitlines()
    output: list[str] = []
    index = 0
    list_open = False

    def close_list() -> None:
        nonlocal list_open
        if list_open:
            output.append("</ul>")
            list_open = False

    while index < len(lines):
        line = lines[index].rstrip()
        if not line:
            close_list()
            index += 1
            continue

        if line.startswith("# "):
            close_list()
            output.append(f"<h1>{inline(line[2:])}</h1>")
            index += 1
            continue
        if line.startswith("## "):
            close_list()
            output.append(f"<h2>{inline(line[3:])}</h2>")
            index += 1
            continue
        if line.startswith("### "):
            close_list()
            output.append(f"<h3>{inline(line[4:])}</h3>")
            index += 1
            continue

        if line.startswith("- "):
            if not list_open:
                output.append("<ul>")
                list_open = True
            output.append(f"<li>{inline(line[2:])}</li>")
            index += 1
            continue

        if (
            line.startswith("|")
            and index + 1 < len(lines)
            and is_separator(lines[index + 1])
        ):
            close_list()
            headers = table_cells(line)
            css_class = "wide-results" if headers and headers[0] == "图片" else ""
            class_attr = f' class="{css_class}"' if css_class else ""
            output.append(f"<table{class_attr}><thead><tr>")
            output.extend(f"<th>{inline(cell)}</th>" for cell in headers)
            output.append("</tr></thead><tbody>")
            index += 2
            while index < len(lines) and lines[index].lstrip().startswith("|"):
                cells = table_cells(lines[index])
                output.append("<tr>")
                output.extend(f"<td>{inline(cell)}</td>" for cell in cells)
                output.append("</tr>")
                index += 1
            output.append("</tbody></table>")
            continue

        close_list()
        paragraph = [line.strip()]
        index += 1
        while index < len(lines):
            following = lines[index].rstrip()
            if (
                not following
                or following.startswith(("# ", "## ", "### ", "- ", "|"))
            ):
                break
            paragraph.append(following.strip())
            index += 1
        output.append(f"<p>{inline(' '.join(paragraph))}</p>")

    close_list()
    body = "\n".join(output)
    return f"""<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <title>{html.escape(title)}</title>
  <link rel="stylesheet" href="{html.escape(css_uri, quote=True)}">
</head>
<body>
{body}
</body>
</html>
"""


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("markdown", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument(
        "--css",
        type=Path,
        default=Path("docs/prince-vision-report.css"),
    )
    parser.add_argument(
        "--html-output",
        type=Path,
        default=Path("tmp/pdfs/vision-benchmark-full.html"),
    )
    args = parser.parse_args()

    markdown_path = args.markdown.resolve()
    css_path = args.css.resolve()
    html_path = args.html_output.resolve()
    output_path = args.output.resolve()
    html_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    document = markdown_to_html(
        markdown_path.read_text(encoding="utf-8"),
        markdown_path.stem,
        css_path.as_uri(),
    )
    html_path.write_text(document, encoding="utf-8")
    subprocess.run(
        ["prince", str(html_path), "--output", str(output_path)],
        check=True,
        cwd=markdown_path.parent,
    )
    print(f"HTML: {html_path}")
    print(f"PDF: {output_path}")


if __name__ == "__main__":
    main()
