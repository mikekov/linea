#!/usr/bin/env python3
"""
Process SVG icons from Dash theme to create themed versions.
Replaces currentColor with specified hex colors and generates .qrc files.
"""

import os
import sys
import re
import argparse
from pathlib import Path


def process_svg_file(input_path, output_path, color, background_color):
    """Process a single SVG file, replacing currentColor with the specified color."""
    with open(input_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Replace black color values with the specified hex color
    modified_content = re.sub(r'\bblack\b', color, content, flags=re.IGNORECASE)
    modified_content = modified_content.replace('#000000', color)

    # Replace white color values with the specified background color
    if background_color:
        modified_content = re.sub(r'\bwhite\b', background_color, modified_content, flags=re.IGNORECASE)
        modified_content = modified_content.replace('#ffffff', background_color)
        modified_content = modified_content.replace('#FFFFFF', background_color)

    # Also replace explicit black (#000000, black) if needed
    # Uncomment if you want to replace explicit black references too
    # modified_content = re.sub(r'#000000', color, modified_content, flags=re.IGNORECASE)
    # modified_content = re.sub(r'black', color, modified_content, flags=re.IGNORECASE)

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(modified_content)


def process_icons(source_dir, output_dir, color, background_color, qrc_output, prefix="/icons"):
    """Process all SVG icons and generate qrc file."""
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    # Process all SVG files
    svg_files = list(Path(source_dir).glob('*.svg'))

    for svg_file in svg_files:
        output_file = output_path / svg_file.name
        process_svg_file(svg_file, output_file, color, background_color)
        print(f"Processed: {svg_file.name} -> {color} on {background_color}")

    # Generate qrc file with specified prefix
    generate_qrc(output_path, qrc_output, prefix)


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
    parser.add_argument('--source', required=True, help='Source directory containing SVG files')
    parser.add_argument('--build-dir', required=True, help='Build directory for output')
    parser.add_argument('--light-color', default='#262626', help='Hex color for light theme (default: #262626)')
    parser.add_argument('--dark-color', default='#ececec', help='Hex color for dark theme (default: #ececec)')
    parser.add_argument('--light-bg-color', default='#ffffff', help='Hex background color for light theme (default: #ffffff)')
    parser.add_argument('--dark-bg-color', default='#1c201f', help='Hex background color for dark theme (default: #1c201f)')

    args = parser.parse_args()

    source_dir = Path(args.source)
    build_dir = Path(args.build_dir)

    if not source_dir.exists():
        print(f"Error: Source directory {source_dir} does not exist")
        sys.exit(1)

    # Create themed icon sets
    light_output = build_dir / 'icons' / 'light'
    dark_output = build_dir / 'icons' / 'dark'

    light_qrc = build_dir / 'icons-light.qrc'
    dark_qrc = build_dir / 'icons-dark.qrc'

    print(f"Processing icons from: {source_dir}")
    print(f"Light theme color: {args.light_color} on {args.light_bg_color}")
    print(f"Dark theme color: {args.dark_color} on {args.dark_bg_color}")
    print()

    # Process light theme icons
    # Use empty prefix so .rcc can be registered under /icons at runtime
    print("Processing light theme icons...")
    process_icons(source_dir, light_output, args.light_color, args.light_bg_color, light_qrc, "/")

    print()

    # Process dark theme icons
    print("Processing dark theme icons...")
    process_icons(source_dir, dark_output, args.dark_color, args.dark_bg_color, dark_qrc, "/")

    print()
    print("Done!")


if __name__ == '__main__':
    main()
