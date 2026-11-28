#!/usr/bin/env python3
"""
sync-grid-layouts.py — unify the top-level "mainGrid" QGridLayout across the
appearance-panel child widgets.

The canonical column scheme lives in COLUMN_ATTRS / PROPS below — edit it here,
then run:

    buildtools/sync-grid-layouts.py

For every .ui file in UI_FILES the script finds the top-level
    <layout class="QGridLayout" name="mainGrid" ...>
element and rewrites its columnstretch / columnminimumwidth attributes and the
spacing/margin <property> numbers. Intentional per-column divergences belong
in COLUMN_OVERRIDES — local edits to these attributes get reverted on sync.
Files without a matching mainGrid are reported and skipped.
"""

import re
import sys
from pathlib import Path

UI_DIR = Path(__file__).resolve().parent.parent / "src" / "qt" / "ui"

UI_FILES = [
    "style-panel.ui",
    "appearance-panel.ui",
    "size-widget.ui",
    "label-widget.ui",
    "rectangle-widget.ui",
    "ellipse-widget.ui",
    "star-widget.ui",
    "text-panel.ui",
    "typography-panel.ui",
    "image-widget.ui",
    "export-widget.ui",
    "filter-widget.ui",
    "lpe-widget.ui",
    "page-widget.ui",
    "path-widget.ui",
]

# Canonical 7-column scheme:
# - col 0 = 15px gutter (and collapsible section header)
# - cols 1 & 3 = stretch content
# - cols 2 & 4 = 4px gaps
# - col 5 = button (25px)
# - col 6 = 10px gap for overlay scrollbar
COLUMN_ATTRS = {
    "columnstretch": "0,1,0,1,0,0,0",
    "columnminimumwidth": "15,0,4,0,4,25,10",
}
PROPS = {
    "horizontalSpacing": 0,
    "verticalSpacing": 4,
    "leftMargin": 0,
    "topMargin": 0,
    "rightMargin": 0,
    "bottomMargin": 0,
}
PROP_ORDER = ["horizontalSpacing", "verticalSpacing",
              "leftMargin", "topMargin", "rightMargin", "bottomMargin"]

# Deliberate per-file divergences: {file: {column attr: {column index: width}}}.
COLUMN_OVERRIDES = {
}

LAYOUT_RE = re.compile(r'<layout class="QGridLayout" name="mainGrid"[^>]*>')
COMMENT_RE = re.compile(r'<!--.*?-->', re.S)
PROP_BLOCK = '<property name="{p}">\\s*<number>-?\\d+</number>\\s*</property>'


def _real_matches(pattern, text):
    """Regex matches that are not inside an XML comment."""
    comments = [(m.start(), m.end()) for m in COMMENT_RE.finditer(text)]
    return [m for m in re.finditer(pattern, text)
            if not any(a <= m.start() < b for a, b in comments)]


def _header_end(s, start):
    """End of the layout header: first uncommented <item or </layout>."""
    for m in _real_matches(r'<item|</layout', s[start:]):
        return start + m.start()
    return len(s)


def _indent_of(text, pos):
    return text[text.rfind('\n', 0, pos) + 1:pos]


def _prop_block(name, value, indent):
    # starts at <property — the caller supplies the first line's indent
    return (f'<property name="{name}">\n'
            f'{indent}  <number>{value}</number>\n'
            f'{indent}</property>')


def patch_file(path):
    s = path.read_text()
    m = LAYOUT_RE.search(s)
    if not m:
        return "SKIP: no mainGrid QGridLayout"

    attrs = dict(COLUMN_ATTRS)
    for attr, cols in COLUMN_OVERRIDES.get(path.name, {}).items():
        parts = attrs[attr].split(',')
        for i, v in cols.items():
            parts[i] = str(v)
        attrs[attr] = ','.join(parts)

    tag = m.group(0)
    new_tag = tag
    for a, v in attrs.items():
        if re.search(rf'{a}="', new_tag):
            new_tag = re.sub(rf'{a}="[^"]*"', f'{a}="{v}"', new_tag)
        else:
            new_tag = new_tag[:-1] + f' {a}="{v}">'

    out = s[:m.start()] + new_tag + s[m.end():]

    # Layout header = between the tag and the first uncommented <item>/</layout>.
    start = m.start() + len(new_tag)
    end = _header_end(out, start)
    seg = out[start:end]

    # Shorthand `spacing` is replaced in place by the explicit h/v pair —
    # leaving it would fight the synced values depending on property order.
    present = set()
    for mm in _real_matches(PROP_BLOCK.format(p='spacing'), seg):
        ind = _indent_of(seg, mm.start())
        repl = (_prop_block('horizontalSpacing', PROPS['horizontalSpacing'], ind)
                + '\n' + ind
                + _prop_block('verticalSpacing', PROPS['verticalSpacing'], ind))
        seg = seg[:mm.start()] + repl + seg[mm.end():]
        present.update(('horizontalSpacing', 'verticalSpacing'))
        break

    # Update canonical properties already present.
    for p in PROP_ORDER:
        pat = PROP_BLOCK.format(p=p)
        for mm in reversed(_real_matches(pat, seg)):
            block = re.sub(r'(<number>)-?\d+', rf'\g<1>{PROPS[p]}', mm.group(0))
            seg = seg[:mm.start()] + block + seg[mm.end():]
            present.add(p)

    # Insert missing properties right after the last real property so the
    # row comments below stay attached to their items.
    missing = [p for p in PROP_ORDER if p not in present]
    if missing:
        real = _real_matches(r'</property>', seg)
        if real:
            ins = real[-1].end()
            ind = _indent_of(seg, _real_matches(r'<property', seg)[-1].start())
        else:
            ins, ind = 0, "      "
        block = ('\n' + ind).join(_prop_block(p, PROPS[p], ind) for p in missing)
        seg = seg[:ins] + '\n' + ind + block + seg[ins:]
    out = out[:start] + seg + out[end:]

    if out == s:
        return "ok (already in sync)"
    path.write_text(out)
    return "updated"


def main():
    print(f"canonical: columnstretch={COLUMN_ATTRS['columnstretch']!r} "
          f"columnminimumwidth={COLUMN_ATTRS['columnminimumwidth']!r} {PROPS}")
    rc = 0
    for name in UI_FILES:
        status = patch_file(UI_DIR / name)
        print(f"{name}: {status}")
        if status.startswith("SKIP"):
            rc = 1
    return rc


if __name__ == "__main__":
    sys.exit(main())
