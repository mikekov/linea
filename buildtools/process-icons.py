#!/usr/bin/env python3
"""
Process SVG icons from Dash theme to create themed versions.
Files ending in -symbolic.svg get black/white replaced with theme colors;
other files are passed through with their colors intact (header still
normalized). Generates .qrc files aliasing each icon without -symbolic.
"""

import os
import sys
import re
import argparse
from pathlib import Path


# Semantic style classes get lighter fill variants in the dark theme so the
# accents stay legible on dark backgrounds. Matched by class name, not color.
SEMANTIC_DARK_COLORS = {
    'success': '#4ab563',
    'error': '#f94657',
}


def process_svg_file(input_path, output_path, color, background_color, recolor=True, semantic_colors=None):
    """Process a single SVG file: normalize the header for Qt resource loading and,
    when recolor is True, replace black/white with the specified theme colors."""
    with open(input_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Strip XML declaration and ensure xmlns="http://www.w3.org/2000/svg" is within
    # the first 256 bytes so QMimeDatabase / QSvgIconEngine recognizes extensionless
    # :/icons/* qrc aliases as image/svg+xml instead of falling back to raster QPixmap.
    content = re.sub(r'^\s*<\?xml[^?]*\?>\s*', '', content)
    content = re.sub(
        r'<svg\b([^>]*?)\s+xmlns="http://www\.w3\.org/2000/svg"',
        r'<svg xmlns="http://www.w3.org/2000/svg"\1',
        content,
        count=1,
    )

    modified_content = content
    if recolor:
        # Replace black color values with the specified hex color
        modified_content = re.sub(r'\bblack\b', color, modified_content, flags=re.IGNORECASE)
        modified_content = modified_content.replace('#000000', color)

        # Replace white color values with the specified background color
        if background_color:
            modified_content = re.sub(r'\bwhite\b', background_color, modified_content, flags=re.IGNORECASE)
            modified_content = modified_content.replace('#ffffff', background_color)
            modified_content = modified_content.replace('#FFFFFF', background_color)

        # Change colors in a style block for a dark theme. Target semantic classes (.success/.error)
        if semantic_colors:
            for cls, new_color in semantic_colors.items():
                pattern = r'(\.' + cls + r'\s*\{[^}]*?fill\s*:\s*)#[0-9a-fA-F]{3,6}'
                modified_content = re.sub(
                    r'(<style\b[^>]*>.*?</style>)',
                    lambda m: re.sub(pattern, r'\g<1>' + new_color, m.group(1)),
                    modified_content,
                    flags=re.DOTALL,
                )

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(modified_content)


def process_icons(source_dirs, output_dir, color, background_color, semantic_colors=None):
    """Process all SVG files from every source dir into one output dir."""
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    # Drop stale outputs from previous runs: icons renamed or deleted in the
    # sources would otherwise linger here and produce duplicate qrc aliases.
    for stale in output_path.glob('*.svg'):
        stale.unlink()

    for source_dir in source_dirs:
        for svg_file in Path(source_dir).glob('*.svg'):
            output_file = output_path / svg_file.name
            # files without the -symbolic suffix keep their own colors
            recolor = svg_file.stem.endswith('-symbolic')
            process_svg_file(svg_file, output_file, color, background_color, recolor,
                             semantic_colors if recolor else None)
            print(f"Processed: {svg_file.name} -> {color} on {background_color}" if recolor
                  else f"Copied: {svg_file.name} (colors intact)")


def generate_qrc(icons_dir, qrc_output, prefix="/icons"):
    """Generate a .qrc file for the processed icons."""
    qrc_content = f'''<!DOCTYPE RCC>
<RCC version="1.0">
    <qresource prefix="{prefix}">
'''

    # Add all SVG files to qrc with relative paths from build directory
    build_dir = Path(icons_dir).parent.parent  # Go up from icons/light to build
    for svg_file in sorted(Path(icons_dir).glob('*.svg')):
        # Extract base name without -symbolic.svg
        base_name = svg_file.stem.replace('-symbolic', '')
        # Get relative path from build directory
        rel_path = svg_file.relative_to(build_dir)
        qrc_content += f'        <file alias="{base_name}">{rel_path}</file>\n'

    qrc_content += '''    </qresource>
</RCC>
'''

    with open(qrc_output, 'w', encoding='utf-8') as f:
        f.write(qrc_content)

    print(f"Generated QRC file: {qrc_output}")


def main():
    parser = argparse.ArgumentParser(description='Process SVG icons for theming')
    parser.add_argument('--source', required=True, nargs='+', help='Source directories containing SVG files')
    parser.add_argument('--build-dir', required=True, help='Build directory for output')
    parser.add_argument('--light-color', default='#262626', help='Hex color for light theme (default: #262626)')
    parser.add_argument('--dark-color', default='#ececec', help='Hex color for dark theme (default: #ececec)')
    parser.add_argument('--light-bg-color', default='#ffffff', help='Hex background color for light theme (default: #ffffff)')
    parser.add_argument('--dark-bg-color', default='#1c201f', help='Hex background color for dark theme (default: #1c201f)')

    args = parser.parse_args()

    source_dirs = [Path(s) for s in args.source]
    build_dir = Path(args.build_dir)

    for source_dir in source_dirs:
        if not source_dir.exists():
            print(f"Error: Source directory {source_dir} does not exist")
            sys.exit(1)

    # Create themed icon sets
    light_output = build_dir / 'icons' / 'light'
    dark_output = build_dir / 'icons' / 'dark'

    light_qrc = build_dir / 'icons-light.qrc'
    dark_qrc = build_dir / 'icons-dark.qrc'

    print(f"Processing icons from: {[str(s) for s in source_dirs]}")
    print(f"Light theme color: {args.light_color} on {args.light_bg_color}")
    print(f"Dark theme color: {args.dark_color} on {args.dark_bg_color}")
    print()

    # Process light theme icons
    # Use empty prefix so .rcc can be registered under /icons at runtime
    print("Processing light theme icons...")
    process_icons(source_dirs, light_output, args.light_color, args.light_bg_color)
    generate_qrc(light_output, light_qrc, "/")

    print()

    # Process dark theme icons
    print("Processing dark theme icons...")
    process_icons(source_dirs, dark_output, args.dark_color, args.dark_bg_color,
                  SEMANTIC_DARK_COLORS)
    generate_qrc(dark_output, dark_qrc, "/")

    print()
    print("Done!")


if __name__ == '__main__':
    main()
