#include "ofApp.h"
// #include "ofAppRunner.h" // Removed - didn't fix compile issue

// Define crop dimensions (can be adjusted)
const int CROP_W = 320;
const int CROP_H = 240;

// Define canvas dimensions for satellite images
const int CANVAS_WIDTH = 2000;
const int CANVAS_HEIGHT = 1500;

// Define path parameters
const int MAX_PATH_POINTS = 4; // Maximum number of simplified path points

//--------------------------------------------------------------
void ofApp::setup(){
    // Initialize crop dimensions
    cropWidth = CROP_W;
    cropHeight = CROP_H;

    // Initialize path parameters
    currentPathPoint = 0;
    pathComplete = true; // Start with path complete so we load a new image
    distanceThreshold = 40.0f; // How close the plotter needs to be to consider a point reached (Increased tolerance)
    
    // Initialize new path timing/state vars
    pointTimeoutDuration = 3.0f; // Timeout after 3 seconds if stuck
    pointPauseDuration = 3.0f;   // Pause for 3 seconds at each point
    waitingAtPoint = false;
    waitingForEmptyPathMove = false;
    emptyPathMoveDuration = 5.0f; // Wait 5 seconds after random move
    currentPointStartTime = 0.0f;
    pointReachedTime = 0.0f;
    emptyPathMoveStartTime = 0.0f;
    
    // Initialize debug mode
    debugMode = false;

    // Scan the images directory
    ofDirectory imgsDir("imgs"); // Correct path relative to data folder within bin
    if (!imgsDir.exists()) {
        ofLogError("ofApp::setup") << "!!! imgs directory NOT FOUND at: " << ofFilePath::getAbsolutePath("data/imgs"); // Log absolute path for clarity
        // Create directory if not found
        imgsDir.create(true); // Create recursively if needed
    } else {
        ofLogNotice("ofApp::setup") << "Directory found: " << ofFilePath::getAbsolutePath("data/imgs"); // Log directory exists
        
        // Log contents BEFORE filtering
        imgsDir.listDir();
        ofLogNotice("ofApp::setup") << "Total files/dirs found before filter: " << imgsDir.size();
        for(int i = 0; i < imgsDir.size(); i++){
            ofLogNotice("ofApp::setup") << "  -> Pre-filter item: " << imgsDir.getName(i);
        }

        // Now filter
        imgsDir.allowExt("jpg");
        imgsDir.sort(); // Sort for consistent order if needed
        imgsDir.listDir(); // Re-list *after* filtering
        ofLogNotice("ofApp::setup") << "Files found *after* jpg filter: " << imgsDir.size();

        imageFiles.clear(); // Clear previous list if any
        for (int i = 0; i < imgsDir.size(); i++) {
            imageFiles.push_back(imgsDir.getPath(i));
            ofLogNotice("ofApp::setup") << "  -> Found image file (post-filter): " << imgsDir.getPath(i); // Log each found file
        }

        ofLogNotice("ofApp::setup") << "Added " << imageFiles.size() << " images to imageFiles vector from " << ofFilePath::getAbsolutePath("data/imgs");
    }
    
    currentImageIndex = -1; // Start with -1 so first loadRandomImage picks index 0
    
    // Load initial image (either from imgs directory or default)
    loadRandomImage(); // Call loadRandomImage AFTER setting up directory scan

    // Get image dimensions for window setup and bounds checking
    // Note: Image dimensions might not be known until loadRandomImage runs
    // So we set window based on constants initially
    ofSetWindowShape(CANVAS_WIDTH, CANVAS_HEIGHT); // Set window size first
    ofSetFrameRate(60);
    
    // Initialize variables
    mouseX = CANVAS_WIDTH / 2; // Use canvas size for initial center
    mouseY = CANVAS_HEIGHT / 2;
    serialMode = false;
    lastSendTime = 0;
    sendInterval = 1.0;
    lastReconnectAttempt = 0;
    plotterX = 0.0f;
    plotterY = 0.0f;
    currentMode = PLOTTER_TRACK; // Default to plotter tracking
    
    // Initialize crop rectangle (will be adjusted in update based on image)
    cropWidth = CROP_W;
    cropHeight = CROP_H;
    cropRect.width = cropWidth;
    cropRect.height = cropHeight;
    cropRect.setFromCenter(mouseX, mouseY, cropWidth, cropHeight); // Center initially based on canvas

    // Setup serial connection
    string portName = findArduinoPort();
    if(portName != "") {
        serial.setup(portName, 115200);
        ofLogNotice() << "Connected to serial port: " << portName;
    } else {
        ofLogError() << "Could not find Arduino port";
    }

    // Initialize received data string
    receivedData = "";
    serialBuffer = "";
    lastPlotterMsg = "";

    // --- Allocate FBO for crop ---
    ofFbo::Settings fboSettings;
    fboSettings.width = cropWidth;
    fboSettings.height = cropHeight;
    fboSettings.internalformat = GL_RGB; // Or GL_RGBA if alpha is needed
    cropFbo.allocate(fboSettings);

    // --- Second Window Setup (Restored) ---
    ofLogNotice() << "Attempting to set up second window.";

    ofGLFWWindowSettings settings;
    // Position the second window.
    // IMPORTANT: Hardcoding position/monitor index without ofGetDisplays.
    // Assumes primary display is 0, secondary is 1. Adjust if needed.
    // Position might need tweaking based on actual display layout.
    float primaryDisplayWidth = ofGetWindowWidth(); // Use window width after it's set
    settings.setPosition(glm::vec2(primaryDisplayWidth, 0));
    settings.setSize(1920, 1080); // Example size, adjust as needed
    settings.windowMode = OF_FULLSCREEN; // Start in windowed mode first for testing
    // settings.windowMode = OF_FULLSCREEN; // Use fullscreen later
    settings.shareContextWith = ofGetCurrentWindow(); // Share OpenGL resources
    settings.monitor = 2; // Target the second monitor explicitly (index 1)
    settings.setPosition({-1920.f, 0.f}); // We need this to move the screen, monitor doesn't really do anything.

    secondWindow = ofCreateWindow(settings);
    if(secondWindow){
        secondWindow->setVerticalSync(true);
        ofAddListener(secondWindow->events().draw, this, &ofApp::drawSecondWindow);
        ofLogNotice() << "Second window created (potentially) on monitor " << settings.monitor
                  << " at " << settings.getPosition().x << "," << settings.getPosition().y
                  << " with size " << settings.getWidth() << "x" << settings.getHeight();
    } else {
        ofLogError("ofApp::setup") << "Failed to create second window.";
    }
    // --- End Restored Second Window Setup ---
    
    // Initial image processing to find paths
    findImagePaths();
}

//--------------------------------------------------------------
void ofApp::loadRandomImage() {
    ofLogNotice("ofApp::loadRandomImage") << "Attempting to load a new random image.";
    
    // Clear previous image data
    image.clear(); 
    pathPoints.clear();
    simplifiedPath.clear();
    currentPathPoint = 0;
    pathComplete = true; // Assume failure until image loads successfully
    
    // If no images in directory, try to load the default pic.jpg
    if (imageFiles.empty()) {
        ofLogWarning("ofApp::loadRandomImage") << "No images found in imageFiles vector. Trying default 'pic.jpg'";
        if (!image.load("pic.jpg")) {
            ofLogError("ofApp::loadRandomImage") << "Failed to load default image: pic.jpg";
            // Create a blank image
            image.allocate(CANVAS_WIDTH, CANVAS_HEIGHT, OF_IMAGE_COLOR);
            image.setColor(ofColor::black);
            image.update();
        } else {
             ofLogNotice("ofApp::loadRandomImage") << "Successfully loaded default 'pic.jpg'";
             pathComplete = false; // Image loaded, ready for processing
        }
    } else {
        // Pick next image in sequence (or loop back to first)
        currentImageIndex = (currentImageIndex + 1) % imageFiles.size();
        string imagePath = imageFiles[currentImageIndex];
        ofLogNotice("ofApp::loadRandomImage") << "Attempting to load image [" << currentImageIndex << "]: " << imagePath;
        
        if (!image.load(imagePath)) {
            ofLogError("ofApp::loadRandomImage") << "!!! Failed to load image: " << imagePath;
            // Try to load default image
            if (!image.load("pic.jpg")) {
                ofLogError("ofApp::loadRandomImage") << "!!! Failed to load default image 'pic.jpg' as fallback.";
                // Create a blank image as last resort
                image.allocate(CANVAS_WIDTH, CANVAS_HEIGHT, OF_IMAGE_COLOR);
                image.setColor(ofColor::black);
            } else {
                ofLogNotice("ofApp::loadRandomImage") << "Successfully loaded default 'pic.jpg' as fallback.";
                pathComplete = false; // Image loaded, ready for processing
            }
        } else {
             ofLogNotice("ofApp::loadRandomImage") << "Successfully loaded image: " << imagePath;
             pathComplete = false; // Image loaded, ready for processing
        }
    }

    // --- Image Scaling and Centering ---
    // Keep the original loaded image in `image`
    // We will calculate scaling factors for drawing in the draw() function.
    // No need to resize or create a larger canvas here.

    // Reset path tracking and process the new image if loaded successfully
    if (image.isAllocated()) {
        ofLogNotice("ofApp::loadRandomImage") << "Image allocated. Dimensions: " << image.getWidth() << "x" << image.getHeight();
        findImagePaths(); // Process the new image to find paths
        // Reset path state variables AFTER findImagePaths (which sets initial pathComplete status)
        waitingAtPoint = false;
        waitingForEmptyPathMove = false;
        currentPointStartTime = ofGetElapsedTimef(); // Start timer for the first point (if any)
        pointReachedTime = 0.0f;
        emptyPathMoveStartTime = 0.0f;
    } else {
        ofLogError("ofApp::loadRandomImage") << "Image is not allocated after load attempt.";
        pathComplete = true; // Ensure path is marked complete if load fails
    }
}

//--------------------------------------------------------------
void ofApp::findImagePaths() {
    if (!image.isAllocated()) {
         ofLogWarning("ofApp::findImagePaths") << "Skipping path finding - image not allocated.";
         pathComplete = true; // Mark as complete if no image
         return;
    }
    ofLogNotice("ofApp::findImagePaths") << "Starting image path processing.";
    
    // Initialize processed image
    processedImage.allocate(image.getWidth(), image.getHeight(), OF_IMAGE_COLOR);
    processedImage = image; // Use copy assignment instead of clone
    
    // Process with Canny edge detection
    processCannyEdges();
    
    // Generate a simplified path from the edge detection results
    simplifyPath(); // This resets currentPathPoint and pathComplete based on simplifiedPath size

    // Reset path state variables (important if called manually via 'p')
    waitingAtPoint = false;
    waitingForEmptyPathMove = false;
    currentPointStartTime = ofGetElapsedTimef(); // Start timer for the first point (if any)
    pointReachedTime = 0.0f;
    emptyPathMoveStartTime = 0.0f;
    
    // --- Send command for the FIRST point ---
    if (!pathComplete && !simplifiedPath.empty() && serialMode && serial.isInitialized()) {
        ofPoint target = simplifiedPath[0]; // Target the first point
        // Map image coords (top=0) to canvas coords (bottom=0) for plotter
        int sendX = ofMap(target.x, 0, image.getWidth(), 0, CANVAS_WIDTH, true);
        int sendY = ofMap(target.y, 0, image.getHeight(), CANVAS_HEIGHT, 0, true); // Invert Y
        sendX = ofClamp(sendX, 0, CANVAS_WIDTH);
        sendY = ofClamp(sendY, 0, CANVAS_HEIGHT);
        string command = "g" + ofToString(sendX) + "," + ofToString(sendY) + "\n";
        serial.writeBytes((unsigned char*)command.c_str(), command.length());
        ofLogNotice("ofApp::findImagePaths") << "Sent initial command for point 0: " << command;
    }
    // --- End send command for first point ---
}

//--------------------------------------------------------------
void ofApp::processCannyEdges() {
    if (!processedImage.isAllocated()) {
        ofLogWarning("ofApp::processCannyEdges") << "Skipping Canny edges - processedImage not allocated.";
        return;
    }
    ofLogNotice("ofApp::processCannyEdges") << "Processing Canny edges using ofxCv.";

    // Clear previous path points
    pathPoints.clear();

    // Use ofxCv helpers
    using namespace ofxCv;
    using namespace cv;

    // Convert the color processedImage to grayscale
    cv::Mat grayMat;
    // Ensure processedImage is OF_IMAGE_COLOR or OF_IMAGE_GRAYSCALE
    if (processedImage.getPixels().getNumChannels() == 1) {
        // Already grayscale
        grayMat = ofxCv::toCv(processedImage); // Directly convert to cv::Mat
    } else if (processedImage.getPixels().getNumChannels() >= 3) {
        // Convert color to gray
        cv::Mat colorMat = ofxCv::toCv(processedImage); // Convert ofImage to cv::Mat first
        cv::cvtColor(colorMat, grayMat, cv::COLOR_RGB2GRAY); // Now convert color space
    } else {
        ofLogError("ofApp::processCannyEdges") << "Unsupported image type for Canny.";
        return;
    }

    // Blur to filter out noise
    GaussianBlur(grayMat, grayMat, cv::Size(5, 5), 0);

    // Apply Canny edge detection
    cv::Mat cannyMat;
    // Adjust Canny thresholds as needed (lower threshold, upper threshold)
    // These values often require tuning based on image content
    float threshold1 = 50;  // Lower threshold
    float threshold2 = 150; // Upper threshold
    Canny(grayMat, cannyMat, threshold1, threshold2);

    // This will show the Canny edges if drawn
    toOf(cannyMat, processedImage.getPixels());
    processedImage.update();

    //processedImage.draw(0, 0, 1920, 1080);

    // Find contours in the Canny output
    cv::Rect roi(0, 0, cannyMat.cols, cannyMat.rows); // Define Region of Interest (whole image)
    // Find contours finds white shapes in a black background
    this->contourFinder.setMinAreaRadius(5); // Ignore very small contours
    this->contourFinder.setMaxAreaRadius(500); // Ignore very large contours (tune as needed)
    this->contourFinder.setThreshold(127); // Threshold for finding contours in the Canny image (should be binary)
    this->contourFinder.findContours(cannyMat);

    ofLogNotice("ofApp::processCannyEdges") << "Found " << this->contourFinder.size() << " contours.";

    // Find the longest contour
    int longestContourIndex = -1;
    float maxContourLength = 0;
    for (int i = 0; i < this->contourFinder.size(); i++) {
        float length = this->contourFinder.getPolyline(i).getPerimeter();
        if (length > maxContourLength) {
            maxContourLength = length;
            longestContourIndex = i;
        }
    }

    // Extract points from the longest contour
    if (longestContourIndex != -1) {
        ofPolyline longestPolyline = this->contourFinder.getPolyline(longestContourIndex);
        // Resample the polyline to get a more evenly distributed set of points
        // Adjust the spacing parameter (e.g., 10.0f) as needed
        ofPolyline resampledPolyline = longestPolyline.getResampledBySpacing(10.0f);
        pathPoints.clear(); // Ensure pathPoints is empty before adding
        const auto& vertices = resampledPolyline.getVertices(); // Get const reference
        pathPoints.reserve(vertices.size()); // Optional: Pre-allocate memory
        for(const auto& vertex : vertices) {
            pathPoints.push_back(vertex); // Copy each vertex (glm::vec3 to ofPoint)
        }
        ofLogNotice("ofApp::processCannyEdges") << "Using longest contour (index " << longestContourIndex << ") with " << pathPoints.size() << " resampled points.";
    } else {
        ofLogWarning("ofApp::processCannyEdges") << "No suitable contours found to generate path points.";
    }

    // pathPoints vector is now populated with points from the longest contour
    // The simplifyPath function will take over from here.
}

//--------------------------------------------------------------
void ofApp::simplifyPath() {
    // Clear the existing simplified path
    simplifiedPath.clear();
    ofLogNotice("ofApp::simplifyPath") << "Simplifying path from " << pathPoints.size() << " points.";
    
    // If we found very few points, use them all
    if (pathPoints.size() <= MAX_PATH_POINTS) {
        simplifiedPath = pathPoints;
    } else {
        // Select 3-4 key points from the path
        int step = pathPoints.size() / MAX_PATH_POINTS;
        for (int i = 0; i < MAX_PATH_POINTS && i * step < pathPoints.size(); i++) {
            simplifiedPath.push_back(pathPoints[i * step]);
        }
    }
    
    // Make sure we have at least one point
    if (simplifiedPath.empty() && !pathPoints.empty()) {
        simplifiedPath.push_back(pathPoints[0]);
    }
    
    currentPathPoint = 0;
    pathComplete = simplifiedPath.empty();
    
    ofLogNotice("ofApp::simplifyPath") << "Simplified to " << simplifiedPath.size() << " key points";
}

//--------------------------------------------------------------
void ofApp::moveToNextPathPoint() {
    float currentTime = ofGetElapsedTimef();

    // 1. Handle waiting after a random move for an empty path
    if (waitingForEmptyPathMove) {
        if (currentTime - emptyPathMoveStartTime >= emptyPathMoveDuration) {
            ofLogNotice("ofApp::moveToNextPathPoint") << "Empty path wait finished. Loading next image.";
            pathComplete = true; // Mark complete *now* so the check below triggers load
            // loadRandomImage(); // Let the main pathComplete check handle loading
        } else {
           return; // Keep waiting otherwise
        }
    }

    // 2. Handle path completion (if flagged by reaching the end OR after empty path wait)
    if (pathComplete) {
        ofLogNotice("ofApp::moveToNextPathPoint") << "Path complete flag is set. Loading next image.";
        loadRandomImage(); // This will reset pathComplete, etc.
        return;
    }

    // 3. Handle case where simplifiedPath is empty (should trigger random move)
    if (simplifiedPath.empty()) {
        ofLogWarning("ofApp::moveToNextPathPoint") << "Simplified path is empty. Sending random move.";
        if (serialMode && serial.isInitialized()) {
            // Send a random point within the canvas
            int randX = ofRandom(0, CANVAS_WIDTH);
            int randY = ofRandom(0, CANVAS_HEIGHT); // Plotter uses bottom=0
            string command = "g" + ofToString(randX) + "," + ofToString(randY) + "\n";
            serial.writeBytes((unsigned char*)command.c_str(), command.length());
            ofLogNotice("ofApp::moveToNextPathPoint") << "Sent random command: " << command;
        }
        waitingForEmptyPathMove = true;
        emptyPathMoveStartTime = currentTime;
        // pathComplete = true; // REMOVED: Don't mark complete yet, wait first.
        return; // Start waiting
    }

    // --- If we have a path and aren't waiting for an empty move ---

    // Make sure currentPathPoint is valid
    if (currentPathPoint < 0 || currentPathPoint >= simplifiedPath.size()) {
        ofLogError("ofApp::moveToNextPathPoint") << "Invalid currentPathPoint: " << currentPathPoint << ". Resetting path.";
        pathComplete = true; // Mark as complete to force reload
        return;
    }

    // 4. Handle waiting at a reached point
    if (waitingAtPoint) {
        if (currentTime - pointReachedTime >= pointPauseDuration) {
            // Pause finished, move to next point
            ofLogNotice("ofApp::moveToNextPathPoint") << "Pause finished at point " << currentPathPoint << ". Moving to next.";
            currentPathPoint++;
            waitingAtPoint = false;

            // Check if path is now complete
            if (currentPathPoint >= simplifiedPath.size()) {
                pathComplete = true;
                ofLogNotice("ofApp::moveToNextPathPoint") << "Reached end of path.";
                return; // Next update will load a new image
            } else {
                // Path not complete, send command for the NEW point
                currentPointStartTime = currentTime; // Reset start timer for the new point
                 if (serialMode && serial.isInitialized()) {
                    ofPoint target = simplifiedPath[currentPathPoint];
                    // Map image coords (top=0) to canvas coords (bottom=0) for plotter
                    int sendX = ofMap(target.x, 0, image.getWidth(), 0, CANVAS_WIDTH, true);
                    int sendY = ofMap(target.y, 0, image.getHeight(), CANVAS_HEIGHT, 0, true); // Invert Y
                    sendX = ofClamp(sendX, 0, CANVAS_WIDTH);
                    sendY = ofClamp(sendY, 0, CANVAS_HEIGHT);
                    string command = "g" + ofToString(sendX) + "," + ofToString(sendY) + "\n";
                    serial.writeBytes((unsigned char*)command.c_str(), command.length());
                    ofLogNotice("ofApp::moveToNextPathPoint") << "Sent command for new point " << currentPathPoint << ": " << command;
                 }
            }
        }
        return; // Still waiting at point
    }

    // 5. If not waiting at a point, check if we've arrived or timed out
    ofPoint currentTarget = simplifiedPath[currentPathPoint];
    if (isNearPoint(currentTarget)) {
        // Reached the point
        ofLogNotice("ofApp::moveToNextPathPoint") << "Reached point " << currentPathPoint << ". Starting pause.";
        waitingAtPoint = true;
        pointReachedTime = currentTime;
        return; // Start waiting
    } else {
        // Not near point, check for timeout
        if (currentTime - currentPointStartTime >= pointTimeoutDuration) {
            ofLogWarning("ofApp::moveToNextPathPoint") << "Timeout waiting for point " << currentPathPoint << ". Skipping.";
            // Timeout, skip to next point
            currentPathPoint++;

            // Check if path is now complete
            if (currentPathPoint >= simplifiedPath.size()) {
                pathComplete = true;
                ofLogNotice("ofApp::moveToNextPathPoint") << "Reached end of path after timeout.";
                return; // Next update will load a new image
            } else {
                // Path not complete, send command for the NEW point
                currentPointStartTime = currentTime; // Reset start timer for the new point
                if (serialMode && serial.isInitialized()) {
                    ofPoint target = simplifiedPath[currentPathPoint];
                    // Map image coords (top=0) to canvas coords (bottom=0) for plotter
                    int sendX = ofMap(target.x, 0, image.getWidth(), 0, CANVAS_WIDTH, true);
                    int sendY = ofMap(target.y, 0, image.getHeight(), CANVAS_HEIGHT, 0, true); // Invert Y
                    sendX = ofClamp(sendX, 0, CANVAS_WIDTH);
                    sendY = ofClamp(sendY, 0, CANVAS_HEIGHT);
                    string command = "g" + ofToString(sendX) + "," + ofToString(sendY) + "\n";
                    serial.writeBytes((unsigned char*)command.c_str(), command.length());
                    ofLogNotice("ofApp::moveToNextPathPoint") << "Sent command for new point " << currentPathPoint << " after timeout: " << command;
                }
            }
        }
        // No return here: If not near and not timed out, just keep waiting for plotter feedback.
        // Command was already sent when we started targeting this point (or after previous pause/timeout).
    }
}

//--------------------------------------------------------------
bool ofApp::isNearPoint(const ofPoint& point) {
    // Map plotter coords (Canvas Space) to Image Space for comparison
    // Note: Requires image to be allocated to get dimensions
    float plotterImageX = plotterX;
    float plotterImageY = plotterY;
    if (image.isAllocated()) {
        plotterImageX = ofMap(plotterX, 0, CANVAS_WIDTH, 0, image.getWidth(), true);
        plotterImageY = ofMap(plotterY, 0, CANVAS_HEIGHT, image.getHeight(), 0, true); // NEW: Invert plotter Y for Image Space comparison
    }
    // Calculate distance in Image Space
    float distance = ofDist(plotterImageX, plotterImageY, point.x, point.y);
    return distance < distanceThreshold;
}

//--------------------------------------------------------------
void ofApp::update(){
    // image.update(); // Not usually needed for static images unless manipulating pixels

    // Update mouse coordinates (relative to the main window)
    mouseX = ofGetMouseX();
    mouseY = ofGetMouseY();

    // --- Serial Reconnection Logic ---
    // Check if serial died and try to reconnect every 5 seconds
    if (!serial.isInitialized()) {
        float currentTime = ofGetElapsedTimef();
        if (currentTime - lastReconnectAttempt >= 5.0f) {
            string portName = findArduinoPort();
            if (portName != "") {
                serial.setup(portName, 115200);
                if (serial.isInitialized()) {
                    ofLogNotice() << "Reconnected to Arduino on port: " << portName;
                }
            }
            lastReconnectAttempt = currentTime;
        }
    }

    // --- Update Crop Rectangle ---
    // Ensure image is loaded before calculating crop based on its dimensions
    if (!image.isAllocated()) {
        // Default behavior or log error if image isn't ready
        // cropRect remains at its initial canvas-centered position
    } else {
        float imageWidth = image.getWidth();
        float imageHeight = image.getHeight();

        // Calculate desired center based on tracking mode
        float targetX = 0;
        float targetY = 0;

        if (currentMode == PLOTTER_TRACK) {
            // Map plotter coords (assumed Canvas Space, but plotter sends bottom=0 Y) to Image Space (top=0) for crop centering
            targetX = ofMap(plotterX, 0, CANVAS_WIDTH, 0, imageWidth, true);
            targetY = ofMap(plotterY, 0, CANVAS_HEIGHT, imageHeight, 0, true); // NEW: Invert plotter Y for Image Space
        } else if (currentMode == PATH_TRACK) {
            // Crop should follow the CURRENT plotter position, even in path mode.
            // Map plotter coords (assumed Canvas Space, plotter sends bottom=0 Y) to Image Space (top=0) for crop centering.
            targetX = ofMap(plotterX, 0, CANVAS_WIDTH, 0, imageWidth, true);
            targetY = ofMap(plotterY, 0, CANVAS_HEIGHT, imageHeight, 0, true); // NEW: Invert plotter Y for Image Space

            // Still need to check path progression
            moveToNextPathPoint();
        }

        // Clamp the center position so the crop rectangle stays within *original* image bounds
        float clampedX = ofClamp(targetX, cropWidth / 2.0f, imageWidth - cropWidth / 2.0f);
        float clampedY = ofClamp(targetY, cropHeight / 2.0f, imageHeight - cropHeight / 2.0f);

        // Set the crop rectangle position based on the clamped center (these are coords within the original image)
        cropRect.setFromCenter(clampedX, clampedY, cropWidth, cropHeight);
    } // End if (image.isAllocated())

    // --- Draw cropped image into FBO ---
    if (image.isAllocated() && cropFbo.isAllocated()) { // Check both are ready
        cropFbo.begin();
        ofClear(0, 0, 0, 0);
        ofSetColor(255);
        // Draw the subsection of the *original* image into the FBO
        image.getTexture().drawSubsection(0, 0, cropFbo.getWidth(), cropFbo.getHeight(),
                                         cropRect.x, cropRect.y, cropRect.width, cropRect.height);
        cropFbo.end();
    } else if (cropFbo.isAllocated()) {
        // Clear FBO if image isn't ready to prevent stale/black frames
        cropFbo.begin();
        ofClear(0, 0, 0, 255); // Clear with opaque black
        cropFbo.end();
    }

    // Send serial command ONLY in PLOTTER_TRACK mode to follow the mouse
    if(currentMode == PLOTTER_TRACK && serialMode && image.isAllocated()) {
        float currentTime = ofGetElapsedTimef();
        if(currentTime - lastSendTime >= sendInterval) {
            if (serial.isInitialized()) {
                // Send MOUSE coordinates mapped to CANVAS dimensions
                int serialX = ofMap(mouseX, 0, ofGetWidth(), 0, CANVAS_WIDTH, true);
                int serialY = ofMap(mouseY, 0, ofGetHeight(), CANVAS_HEIGHT, 0, true);

                // Clamp values just in case mapping goes slightly out
                serialX = ofClamp(serialX, 0, CANVAS_WIDTH);
                serialY = ofClamp(serialY, 0, CANVAS_HEIGHT);

                string command = "g" + ofToString(serialX) + "," + ofToString(serialY) + "\n";
                serial.writeBytes((unsigned char*)command.c_str(), command.length());
                
                lastSendTime = currentTime; // Update time only after sending
            }
        }
    }

    // --- Process Incoming Serial Data ---
    if(serial.isInitialized() && serial.available() > 0) {
        // Read all available bytes
        unsigned char bytesReturned[300]; // Increase buffer size slightly
        memset(bytesReturned, 0, 300);
        int nRead = serial.readBytes(bytesReturned, 299); // Read up to 299 bytes
        if (nRead > 0) {
            // Append the received bytes to the buffer
            receivedData = (char*)bytesReturned; // Keep showing raw chunks for debug
            serialBuffer.append((char*)bytesReturned, nRead);
        }
    }

    // Process complete lines from the buffer
    size_t newlinePos;
    while ((newlinePos = serialBuffer.find('\n')) != std::string::npos) {
        // Extract the line including the newline
        std::string line = serialBuffer.substr(0, newlinePos + 1);
        
        // Remove the line from the buffer
        serialBuffer.erase(0, newlinePos + 1);

        // Trim whitespace (like \r that might be before \n)
        ofStringReplace(line, "\r", ""); 
        ofStringReplace(line, "\n", "");

        // Check if it's a plotter position command ("pX,Y")
        if (line.length() > 1 && line[0] == 'p') {
            std::string payload = line.substr(1);
            std::vector<std::string> parts = ofSplitString(payload, ",");
            if (parts.size() == 2) {
                try {
                    plotterX = std::stof(parts[0]);
                    plotterY = std::stof(parts[1]);
                    lastPlotterMsg = line; // Store the successfully parsed line
                    ofLogVerbose("Serial Parse") << "Parsed plotter pos: " << plotterX << ", " << plotterY;
                } catch (const std::invalid_argument& ia) {
                    ofLogWarning("Serial Parse") << "Could not parse plotter coords from: " << line << " - Invalid argument";
                } catch (const std::out_of_range& oor) {
                    ofLogWarning("Serial Parse") << "Could not parse plotter coords from: " << line << " - Out of range";
                }
            } else {
                ofLogWarning("Serial Parse") << "Ignoring malformed 'p' command: " << line;
            }
        } else {
            // Optional: Log other received lines for debugging
            // ofLogVerbose("Serial Receive") << "Received non-plotter line: " << line;
        }
    }
    // --- End Process Incoming Serial Data ---
}

//--------------------------------------------------------------
void ofApp::draw(){
    ofBackground(0); // Black background for main window

    if (image.isAllocated()) {
        ofSetColor(255);
        
        // --- Calculate Scaling to Fit Canvas ---
        float imgW = image.getWidth();
        float imgH = image.getHeight();
        float canvasW = ofGetWidth(); // Use actual window width
        float canvasH = ofGetHeight(); // Use actual window height
        
        float imgAspect = imgW / imgH;
        float canvasAspect = canvasW / canvasH;
        
        float drawW, drawH, drawX, drawY;
        
        if (imgAspect > canvasAspect) {
            // Image is wider than canvas aspect ratio -> fit to width
            drawW = canvasW;
            drawH = drawW / imgAspect;
            drawX = 0;
            drawY = (canvasH - drawH) / 2.0f; // Center vertically
        } else {
            // Image is taller or same aspect ratio -> fit to height
            drawH = canvasH;
            drawW = drawH * imgAspect;
            drawY = 0;
            drawX = (canvasW - drawW) / 2.0f; // Center horizontally
        }

        // --- Draw the Scaled Image ---
        image.draw(drawX, drawY, drawW, drawH);

        // --- Draw Debug Overlay if Enabled --- 
        if (debugMode) {
            // Draw Canny output (stored in processedImage) semi-transparently
            if (processedImage.isAllocated()) {
                ofPushStyle();
                ofSetColor(255, 255, 255, 100); // White, semi-transparent
                processedImage.draw(drawX, drawY, drawW, drawH);
                ofPopStyle();
            }
            // Draw contours found
            ofPushStyle();
            ofSetLineWidth(2);
            ofSetColor(ofColor::magenta); // Draw contours in magenta
            
            ofPushMatrix(); // Save the current transformation state
            ofTranslate(drawX, drawY); // Move the origin to the top-left of where the image is drawn
            // We need to scale the contour drawing to match the image scaling
            // The contours are found in the original image dimensions (imgW, imgH)
            // The image is drawn at (drawW, drawH)
            ofScale(drawW / imgW, drawH / imgH);
            
            contourFinder.draw(); // Draw the contours (now relative to the translated/scaled origin)
            
            ofPopMatrix(); // Restore the previous transformation state
            ofPopStyle();
        }

        // --- Draw Overlay Elements Scaled to Match Image ---
        // Calculate scale factor from original image to drawn size
        float scaleX = drawW / imgW;
        float scaleY = drawH / imgH;

        // Draw the processed image overlay if needed (scaled)
        if (currentMode == PATH_TRACK && processedImage.isAllocated()) {
            ofSetColor(255, 255, 255, 100); // Semi-transparent overlay
            processedImage.draw(drawX, drawY, drawW, drawH);
        }

        // Draw the simplified path points (scaled and offset)
        if (currentMode == PATH_TRACK && !simplifiedPath.empty()) {
            ofPushStyle();
            ofSetLineWidth(2);
            
            // Draw connecting lines (scaled)
            ofSetColor(ofColor::green, 180);
            ofNoFill();
            ofBeginShape();
            for (const auto& point : simplifiedPath) {
                ofVertex(drawX + point.x * scaleX, drawY + point.y * scaleY);
            }
            ofEndShape(false);
            
            // Draw circles at each point (scaled)
            ofFill();
            float pointRadius = 10; // Keep radius constant on screen
            for (int i = 0; i < simplifiedPath.size(); i++) {
                 if (i < currentPathPoint) ofSetColor(ofColor::green); // Visited
                 else if (i == currentPathPoint) ofSetColor(ofColor::yellow); // Current target
                 else ofSetColor(ofColor::red); // Future
                
                ofDrawCircle(drawX + simplifiedPath[i].x * scaleX, drawY + simplifiedPath[i].y * scaleY, pointRadius);
            }
            
            ofPopStyle();
        }

        // Draw the red crop rectangle indicator (scaled and offset)
        ofPushStyle();
        ofNoFill();
        ofSetColor(ofColor::red);
        ofSetLineWidth(2);
        // Scale the cropRect coordinates and dimensions
        ofDrawRectangle(drawX + cropRect.x * scaleX, 
                        drawY + cropRect.y * scaleY, 
                        cropRect.width * scaleX, 
                        cropRect.height * scaleY);
        ofPopStyle();

    } else {
        // Draw placeholder if image failed to load
        ofSetColor(50);
        ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());
        ofSetColor(255);
        ofDrawBitmapString("Image failed to load or none found in ./bin/data/imgs", ofGetWidth()/2 - 150, ofGetHeight()/2);
    }

    // Draw text overlays (Coordinates, Serial Status, Received Data) - these stay relative to window
    ofSetColor(255); // White text
    // Map mouse to original image coords for display text
    int displayMouseX = image.isAllocated() ? ofMap(mouseX, 0, ofGetWidth(), 0, image.getWidth(), true) : mouseX;
    int displayMouseY = image.isAllocated() ? ofMap(mouseY, 0, ofGetHeight(), 0, image.getHeight(), true) : mouseY;
    string coordText = "Mouse (Win): " + ofToString(mouseX) + "," + ofToString(mouseY) + 
                      " | (Img): " + ofToString(displayMouseX) + "," + ofToString(displayMouseY);
    ofDrawBitmapStringHighlight(coordText, 10, 20, ofColor::black, ofColor::white);

    string trackModeText = "Track Mode: ";
    if (currentMode == PLOTTER_TRACK) {
        trackModeText = "Track Mode: Plotter (M)";
    } else { // PATH_TRACK
        trackModeText = "Track Mode: Path (M)";
    }
    ofDrawBitmapStringHighlight(trackModeText, 10, 40, ofColor::black, ofColor::white);

    string statusText = "Serial Mode: " + string(serialMode ? "ON" : "OFF") + " (Space)";
    ofDrawBitmapStringHighlight(statusText, 10, 60, ofColor::black, ofColor::white);
    
    string pathText = "Path Points: " + ofToString(simplifiedPath.size()) +
                     " Current: " + ofToString(currentPathPoint) +
                     " Complete: " + string(pathComplete ? "YES" : "NO");
    ofDrawBitmapStringHighlight(pathText, 10, 80, ofColor::black, ofColor::white);
    
    string imageText = "Image: " + (imageFiles.empty() ? "None" : 
                       ofToString(currentImageIndex+1) + "/" + ofToString(imageFiles.size()));
    ofDrawBitmapStringHighlight(imageText, 10, 100, ofColor::black, ofColor::white);

    string debugText = "Debug Mode (d): " + string(debugMode ? "ON" : "OFF");
    ofDrawBitmapStringHighlight(debugText, 10, 120, ofColor::black, ofColor::white);

    if(lastPlotterMsg != "") {
        ofDrawBitmapStringHighlight("Plotter Pos Msg: " + lastPlotterMsg + 
                                " (X: " + ofToString(plotterX, 2) + ", Y: " + ofToString(plotterY, 2) + ")", 
                                    10, ofGetHeight() - 40, ofColor::black, ofColor::white);
    }
    if(receivedData != "") {
        ofDrawBitmapStringHighlight("Raw Serial In: " + receivedData, 10, ofGetHeight() - 20, ofColor::black, ofColor::white);
    }
}

//--------------------------------------------------------------
void ofApp::drawSecondWindow(ofEventArgs & args){
    // This function is called automatically to draw on the second window
    if (!cropFbo.isAllocated()) {
        ofBackground(20, 0, 0); // Dark red background if FBO not ready
        ofSetColor(255);
        ofDrawBitmapStringHighlight("Crop FBO not allocated", 20, 20, ofColor::black, ofColor::white);
        return; 
    }

    ofBackground(0); // Black background for second window

    // Draw the FBO content, scaled to fill the second window
    ofSetColor(255);
    cropFbo.draw(0, 0, secondWindow->getWidth(), secondWindow->getHeight());

    // Optionally add text or overlays to the second window here
    // ofDrawBitmapStringHighlight("Cropped View (FBO)", 20, 20, ofColor::black, ofColor::white);
}

//--------------------------------------------------------------
string ofApp::findArduinoPort() {
    std::vector<ofSerialDeviceInfo> devices = serial.getDeviceList();
    for(auto& device : devices) {
        string name = device.getDeviceName();
        // Expanded check for common Arduino port names
        if(name.find("usbmodem") != string::npos ||
           name.find("tty.usbmodem") != string::npos || // macOS
           name.find("ttyACM") != string::npos ) {       // Linux
            ofLogNotice() << "Found potential Arduino port: " << device.getDevicePath();
            return device.getDevicePath();
        }
    }
    return "";
}

//--------------------------------------------------------------
void ofApp::exit(){
    if(serial.isInitialized()) {
        serial.close();
    }
    // Clean up listener if second window was created
    if (secondWindow) {
        ofRemoveListener(secondWindow->events().draw, this, &ofApp::drawSecondWindow);
    }
    image.clear(); // Clear image resources
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if(key == ' ') {
        serialMode = !serialMode;
        if(serialMode) {
            lastSendTime = ofGetElapsedTimef(); // Reset timer when turning on
        }
    }
    if(key == 'm') {
        // Cycle between PLOTTER_TRACK and PATH_TRACK
        if (currentMode == PLOTTER_TRACK) {
            currentMode = PATH_TRACK;
            ofLogNotice() << "Switched to PATH_TRACK mode";
            // Process the current image to find paths if needed
            if (simplifiedPath.empty()) {
                findImagePaths();
            }
        } else { // Was PATH_TRACK
            currentMode = PLOTTER_TRACK;
            ofLogNotice() << "Switched to PLOTTER_TRACK mode";
        }
    }
    if(key == 'n') {
        // Force load next image
        loadRandomImage();
    }
    if(key == 'p') {
        // Re-process current image to find new paths
        findImagePaths();
    }
    if(key == 'd') {
        debugMode = !debugMode;
        ofLogNotice() << "Debug Mode: " << (debugMode ? "ON" : "OFF");
    }
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){ }

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){ } // Already handled in update

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){ }

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){ }

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){ }

//--------------------------------------------------------------
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY){ }

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){ }

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){ }

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){ }

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ }

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){ }

