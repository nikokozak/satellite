#!/bin/bash

# Configuration
SOURCE_DIR=~/Downloads/archive
TARGET_DIR=./bin/data/imgs
NUM_FILES=100

# Make sure target directory exists
mkdir -p "$TARGET_DIR"

# Find all TIFF files and select 100 random ones
find "$SOURCE_DIR" -type f \( -name "*.tiff" -o -name "*.tif" \) | sort -R | head -n "$NUM_FILES" | while read file; do
    # Extract filename without path and extension
    filename=$(basename "$file")
    base_filename="${filename%.*}"
    
    # Convert TIFF to JPEG with 85% quality (good balance between size and quality)
    echo "Converting: $filename"
    convert "$file" -quality 85 "$TARGET_DIR/${base_filename}.jpg"
done

# Count how many files were converted
echo "Conversion complete. $(ls -l "$TARGET_DIR" | grep -c "\.jpg$") JPEG files created in $TARGET_DIR" 