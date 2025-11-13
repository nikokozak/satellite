#!/bin/bash
# Bundle external libraries into the .app for portability

APP_PATH="bin/plotter.app"
FRAMEWORKS_DIR="$APP_PATH/Contents/Frameworks"
EXECUTABLE="$APP_PATH/Contents/MacOS/plotter"

echo "Bundling external libraries into $APP_PATH..."

# Create Frameworks directory if it doesn't exist
mkdir -p "$FRAMEWORKS_DIR"

# Find all external dylibs (homebrew or /usr/local)
EXTERNAL_LIBS=$(otool -L "$EXECUTABLE" | grep -E "(homebrew|usr/local)" | awk '{print $1}')

for lib in $EXTERNAL_LIBS; do
    if [ -f "$lib" ]; then
        lib_name=$(basename "$lib")
        echo "  Copying $lib_name..."

        # Copy the library
        cp "$lib" "$FRAMEWORKS_DIR/"

        # Change the install path in the executable to look in @executable_path/../Frameworks
        install_name_tool -change "$lib" "@executable_path/../Frameworks/$lib_name" "$EXECUTABLE"

        # Update the library's own install name
        install_name_tool -id "@executable_path/../Frameworks/$lib_name" "$FRAMEWORKS_DIR/$lib_name"

        echo "    ✓ Bundled $lib_name"
    else
        echo "    ✗ Warning: $lib not found"
    fi
done

echo ""
echo "Done! Verifying..."
otool -L "$EXECUTABLE" | grep -E "(homebrew|usr/local)" && echo "⚠️  Still has external dependencies" || echo "✓ All external dependencies bundled!"
