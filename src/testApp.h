#pragma once

#include "ofMain.h"
#include "ofxCvHaarFinder.h"
#include "ofxCv.h"
#include "ofxGui.h"

#define MAX_FACES 4

class testApp : public ofBaseApp{
	public:
		void setup();
		void update();
		void draw();
		
		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);
        void setupShader();
        void drawDebug();
        void drawWide();
        void rotatedCVprocess();
        void standardCVprocess();
    
        ofImage cvScaleImage;
    
        int camWidth, camHeight, cvWidth, cvHeight;
    
        ofVideoGrabber cam;
    
        ofxCv::ObjectFinder faceFinder;
    
        vector <ofImage> cropFace;
    
        vector <ofRectangle> foundFace;
    
        ofImage rotateIntoMe;
    
        ofShader shader;
    
        vector <ofFbo> faceFbo;
    
        ofFbo maskFbo;
        
        ofImage featherMask;
        
        float boxSize;
        
        bool bDrawDebug;
        
        ofPoint maskSquish;
    
        ofImage testCapture;
    
        const int maxFaces = 3;
    
        ofTexture tex1;
        ofFbo largeFbo;
        
        ofxPanel gui;
        ofxFloatSlider maskSizeX, maskSizeY, maskOffsetX, maskOffsetY, boxSizeX, boxSizeY, postCvOffsetX, postCvOffsetY;
        ofxFloatSlider  leftScreenYTop, leftScreenYBottom, leftScreenXRight, leftScreenXLeft;
        ofxFloatSlider  rightScreenYTop, rightScreenYBottom, rightScreenXRight, rightScreenXLeft;
    
        ofxIntSlider faceOpacity;
    
        ofxIntSlider transCvX, transCvY, cvNudgeY, cvNudgeX;
    
        ofxToggle portraitCam, flopCam, bDrawWide, bSaveImages;
    
        float snapShotTimer;
        float timeBetweenSaves;
        bool screenShot;
    
    ofImage fullScreenGrab;
    
    int screenGrabCounter;
    
        //1280x720 cut in half horizontally 1280x360
    
};
