# Satellite Image Plotter

This application processes satellite images to generate paths for a connected plotter to follow.

## Setup

1. Place satellite images in the `bin/data/imgs` directory
2. Launch the application
3. The app will load a random image from the directory and find edges/paths

## Controls

- **Space**: Toggle serial mode (sending coordinates to plotter)
- **M**: Cycle through tracking modes (Mouse -> Plotter -> Path)
- **N**: Load next image
- **P**: Re-process current image to find new paths

## Tracking Modes

1. **Mouse Track**: The crop rectangle follows mouse position
2. **Plotter Track**: The crop rectangle follows actual plotter position
3. **Path Track**: The app finds paths in the image and guides the plotter along them

## Workflow

1. Images are automatically loaded from the `imgs` folder and scaled to fit a 2000x1500 canvas
2. In Path Track mode, the app detects edges and identifies paths in the image
3. Paths are simplified to 3-4 key points for the plotter to follow
4. Once the plotter completes a path, a new image is loaded automatically

## Technical Details

- Simple edge detection is used to find paths (threshold-based)
- Path points are sampled at regular intervals
- The plotter is guided sequentially through path points
- A distance threshold determines when the plotter has reached a point 