#!/usr/bin/env python3
"""
Create a multi-layer ICO file with standard sizes: 48x48, 32x32, and 16x16.
This script uses PIL (Pillow) to read an existing icon/image and create a proper
multi-resolution ICO file that can be used as a favicon or application icon.

Usage:
    python scripts/create_multi_layer_icon.py input_image.png output.ico
    python scripts/create_multi_layer_icon.py input_image.png eds/favicon.ico

If input_image is not provided, the script will create a simple default icon.
"""

import sys
import os
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Error: PIL (Pillow) is required. Install with: pip install Pillow")
    sys.exit(1)

def create_default_icon():
    """Create a simple default icon with ESP32 text."""
    # Create 48x48 image (will be scaled down for smaller sizes)
    img = Image.new('RGBA', (48, 48), (0, 100, 200, 255))  # Blue background
    draw = ImageDraw.Draw(img)
    
    # Try to use a font, fall back to default if not available
    try:
        # Try to use a larger font
        font = ImageFont.truetype("arial.ttf", 12)
    except:
        try:
            font = ImageFont.load_default()
        except:
            font = None
    
    # Draw "ESP32" text
    text = "ESP32"
    bbox = draw.textbbox((0, 0), text, font=font) if font else (0, 0, 40, 20)
    text_width = bbox[2] - bbox[0]
    text_height = bbox[3] - bbox[1]
    position = ((48 - text_width) // 2, (48 - text_height) // 2)
    draw.text(position, text, fill=(255, 255, 255, 255), font=font)
    
    return img

def create_multi_layer_ico(input_path=None, output_path='favicon.ico'):
    """
    Create a multi-layer ICO file with 48x48, 32x32, and 16x16 sizes.
    
    Args:
        input_path: Path to input image (PNG, ICO, etc.). If None, creates default icon.
        output_path: Path to output ICO file.
    """
    # Load or create the source image
    if input_path and os.path.exists(input_path):
        print(f"Loading source image: {input_path}")
        source = Image.open(input_path)
        # Convert to RGBA if needed
        if source.mode != 'RGBA':
            source = source.convert('RGBA')
    else:
        if input_path:
            print(f"Warning: Input image '{input_path}' not found, creating default icon")
        else:
            print("Creating default icon (no input specified)")
        source = create_default_icon()
    
    # Create images at required sizes: 48x48, 32x32, 16x16
    sizes = [48, 32, 16]
    images = []
    
    for size in sizes:
        # Resize with high-quality resampling
        img = source.resize((size, size), Image.Resampling.LANCZOS)
        images.append(img)
        print(f"  Created {size}x{size} image")
    
    # Save as ICO file with multiple sizes
    # PIL's save method will automatically create a multi-layer ICO
    images[0].save(
        output_path,
        format='ICO',
        sizes=[(img.width, img.height) for img in images],
        append_images=images[1:] if len(images) > 1 else []
    )
    
    # Verify the ICO file
    file_size = os.path.getsize(output_path)
    print(f"\nCreated multi-layer ICO file: {output_path}")
    print(f"  File size: {file_size} bytes")
    print(f"  Contains {len(images)} image sizes: {', '.join(f'{s}x{s}' for s in sizes)}")
    
    # Read back to verify
    verify = Image.open(output_path)
    if hasattr(verify, 'sizes'):
        print(f"  Verified ICO contains sizes: {verify.sizes}")
    verify.close()
    
    return output_path

if __name__ == '__main__':
    input_file = None
    output_file = 'eds/favicon.ico'
    
    if len(sys.argv) >= 2:
        input_file = sys.argv[1]
    if len(sys.argv) >= 3:
        output_file = sys.argv[2]
    
    # Ensure output directory exists
    output_dir = os.path.dirname(output_file)
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)
        print(f"Created directory: {output_dir}")
    
    print(f"Creating multi-layer ICO file...")
    print(f"  Output: {output_file}")
    
    try:
        create_multi_layer_ico(input_file, output_file)
        print("\nSuccess! The ICO file is ready to use.")
        print("\nTo use it as the favicon for the File Object:")
        print("  - The build system will automatically use eds/favicon.ico")
        print("  - Or specify a different file in CMakeLists.txt")
    except Exception as e:
        print(f"\nError: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)

