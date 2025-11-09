#pragma once

#include "ofMain.h"
#include "ofxDlib.h"
#include "ofxCv.h"

using namespace dlib;

// Tracking mode for the crop rectangle
enum TrackingMode {
	PLOTTER_TRACK,
	PATH_TRACK
};

class ofApp : public ofBaseApp{

	public:
		void setup() override;
		void update() override;
		void draw() override;
		void exit() override;

		void keyPressed(int key) override;
		void keyReleased(int key) override;
		void mouseMoved(int x, int y ) override;
		void mouseDragged(int x, int y, int button) override;
		void mousePressed(int x, int y, int button) override;
		void mouseReleased(int x, int y, int button) override;
		void mouseScrolled(int x, int y, float scrollX, float scrollY) override;
		void mouseEntered(int x, int y) override;
		void mouseExited(int x, int y) override;
		void windowResized(int w, int h) override;
		void dragEvent(ofDragInfo dragInfo) override;
		void gotMessage(ofMessage msg) override;
		
		// Mouse coordinates
		int mouseX, mouseY;
		
		// Serial communication
		ofSerial serial;
		bool serialMode;
		float lastSendTime;
		float sendInterval;
		float lastReconnectAttempt;

		// Find Arduino serial port
		string findArduinoPort();
		
		// Received serial data
		string receivedData; // Stores raw incoming data chunks
		string serialBuffer; // Accumulates serial data
		string lastPlotterMsg; // Last valid 'pX,Y' message received
		float plotterX;
		float plotterY;
		
		// Tracking Mode
		TrackingMode currentMode;
		
		// Image display
		ofImage image; // Replaced ofVideoPlayer

		// Cropping
		int cropWidth;
		int cropHeight;
		ofRectangle cropRect;

		// FBO for cropped video
		ofFbo cropFbo;

		// DLib rect
        dlib::frontal_face_detector detector;
		std::vector<dlib::rectangle> dlibRect;

		// Second window
		shared_ptr<ofAppBaseWindow> secondWindow;
		void drawSecondWindow(ofEventArgs & args); // Draw callback for second window
		
		// Image loading and processing
		std::vector<string> imageFiles;
		int currentImageIndex;
		void loadRandomImage();
		ofImage processedImage; // Stores edge detection results
		
		// Path generation and tracking
		void findImagePaths();
		void processCannyEdges();
		void simplifyPath();
		std::vector<ofPoint> pathPoints; // Full detected path
		std::vector<ofPoint> simplifiedPath; // 3-4 key points
		int currentPathPoint; // Index of current target point
		bool pathComplete; // Flag to indicate when we've reached end of path
		void moveToNextPathPoint();
		bool isNearPoint(const ofPoint& point);
		float distanceThreshold; // How close we need to be to consider at a point

        // --- New Path Timing and State Variables ---
        float currentPointStartTime; // Timestamp when we started targeting the current point
        float pointReachedTime;      // Timestamp when isNearPoint became true
        float pointTimeoutDuration;  // How long to wait before skipping a stuck point
        float pointPauseDuration;    // How long to pause after reaching a point
        bool waitingAtPoint;         // Flag: true if we've reached a point and are pausing
        bool waitingForEmptyPathMove; // Flag: true if path was empty and we sent a random move
        float emptyPathMoveStartTime; // Timestamp for the random move command start
        float emptyPathMoveDuration;  // How long to wait after the random move
        // --- End New Path Timing and State Variables ---

        // --- Debug Visualization --- 
        bool debugMode; // Flag to toggle debug view
        ofxCv::ContourFinder contourFinder; // Store contours for drawing
};
