#!/usr/bin/env python3
"""
Generate application icon for Clarity Layer.

Creates a simple accessibility-themed icon (eye with magnifying lens).
Requires: pip install pillow
"""

import os
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("Error: Pillow library required. Install with: pip install pillow")
    exit(1)


def create_icon(size):
    """Create a single icon image at the specified size."""
    # Create transparent background
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Calculate dimensions based on size
    padding = size // 8
    center = size // 2
    eye_radius = (size - 2 * padding) // 2

    # Background circle (soft blue)
    draw.ellipse(
        [padding, padding, size - padding, size - padding],
        fill=(41, 128, 185, 255),  # Nice blue color
        outline=(52, 152, 219, 255)
    )

    # Eye white
    eye_h_radius = eye_radius * 7 // 10
    eye_v_radius = eye_radius * 5 // 10
    draw.ellipse(
        [center - eye_h_radius, center - eye_v_radius,
         center + eye_h_radius, center + eye_v_radius],
        fill=(255, 255, 255, 255),
        outline=(33, 97, 140, 255),
        width=max(1, size // 32)
    )

    # Pupil (dark)
    pupil_radius = eye_v_radius * 6 // 10
    draw.ellipse(
        [center - pupil_radius, center - pupil_radius,
         center + pupil_radius, center + pupil_radius],
        fill=(33, 47, 61, 255)
    )

    # Pupil highlight
    highlight_radius = pupil_radius // 3
    highlight_offset = pupil_radius // 3
    draw.ellipse(
        [center - highlight_offset - highlight_radius,
         center - highlight_offset - highlight_radius,
         center - highlight_offset + highlight_radius,
         center - highlight_offset + highlight_radius],
        fill=(255, 255, 255, 255)
    )

    # Magnifying lens (bottom-right corner)
    lens_size = size // 3
    lens_x = size - lens_size - padding // 2
    lens_y = size - lens_size - padding // 2

    # Lens handle
    handle_width = max(2, size // 16)
    draw.line(
        [lens_x + lens_size // 3, lens_y + lens_size // 3,
         lens_x + lens_size, lens_y + lens_size],
        fill=(142, 68, 173, 255),  # Purple
        width=handle_width
    )

    # Lens circle
    lens_radius = lens_size // 3
    draw.ellipse(
        [lens_x, lens_y,
         lens_x + lens_radius * 2, lens_y + lens_radius * 2],
        fill=(155, 89, 182, 128),  # Semi-transparent purple
        outline=(142, 68, 173, 255),
        width=max(1, size // 32)
    )

    return img


def main():
    # Get output directory
    script_dir = Path(__file__).parent
    output_dir = script_dir.parent / "assets" / "icons"
    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"Generating icon in: {output_dir}")
    print()

    # Generate icons at multiple sizes for ICO file
    sizes = [16, 32, 48, 256]
    images = []

    for size in sizes:
        img = create_icon(size)
        images.append(img)
        print(f"Generated {size}x{size} icon")

    # Save as ICO file (multi-resolution)
    ico_path = output_dir / "clarity.ico"
    images[0].save(
        str(ico_path),
        format='ICO',
        sizes=[(s, s) for s in sizes],
        append_images=images[1:]
    )
    print()
    print(f"Created: {ico_path}")

    # Also save a PNG version for other uses
    png_path = output_dir / "clarity_256.png"
    images[-1].save(str(png_path), format='PNG')
    print(f"Created: {png_path}")

    print()
    print("Icon generation complete!")


if __name__ == "__main__":
    main()
