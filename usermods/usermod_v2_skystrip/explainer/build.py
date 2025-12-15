#!/usr/bin/env python3
"""
Build a print-friendly HTML explainer from the single source-of-truth FAQ.md.

Usage:
  python3 build.py

Reads:
  ../FAQ.md
  template.html

Writes:
  explainer.html

No external dependencies (stdlib only). This supports a small subset of
Markdown used by the SkyStrip FAQ: headings, paragraphs, unordered/ordered
lists, fenced code blocks, inline code/strong emphasis, and simple tables.
"""
from __future__ import annotations

from dataclasses import dataclass
from html import escape
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent
FAQ_MD = (ROOT / ".." / "FAQ.md").resolve()
TEMPLATE = ROOT / "template.html"
OUT = ROOT / "explainer.html"

HEADING_RE = re.compile(r"^(#{1,6})\s+(.+?)\s*$")
UL_ITEM_RE = re.compile(r"^\s*[-*]\s+(.+?)\s*$")
OL_ITEM_RE = re.compile(r"^\s*(\d+)\.\s+(.+?)\s*$")
TABLE_SEPARATOR_RE = re.compile(
    r"^\s*\|?\s*:?-{3,}:?\s*(\|\s*:?-{3,}:?\s*)+\|?\s*$"
)

SPLIT_SECTIONS = {
    "Wind View (WV)",
    "Temperature View (TV)",
    "24-Hour Delta View (DV)",
    "24 Hour Delta View (DV)",
}


def render_inline(text: str) -> str:
    s = escape(text)
    s = re.sub(r"`([^`]+)`", r"<code>\1</code>", s)
    s = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", s)
    return s


def parse_table_row(line: str) -> list[str]:
    raw = line.strip()
    if raw.startswith("|"):
        raw = raw[1:]
    if raw.endswith("|"):
        raw = raw[:-1]
    return [cell.strip() for cell in raw.split("|")]


def render_table(header: list[str], rows: list[list[str]]) -> str:
    cols = max([len(header)] + [len(r) for r in rows]) if (header or rows) else 0
    header_cells = header + [""] * (cols - len(header))
    rendered_header = "\n".join(f"<th>{render_inline(c)}</th>" for c in header_cells)

    rendered_rows = []
    for row in rows:
        row_cells = row + [""] * (cols - len(row))
        rendered_rows.append(
            "<tr>"
            + "".join(f"<td>{render_inline(c)}</td>" for c in row_cells)
            + "</tr>"
        )

    return (
        "<table>"
        "<thead><tr>"
        + rendered_header
        + "</tr></thead>"
        "<tbody>"
        + "\n".join(rendered_rows)
        + "</tbody></table>"
    )


@dataclass(frozen=True)
class RenderResult:
    title: str
    html: str


@dataclass(frozen=True)
class Block:
    kind: str
    html: str
    level: int | None = None
    text: str | None = None


def blocks_to_html(blocks: list[Block]) -> str:
    out: list[str] = []

    preamble: list[str] = []
    current_h2: Block | None = None
    section_blocks: list[Block] = []

    def flush_section() -> None:
        nonlocal current_h2, section_blocks, preamble
        if current_h2 is None:
            return

        title = current_h2.text or ""
        body_html = "\n".join(b.html for b in section_blocks)
        section_classes = ["section"]
        if title:
            slug = re.sub(r"[^a-z0-9]+", "-", title.lower()).strip("-")
            section_classes.append(f"section-{slug}")
        if title == "Wind View (WV)":
            section_classes.append("page-break-after")

        if title in SPLIT_SECTIONS:
            left = "\n".join(b.html for b in section_blocks if b.kind != "table")
            right = "\n".join(b.html for b in section_blocks if b.kind == "table")
            if right.strip():
                body_html = (
                    '<div class="split-grid">'
                    f'<div class="split-left">{left}</div>'
                    f'<div class="split-right">{right}</div>'
                    "</div>"
                )

        out.append(
            f'<section class="{" ".join(section_classes)}">{current_h2.html}{body_html}</section>'
        )
        current_h2 = None
        section_blocks = []

    for b in blocks:
        if b.kind == "heading" and b.level == 2:
            if current_h2 is None:
                out.extend(preamble)
                preamble = []
            else:
                flush_section()
            current_h2 = b
            continue

        if current_h2 is None:
            preamble.append(b.html)
        else:
            section_blocks.append(b)

    if current_h2 is None:
        out.extend(preamble)
    else:
        flush_section()

    return "\n".join(out)


def render_markdown(md_text: str) -> RenderResult:
    lines = md_text.splitlines()
    i = 0
    title: str | None = None
    blocks: list[Block] = []

    def is_table_start(idx: int) -> bool:
        if idx + 1 >= len(lines):
            return False
        a = lines[idx]
        b = lines[idx + 1]
        return "|" in a and TABLE_SEPARATOR_RE.match(b.strip()) is not None

    while i < len(lines):
        line = lines[i].rstrip("\n")
        if not line.strip():
            i += 1
            continue

        if line.strip().startswith("```"):
            i += 1
            buf = []
            while i < len(lines) and not lines[i].strip().startswith("```"):
                buf.append(lines[i].rstrip("\n"))
                i += 1
            if i < len(lines):
                i += 1
            blocks.append(
                Block(
                    kind="pre",
                    html=f"<pre><code>{escape(chr(10).join(buf))}</code></pre>",
                )
            )
            continue

        m = HEADING_RE.match(line)
        if m:
            level = len(m.group(1))
            text = m.group(2).strip()
            if level == 1 and title is None:
                title = text
                i += 1
                continue
            blocks.append(
                Block(
                    kind="heading",
                    level=level,
                    text=text,
                    html=f"<h{level}>{render_inline(text)}</h{level}>",
                )
            )
            i += 1
            continue

        if is_table_start(i):
            header = parse_table_row(lines[i])
            i += 2
            rows: list[list[str]] = []
            while i < len(lines) and lines[i].strip() and "|" in lines[i]:
                if HEADING_RE.match(lines[i]) or UL_ITEM_RE.match(lines[i]) or OL_ITEM_RE.match(lines[i]):
                    break
                rows.append(parse_table_row(lines[i]))
                i += 1
            blocks.append(Block(kind="table", html=render_table(header, rows)))
            continue

        ul = UL_ITEM_RE.match(line)
        if ul:
            items: list[str] = []
            while i < len(lines):
                if not lines[i].strip():
                    j = i + 1
                    while j < len(lines) and not lines[j].strip():
                        j += 1
                    if j < len(lines) and UL_ITEM_RE.match(lines[j]):
                        i = j
                        continue
                    break

                mm = UL_ITEM_RE.match(lines[i])
                if mm:
                    items.append(mm.group(1).strip())
                    i += 1
                    continue

                if (lines[i].startswith("  ") or lines[i].startswith("\t")) and items:
                    items[-1] = f"{items[-1]} {lines[i].strip()}"
                    i += 1
                    continue

                break

            blocks.append(
                Block(
                    kind="list",
                    html="<ul>"
                    + "".join(f"<li>{render_inline(it)}</li>" for it in items)
                    + "</ul>",
                )
            )
            continue

        ol = OL_ITEM_RE.match(line)
        if ol:
            items: list[str] = []
            while i < len(lines):
                if not lines[i].strip():
                    j = i + 1
                    while j < len(lines) and not lines[j].strip():
                        j += 1
                    if j < len(lines) and OL_ITEM_RE.match(lines[j]):
                        i = j
                        continue
                    break

                mm = OL_ITEM_RE.match(lines[i])
                if mm:
                    items.append(mm.group(2).strip())
                    i += 1
                    continue

                if (lines[i].startswith("  ") or lines[i].startswith("\t")) and items:
                    items[-1] = f"{items[-1]} {lines[i].strip()}"
                    i += 1
                    continue

                break

            blocks.append(
                Block(
                    kind="list",
                    html="<ol>"
                    + "".join(f"<li>{render_inline(it)}</li>" for it in items)
                    + "</ol>",
                )
            )
            continue

        # Paragraph: accumulate until blank line or another block begins.
        para_lines = [line.strip()]
        i += 1
        while i < len(lines) and lines[i].strip():
            nxt = lines[i].rstrip("\n")
            if HEADING_RE.match(nxt) or UL_ITEM_RE.match(nxt) or OL_ITEM_RE.match(nxt) or is_table_start(i) or nxt.strip().startswith("```"):
                break
            para_lines.append(nxt.strip())
            i += 1
        blocks.append(
            Block(kind="p", html=f"<p>{render_inline(' '.join(para_lines))}</p>")
        )

    if title is None:
        title = "SkyStrip"
    return RenderResult(title=title, html=blocks_to_html(blocks))


def main() -> int:
    md = FAQ_MD.read_text(encoding="utf-8")
    rendered = render_markdown(md)
    template = TEMPLATE.read_text(encoding="utf-8")
    html = template.replace("{{TITLE}}", escape(rendered.title))
    html = html.replace("{{CONTENT}}", rendered.html)
    OUT.write_text(html, encoding="utf-8")
    print(f"Wrote {OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
