# Satellite Image Plotter - Technical Specification

## Overview

This application processes satellite images to identify paths and guide a connected plotter along these paths. It builds on an existing plotter control system, adding functionality to automatically analyze images and derive simplified paths for the plotter to follow.

## Architecture

The application uses OpenFrameworks (OF) for rendering and image processing. Key components include:

1. **Main Application (ofApp)**: Central controller handling UI, image processing, and plotter communication
2. **Image Processing**: Simple edge detection and path extraction
3. **Serial Communication**: Sending coordinates to the physical plotter device
4. **Dual Window Display**: Main window showing the full image with paths, second window showing the crop view

## Core Functionality

### Image Loading and Processing

- `loadRandomImage()`: Loads images from the `imgs` directory, scales them to fit a 2000x1500 canvas while maintaining aspect ratio
- `findImagePaths()`: Processes loaded images to detect edges and identify paths
- `processCannyEdges()`: Implements a simplified edge detection algorithm (threshold-based)
- `simplifyPath()`: Reduces complex paths to a manageable set of key points (3-4)

### Plotter Control

- `moveToNextPathPoint()`: Guides the plotter sequentially through path points
- `isNearPoint()`: Determines when the plotter has reached a target point
- Serial communication sending 'g' commands with X,Y coordinates

### Tracking Modes

- **Mouse Track**: Crop rectangle follows mouse position
- **Plotter Track**: Crop rectangle follows actual plotter position
- **Path Track**: Crop rectangle follows path points, plotter is guided along detected paths

## Data Structures

- `vector<string> imageFiles`: Collection of satellite image paths
- `vector<ofPoint> pathPoints`: Collection of all detected edge points
- `vector<ofPoint> simplifiedPath`: Reduced set of 3-4 key points for plotter traversal

## User Interface

- Real-time visualization of detected paths and current plotter target
- Status information showing tracking mode, point counts, and completion status
- Keyboard controls for changing modes and triggering actions
- Color-coded path points (green=visited, yellow=current, red=future)

## Algorithm Details

### Path Detection and Simplification

1. Convert image to grayscale using `ofxCv::cvtColor`
2. Apply Gaussian blur (`ofxCv::blur`) to reduce noise
3. Apply Canny edge detection (`ofxCv::Canny`) with configurable thresholds
4. Find contours (`ofxCv::findContours`) in the resulting edge map
5. Select the longest contour found
6. Resample the points along the longest contour (`ofPolyline::getResampledBySpacing`) to populate `pathPoints`
7. The `simplifyPath` function then selects a subset of these points (currently evenly spaced) for `simplifiedPath`

### Plotter Movement Logic

1. Set the first point as the initial target
2. Send coordinates to the plotter until it reaches the point (distance < threshold)
3. Advance to the next point when the plotter is near enough to the current target
4. When all points are visited, wait briefly then load a new image

## Future Improvements

- Implement true Canny edge detection using OpenCV
- Add contour following for more coherent paths
- Implement more sophisticated path simplification (Douglas-Peucker algorithm)
- Add user controls for adjusting detection parameters
- Support for saving/loading interesting paths 