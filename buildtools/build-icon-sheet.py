#!/usr/bin/env python3
import os, sys

iconsize = 16
icongap = 18
resolution = 900
columns = 34

SPATH = os.path.dirname(os.path.realpath(__file__))
folder = "/Users/mike/dev/linea/share/icons/Dash/symbolic/actions"
template_path = os.path.join(SPATH, "theme-icons-template.svg")
output_dir = os.path.realpath(os.path.join(SPATH, "../build"))
output_path = os.path.join(output_dir, "dash-icon-sheet.svg")

if not os.path.isdir(folder):
    print("Icon folder not found:", folder, file=sys.stderr)
    sys.exit(1)
if not os.path.isfile(template_path):
    print("Template not found:", template_path, file=sys.stderr)
    sys.exit(1)

with open(template_path, "r") as f:
    svg = f.read()

img = ""
counter = -1
y = -icongap - iconsize / 2

for filename in sorted(os.listdir(folder)):
    if not filename.endswith(".svg"):
        continue
    fullfile = os.path.join(folder, filename)
    counter += 1
    if counter % columns == 0:
        y += iconsize + icongap
    x = iconsize / 2 + (counter % columns) * (iconsize + icongap)
    title = os.path.basename(filename)
    if title.endswith("-symbolic.svg"):
        title = title[:-13]
    img += '<image href="file:' + fullfile + '" y="' + str(y) + '" x="' + str(x) \
           + '" inkscape:svg-dpi="' + str(resolution) \
           + '" width="' + str(iconsize) + '" height="' + str(iconsize) \
           + '" style="image-rendering:optimizeQuality" id="image-' + str(x) + '_' + str(y) + '" />' \
           + '<text x="' + str(x) + '" y="' + str(y + iconsize + 4) \
           + '" class="text"><tspan>' + title + '</tspan></text>'

svg = svg.replace('<g id="g1" />', '<g id="g1">' + img + '</g>')

os.makedirs(output_dir, exist_ok=True)
if os.path.isfile(output_path):
    os.remove(output_path)
with open(output_path, "w") as f:
    f.write(svg)

print("Wrote", output_path)
