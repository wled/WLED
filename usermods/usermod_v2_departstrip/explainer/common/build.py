#!/usr/bin/env python3
"""
Builds a two-page, half-letter explainer for a given installation name.

Usage:
    python3 common/build.py <install_name>

Reads:
    installs/<install_name>/legend.json  (per-install lines/colors or direction/stops)
    common/faq.md                        (shared FAQ; optional — falls back to defaults)
    common/template*.html                (shared HTML with placeholders)

Writes:
    installs/<install_name>/explainer.html

No external dependencies (stdlib only).
"""
from __future__ import annotations
import sys
import json
import re
from pathlib import Path
from datetime import date

ROOT = Path(__file__).resolve().parents[1]
COMMON = ROOT / "common"
INSTALLS = ROOT / "installs"

TEMPLATE_PLACEHOLDERS = {
    "legend": "{{LEGEND_ITEMS}}",
    "groups": "{{LEGEND_GROUPS}}",
    "faq": "{{FAQ_BLOCKS}}",
    "title": "{{TITLE}}",
    "subtitle": "{{SUBTITLE}}",
    "updated": "{{UPDATED}}",
    "directions": "{{DIRECTIONS}}",
}

DEFAULT_TITLE = "DepartStrip"
DEFAULT_SUBTITLE = "Arrivals at a glance"

DEFAULT_FAQ_MD = """
### What am I looking at?
Each LED represents a scheduled arrival. Colors map to the legend. Dots move right as arrival time approaches.

### How recent is the data?
Continuously updated from official GTFS-Realtime/agency sources. If a feed drops, we fall back to last known schedule.

### What do blinking or dim lights mean?
Blinking = status change (delay/new estimate). Dimmer = farther out in time; brighter = imminent.

### Can I change which lines are shown?
Yes—use the config file or app to enable/disable routes and tweak colors.
""".strip()

FAQ_H3 = re.compile(r"^###\s+(.+)$", re.M)


def parse_faq(md_text: str) -> list[tuple[str, str]]:
    """Return list of (question, answer_html) parsed from markdown with H3 questions.
    Very light parser: groups paragraphs until next H3.
    """
    items: list[tuple[str, str]] = []
    positions = list(FAQ_H3.finditer(md_text))
    if not positions:
        md_text = DEFAULT_FAQ_MD
        positions = list(FAQ_H3.finditer(md_text))

    for i, m in enumerate(positions):
        q = m.group(1).strip()
        start = m.end()
        end = positions[i + 1].start() if i + 1 < len(positions) else len(md_text)
        body = md_text[start:end].strip()
        # Convert bare newlines to <br> within paragraphs, and blank lines to paragraph breaks
        paras = [p.strip() for p in re.split(r"\n\s*\n", body) if p.strip()]
        html_paras = []
        for p in paras:
            inner = re.sub(r"\n+", "<br>", p)
            html_paras.append(f"<p>{inner}</p>")
        a_html = "\n".join(html_paras) if html_paras else "<p></p>"
        items.append((q, a_html))
    return items


def load_legend(legend_path: Path) -> dict:
    with legend_path.open("r", encoding="utf-8") as f:
        data = json.load(f)
    # Basic validation
    has_lines = "lines" in data
    has_directions = "directions" in data
    if not has_lines and not has_directions:
        raise ValueError(f"Missing 'lines' or 'directions' in {legend_path}")
    if has_lines and not isinstance(data["lines"], list):
        raise ValueError(f"Missing 'lines' list in {legend_path}")
    if not has_lines:
        data["lines"] = []
    return data


def render_legend_items(lines: list[dict]) -> str:
    buf = []
    for item in lines:
        label = item.get("label", "")
        desc = item.get("desc", "")
        hex_color = item.get("hex", "#000000")
        buf.append(
            f"""
      <div class="legend-item" role="listitem">
        <div class="swatch" style="background:{hex_color}"></div>
        <div>
          <div class="label">{label}</div>
          <div class="desc">{desc}</div>
        </div>
      </div>"""
        )
    return "\n".join(buf)


def render_legend_groups(groups: list[dict], faq_html: str = "") -> str:
    sections = []
    for group in groups:
        title = group.get("title", "")
        lines = group.get("lines", [])
        classes = "legend-group"
        group_class = group.get("class")
        if group_class:
            classes += " " + group_class
        extra = ""
        if group.get("appendFaq") and faq_html:
            extra = f"""
        <div class="legend-spacer" aria-hidden="true"></div>
        <div class="faq faq-inline">
          <h3>Quick FAQ</h3>
{faq_html}
        </div>"""
        sections.append(
            f"""
      <section class="{classes}">
        <h3>{title}</h3>
        <div class="legend-list" role="list">
{render_legend_items(lines)}
        </div>{extra}
      </section>"""
        )
    return "\n".join(sections)


def render_routes(routes: list[dict]) -> str:
    buf = []
    for route in routes:
        label = route.get("label", "")
        desc = route.get("desc", "")
        hex_color = route.get("hex", "#000000")
        buf.append(
            f"""
        <div class="route" role="listitem">
          <div class="swatch" style="background:{hex_color}"></div>
          <div class="route-body">
            <div class="route-label">{label}</div>
            <div class="route-desc">{desc}</div>
          </div>
        </div>"""
        )
    return "\n".join(buf)


def render_stops(stops: list[dict]) -> str:
    cards = []
    for stop in stops:
        name = stop.get("name", "")
        stop_id = stop.get("id", "")
        routes = stop.get("routes", [])
        cards.append(
            f"""
      <article class="stop-card">
        <div class="stop-header">
          <div class="stop-name">{name}</div>
          <div class="stop-id">Stop {stop_id}</div>
        </div>
        <div class="route-list" role="list">
{render_routes(routes)}
        </div>
      </article>"""
        )
    return "\n".join(cards)


def render_directions(directions: list[dict]) -> str:
    sections = []
    for direction in directions:
        title = direction.get("title", "")
        stops = direction.get("stops", [])
        sections.append(
            f"""
    <section class="direction">
      <div class="direction-heading">
        <div class="direction-label">{title}</div>
      </div>
      <div class="stop-grid" role="list">
{render_stops(stops)}
      </div>
    </section>"""
        )
    return "\n".join(sections)


def render_faq_blocks(items: list[tuple[str, str]]) -> str:
    buf = []
    for q, a_html in items:
        buf.append(
            f"""
      <div class="qa">
        <div class="q">{q}</div>
        <div class="a">{a_html}</div>
      </div>"""
        )
    return "\n".join(buf)


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage: python3 common/build.py <install_name>")
        return 2
    name = sys.argv[1]

    legend_path = INSTALLS / name / "legend.json"

    if not legend_path.exists():
        raise SystemExit(f"Missing legend file: {legend_path}")

    legend = load_legend(legend_path)
    title = legend.get("title", DEFAULT_TITLE)
    subtitle = legend.get("subtitle", DEFAULT_SUBTITLE)

    template_name = legend.get("template", "template.html")
    template_path = COMMON / template_name
    if not template_path.exists():
        raise SystemExit(f"Missing template: {template_path}")

    faq_name = legend.get("faq")
    faq_md_path = COMMON / faq_name if faq_name else COMMON / "faq.md"
    faq_md = faq_md_path.read_text(encoding="utf-8") if faq_md_path.exists() else DEFAULT_FAQ_MD
    faq_items = parse_faq(faq_md)

    template = template_path.read_text(encoding="utf-8")
    html = template
    faq_html = render_faq_blocks(faq_items)
    lines = legend.get("lines", [])
    groups = legend.get("groups", [])
    directions = legend.get("directions", [])
    if groups:
        bart = next((g for g in groups if g.get("title") == "BART"), None)
        ac = next((g for g in groups if g.get("title") == "AC Transit"), None)
        html = html.replace("{{BART_LEGEND}}", render_legend_groups([bart], faq_html) if bart else "")
        html = html.replace("{{AC_LEGEND}}", render_legend_groups([ac], "") if ac else "")
    else:
        html = html.replace("{{BART_LEGEND}}", "")
        html = html.replace("{{AC_LEGEND}}", "")
    html = html.replace(TEMPLATE_PLACEHOLDERS["legend"], render_legend_items(lines))
    html = html.replace(TEMPLATE_PLACEHOLDERS["faq"], faq_html)
    html = html.replace(TEMPLATE_PLACEHOLDERS["directions"], render_directions(directions) if directions else "")
    html = html.replace(TEMPLATE_PLACEHOLDERS["title"], title)
    html = html.replace(TEMPLATE_PLACEHOLDERS["subtitle"], subtitle)

    out_path = INSTALLS / name / "explainer.html"
    out_path.write_text(html, encoding="utf-8")
    print(f"Wrote {out_path}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
