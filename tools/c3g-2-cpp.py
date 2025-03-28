"""
ChatGPT genrated code to convert .c3g (cpt-city palette in CSS3 format) file to C++ array for FastLED CRGBPalette16 generation.
Download palette files from http://seaviewsensing.com/pub/cpt-city/
"""
# Usage: python c3g-2-cpp.py <c3g_file_path> [<array_name>]

import argparse
import re
import os
import requests

def rgb_to_tuple(rgb_str):
    """
    Convert a CSS3 rgb(r, g, b) string to an (R, G, B) tuple.
    Example: "rgb(255, 0, 0)" -> (255, 0, 0)
    """
    match = re.match(r'rgb\(\s*(\d+),\s*(\d+),\s*(\d+)\)', rgb_str)
    if match:
        r = int(match.group(1))
        g = int(match.group(2))
        b = int(match.group(3))
        return r, g, b
    else:
        raise ValueError(f"Invalid RGB format: {rgb_str}")

def extract_colors_from_gradient(gradient):
    """
    Extract all color codes and their positions from a CSS3 linear-gradient string.
    Example: "linear-gradient(0deg, rgb(252,216,252) 0.000%, rgb(255,192,255) 18.990%, ...)"
    -> [("rgb(252,216,252)", 0.0), ("rgb(255,192,255)", 18.990), ...]
    """
    # Regex to match the colors in rgb(r, g, b) format and their positions
    color_position_regex = r'rgb\(\s*(\d+),\s*(\d+),\s*(\d+)\)\s*(\d+\.\d+)%'
    
    # Find all colors and their positions in the gradient string
    return [(f'rgb({r}, {g}, {b})', float(position)) for r, g, b, position in re.findall(color_position_regex, gradient)]

def position_to_index(position):
    """
    Convert a percentage position in the gradient (0.0 to 100.0) to an index in a 0-255 range.
    The range of 0% to 100% is divided into 256 sections (for a palette of size 256).
    """
    return int(position / 100 * 255)  # Maps 0.0% to index 0, 100.0% to index 255

def c3g_to_cpp_array(c3g_file_path, array_name):
    # Open the .c3g file
    with open(c3g_file_path, 'r') as f:
        lines = f.readlines()

    # Initialize an empty list for the colors
    color_array = []

    # Process each line in the file
    for line in lines:
        # Remove comments (single-line and block comments)
        line = re.sub(r'//.*$', '', line)  # Remove single-line comments
        line = re.sub(r'/\*.*?\*/', '', line, flags=re.DOTALL)  # Remove block comments

        # Strip any extra whitespace
        line = line.strip()

        # Skip empty lines
        if not line:
            continue
        
        # If the line contains a linear-gradient, extract the colors and positions
        if 'rgb(' in line:
            # Extract the gradient colors and their positions
            color_positions = extract_colors_from_gradient(line)
            # Convert each color to RGB and map it to the appropriate index
            for color, position in color_positions:
                r, g, b = rgb_to_tuple(color)
                index = position_to_index(position)
                color_array.append(f"{index:>3}, {r:>3}, {g:>3}, {b:>3}")
    
    # Format the final C++ array string with the custom array name
    cpp_array = f"const uint8_t {array_name}[] PROGMEM = {{\n  " + ",\n  ".join(color_array) + "};"
    
    return cpp_array


def download_file(url, save_path):
    """
    Download a file from a URL and save it to a specified path.
    """
    try:
        response = requests.get(url)
        response.raise_for_status()  # Raise an error for bad status codes
        with open(save_path, 'wb') as f:
            f.write(response.content)
        #print(f"Downloaded {url} to {save_path}")
    except requests.exceptions.RequestException as e:
        print(f"Error downloading {url}: {e}")

def process_urls_from_file(url_file, array_name=None):
    """
    Process each URL from a file, download the corresponding .c3g file,
    and convert it to a C++ array.
    """
    with open(url_file, 'r') as f:
        urls = f.readlines()

    # For each URL, download the file and convert it
    for url in urls:
        url = url.strip()  # Remove any leading/trailing whitespace or newlines
        if not url:
            continue

        # Extract file name from the URL or use a default name
        file_name = os.path.basename(url)
        if not file_name.endswith('.c3g'):
            file_name += '.c3g'

        # Download the file
        download_file(url, file_name)

        if not os.path.exists(file_name):
            continue

        # Determine array name (use provided array name or file name without extension)
        if array_name is None:
            array_name = os.path.splitext(file_name)[0] + '_gp'

        # Convert the .c3g file to C++ array
        cpp_code = c3g_to_cpp_array(file_name, array_name)
        print(f"// Gradient palette \"{array_name}\", originally from")
        print(f"// {url}")
        print(cpp_code)
        print()  # Empty line between outputs

        array_name = None

        # Optionally, you can save the C++ code to a file
        # with open(f"{array_name}_palette.cpp", 'w') as f:
        #     f.write(cpp_code)
        os.remove(file_name)  # Remove the downloaded file after processing


def main():
    # Create the parser object
    parser = argparse.ArgumentParser(description="Convert .c3g file with gradients to a C++ array for FastLED.")
    
    # Add command-line argument for the .c3g file
    parser.add_argument("c3g_file", help="Path to the .c3g file to be converted or .txt file with URLs")
    parser.add_argument("array_name", nargs="?", help="Name of the C++ array to be generated")

    # Parse arguments
    args = parser.parse_args()
    

    if args.c3g_file.endswith('.c3g'):
        # If no array name is provided, use the file name without extension as the array name
        if not args.array_name:
            array_name = os.path.splitext(os.path.basename(args.c3g_file))[0]
        else:
            array_name = args.array_name

        # Call the function to process the file and convert to C++ array format
        cpp_palette_code = c3g_to_cpp_array(args.c3g_file, array_name)
        
        # Output the result to the console
        print(f"// Gradient palette \"{array_name}\", originally from")
        print(f"// {args.c3g_file}")
        print(cpp_palette_code)
        
    else:
        # Process URLs from the provided text file
        process_urls_from_file(args.c3g_file, args.array_name)


if __name__ == "__main__":
    main()
