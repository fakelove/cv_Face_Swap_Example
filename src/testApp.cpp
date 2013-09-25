#include "testApp.h"

using namespace ofxCv;
using namespace cv;

//--------------------------------------------------------------
void testApp::setup(){

    
    ofSetVerticalSync(true);
    ofSetFrameRate(60);
    
    ofEnableAlphaBlending();
    
    camWidth = 1280;
    camHeight = 720;
    
    cvWidth = 640;
    cvHeight = 360;
    
    faceFinder.setup(ofToDataPath("haarcascade_frontalface_alt2.xml")); //THIS FILE IS REQUIRED FOR DETECTION
	faceFinder.setPreset(ObjectFinder::Fast);
    
    //faceFinder.setCannyPruning(true);
    faceFinder.setRescale(0.5); //the size it will rescale the incoming image into
    faceFinder.setMinSizeScale(0.01); //smallest object it will find
    faceFinder.setMaxSizeScale(0.5); //largest object

	faceFinder.getTracker().setSmoothingRate(.25); //keep it from being jittery
    
    cvScaleImage.allocate(cvWidth, cvHeight, OF_IMAGE_COLOR); //for re-sizing image for CV
    rotateIntoMe.allocate(cvWidth, cvHeight, OF_IMAGE_COLOR);
    
    cam.setDeviceID(1);
    cam.initGrabber(camWidth, camHeight);

    ofImage temp;
    temp.allocate(camWidth, camHeight, OF_IMAGE_COLOR);
    cropFace.assign(maxFaces, temp);
    
    foundFace.assign(maxFaces, ofRectangle(0,0,0,0));
    
    boxSize = 0.1;
    
    bDrawDebug = true;
    
    featherMask.loadImage("masks/whiteMask2.png");
    
    maskFbo.allocate(camWidth,camHeight, GL_RGBA);
    
    maskFbo.begin();
    ofClear(0, 0, 0);
    featherMask.draw(0, 0, camWidth, camHeight);
    maskFbo.end();
    
    //Set up the FBO's you'll use for your faces
    ofFbo::Settings s;
    s.width			= camWidth;
    s.height			= camHeight;
    s.internalformat   = GL_RGBA;
    s.useDepth			= true;
    
    ofFbo tempFbo1, tempFbo2, tempFbo3, tempFbo4, tempFbo5;
    
    tempFbo1.allocate(s);
    tempFbo2.allocate(s);
    tempFbo3.allocate(s);
    tempFbo4.allocate(s);
    tempFbo5.allocate(s);
    
    faceFbo.push_back(tempFbo1);
    faceFbo.push_back(tempFbo2);
    faceFbo.push_back(tempFbo3);
    faceFbo.push_back(tempFbo4);
    faceFbo.push_back(tempFbo5);

    for (int i =0; i<maxFaces; i++) {
        faceFbo[i].begin();
        ofClear(0, 0, 0, 255);
        faceFbo[i].end();
    }
    
    largeFbo.allocate(1280, 720, GL_RGB); //draw camera and faces into dis thing, this will be split in half when on 2 monitors
    

    //GUIIIIII
    gui.setup("Panel");
    gui.add(maskSizeX.setup("Mask Size X", 1.0,0.1,1.5)); //mask size within shader
    gui.add(maskSizeY.setup("Mask Size Y", 1.0,0.1,1.5));
    gui.add(maskOffsetX.setup("Mask Offset X", 0.0,0.0,0.5)); //mask offset within shader
    gui.add(maskOffsetY.setup("Mask Offset Y", 0.0,0.0,0.5));
    gui.add(boxSizeX.setup("Box Size X", 0,-0.1,.5)); //size of face
    gui.add(boxSizeY.setup("Box Size Y", 0,-0.1,.5));
    gui.add(leftScreenYTop.setup("Left Screen Y Top", 0.5,0.0,2)); //for tex coords on 2 screens
    gui.add(leftScreenYBottom.setup("Left Screen Y Bottom", 1,0.0,1.5));
    gui.add(leftScreenXLeft.setup("Left Screen X Left", 0.74,-0.5,2));
    gui.add(leftScreenXRight.setup("Left Screen X Right", 0,-0.5,2));
    gui.add(rightScreenYTop.setup("Right Screen Y Top", 0,-0.5,1.5));
    gui.add(rightScreenYBottom.setup("Right Screen Y Bottom", 0.5,0.0,1.5));
    gui.add(rightScreenXLeft.setup("Right Screen X Left", 0.75,-0.5,1.5));
    gui.add(rightScreenXRight.setup("Right Screen X Right", 0,-0.5,1.5));
    gui.add(cvNudgeX.setup("CV Nudge X", 100,-200,300)); //nudges the x/y within cv
    gui.add(cvNudgeY.setup("Cv Nudge Y", 0,-50,50));
    gui.add(postCvOffsetX.setup("Post CV offset X", 0,-200,300)); //nudges the x/y when the face is being re-drawn
    gui.add(postCvOffsetY.setup("Post CV offset Y", 0,-200,300));
    gui.add(transCvX.setup("Trans X", 40,0,260)); //these move the square ROI up and down
    gui.add(transCvY.setup("Trans Y", -300,-400,300)); //moves the face slightly left and right
    gui.add(portraitCam.setup("Portrait Cam", true)); //switch between portrait and landscape CV
    gui.add(flopCam.setup("Flop Camera", true)); //flip if the camera is upside down
    gui.add(bDrawWide.setup("Wide Draw", false)); //this lets you draw across 2 screens
    gui.add(bSaveImages.setup("Save Images", false));
    gui.add(faceOpacity.setup("Face Opacity", 200,0,255));
    gui.loadFromFile("settings.xml");

    setupShader();
    
    timeBetweenSaves = 20; //10 seconds between each image save
    
    snapShotTimer = -20;
    
    screenGrabCounter = 0;
    
}

//--------------------------------------------------------------
void testApp::update(){
	cam.update();
    
    if (portraitCam) { //is your camera sideways?
        rotatedCVprocess();
    }else{
        standardCVprocess();
    }
    
    if (ofGetElapsedTimef()-snapShotTimer > timeBetweenSaves && bSaveImages) { //tells it to save a screenshot every X seconds
        snapShotTimer = ofGetElapsedTimef();
        screenShot = true;
    }
    

}

//--------------------------------------------------------------
void testApp::draw(){
    ofBackground(0);
    ofSetColor(255);
    
    maskFbo.begin();
    ofClear(0, 0, 0); 
    featherMask.draw(camWidth*maskOffsetX, camHeight*maskOffsetY, camWidth*maskSizeX, camHeight*maskSizeY); //draw your mask into an FBO (this could be done in setup, but it's done here since we're changing it's parameters during runtime
    maskFbo.end();

    if(bDrawWide)largeFbo.begin(); //draw into an FBO so we can split it in half to draw across 2 screens
    
    ofDisableNormalizedTexCoords(); //disable so things draw properly

    ofSetColor(255);
    
    //DRAW THE CAMERA
    cam.draw(0,0, camWidth, camHeight);
    
    //DRAW THE FACE CUTOUTS
    if(faceFinder.size() > 0) {
        int numFace = ofClamp(faceFinder.size(),0, maxFaces);
        for(int i=0; i<numFace; i++){
            
            
                //ofSetColor(255,i*50,100);
            ofSetColor(255, 255, 255,faceOpacity);
            
            if(bDrawWide) faceFbo[(i+1)%numFace].draw(postCvOffsetX+ofMap(foundFace[i%numFace].x,0, camWidth,0,ofGetWidth()/2),
                postCvOffsetY+ofMap(foundFace[i%numFace].y,0, camHeight,0,ofGetHeight()),
                ofMap(foundFace[(i+0)%numFace].width, 0, camWidth, 0, ofGetWidth()/2),
                ofMap(foundFace[(i+0)%numFace].height, 0,camHeight, 0, ofGetHeight())); //draw the second face in the first face's position
            
            if(!bDrawWide)faceFbo[(i+1)%numFace].draw(postCvOffsetX+ofMap(foundFace[i%numFace].x,0, camWidth,0,ofGetWidth()),
                                                                postCvOffsetY+ofMap(foundFace[i%numFace].y,0, camHeight,0,ofGetHeight()),
                                                                ofMap(foundFace[(i+0)%numFace].width, 0, camWidth, 0, ofGetWidth()),
                                                                ofMap(foundFace[(i+0)%numFace].height, 0,camHeight, 0, ofGetHeight()));

            if (bDrawDebug) {
                //draw a rectangle around faces in debug
                ofNoFill();
                ofSetLineWidth(4);

                ofSetColor(255, 100, 100,255);
                ofRect(postCvOffsetX+ofMap(foundFace[i%numFace].x,0, camWidth,0,ofGetWidth()),
                       postCvOffsetY+ofMap(foundFace[i%numFace].y,0, camHeight,0,ofGetHeight()),
                       ofMap(foundFace[i%numFace].width, 0, camWidth, 0, ofGetWidth()),
                       ofMap(foundFace[i%numFace].height, 0,camHeight, 0, ofGetHeight()));
                ofSetLineWidth(0.1);
            }
            
        }
        
        if(screenShot && faceFinder.size()>=2){ //screenshot only if 2 faces found
            screenShot = false;
            fullScreenGrab.grabScreen(0, 0, 1280, 720); //just grab the important bit
            fullScreenGrab.saveImage("saveFace/"+ofToString(ofGetSystemTime())+".jpg");

            screenGrabCounter++;
        }

    }
    
    if(bDrawWide)largeFbo.end();

    
    if(bDrawWide)drawWide();
    
    ofDisableNormalizedTexCoords();
    
    
    if(bDrawDebug) {
        
        if(faceFinder.size() > 0) {
            ofSetColor(255);
            for(int i=0; i<ofClamp(faceFinder.size(),0, maxFaces); i++){
                faceFbo[i].draw(i*200, 0, 200,200);
                cropFace[i].draw(i*200, 200, 200,200);
                ofDrawBitmapStringHighlight(ofToString(i), i*200,20);
            }
            ofFill();
            ofSetColor(255, 0, 0);
            ofRect(0,0,ofGetWidth(), 20);
        }
        drawDebug();
        gui.setPosition(ofPoint(30,200));
        gui.draw();
        
        ofPushMatrix();
        ofTranslate(600,300);
        ofRotateZ(-90);
        ofSetColor(255);
        cam.draw(0,0, 1280/4,720/4);
        ofPopMatrix();
        
        testCapture.draw(600+720/4,0, cvHeight/2, cvHeight/2); //this draws the CV texture to screen so you can be sure of what CV is looking at
        
       
    }

    
   

    
}

//--------------------------------------------------------------
void testApp::keyPressed(int key){
    
    switch (key) {
        case 'd':
            bDrawDebug = !bDrawDebug;
            break;
        case 'w':
            ofSetWindowShape(2560, 720);
            break;
        case 'f':
            ofToggleFullscreen();  
            break;
        case 's':
            gui.saveToFile("settings.xml");
            break;
        case 'l':
            gui.loadFromFile("settings.xml");
            break;
        default:
            break;
    }


    
}

//--------------------------------------------------------------
void testApp::keyReleased(int key){

}

//---------------
void testApp::drawDebug(){
    ofSetColor(0);
    ofDrawBitmapStringHighlight("FPS: " + ofToString(ofGetFrameRate()), 20,20);
    ofDrawBitmapStringHighlight("Number of Faces Found: " + ofToString(faceFinder.size()), 20,60);
    ofDrawBitmapStringHighlight("Resolution: " + ofToString(ofGetWidth()) + " " + ofToString(ofGetHeight()), 20,40);
    ofDrawBitmapStringHighlight("Face 1 Width:" +ofToString(foundFace[0].width), 20,80);
    ofDrawBitmapStringHighlight("Face 1 X:" +ofToString(foundFace[0].x), 20,100);
    ofDrawBitmapStringHighlight("Screen Grabs so far:" +ofToString(screenGrabCounter), 20,120);
    

}
//--------------------------------------------------------------
void testApp::setupShader(){
    if(ofGetGLProgrammableRenderer()){
    	string vertex = "#version 150\n\
    	\n\
		uniform mat4 projectionMatrix;\n\
		uniform mat4 modelViewMatrix;\n\
    	uniform mat4 modelViewProjectionMatrix;\n\
    	\n\
    	\n\
    	in vec4  position;\n\
    	in vec2  texcoord;\n\
    	\n\
    	out vec2 texCoordVarying;\n\
    	\n\
    	void main()\n\
    	{\n\
        texCoordVarying = texcoord;\
        gl_Position = modelViewProjectionMatrix * position;\n\
    	}";
		string fragment = "#version 150\n\
		\n\
		uniform sampler2DRect tex0;\
		uniform sampler2DRect maskTex;\
        in vec2 texCoordVarying;\n\
		\
        out vec4 fragColor;\n\
		void main (void){\
		vec2 pos = texCoordVarying;\
		\
		vec3 src = texture(tex0, pos).rgb;\
		float mask = texture(maskTex, pos).r;\
		\
		fragColor = vec4( src , mask);\
		}";
		shader.setupShaderFromSource(GL_VERTEX_SHADER, vertex);
		shader.setupShaderFromSource(GL_FRAGMENT_SHADER, fragment);
		shader.bindDefaults();
		shader.linkProgram();
    }else{
		string shaderProgram = "#version 120\n \
		#extension GL_ARB_texture_rectangle : enable\n \
		\
		uniform sampler2DRect tex0;\
		uniform sampler2DRect maskTex;\
		\
		void main (void){\
		vec2 pos = gl_TexCoord[0].st;\
		\
		vec3 src = texture2DRect(tex0, pos).rgb;\
		float mask = texture2DRect(maskTex, pos).r;\
		\
		gl_FragColor = vec4( src , mask);\
		}";
		shader.setupShaderFromSource(GL_FRAGMENT_SHADER, shaderProgram);
		shader.linkProgram();
        cout<<"Shader Linked"<<endl;
    }
}
//---------------------------
void testApp::drawWide(){
    
    //This is for drawing a stretched camera feed across 2 monitors with texture coordinates. It takes an FBO texture and maps it onto 2 GL_Quads with texture parameters that you set during run time. This also allows you to do some bezel compensation between the two monitors so the stream looks a little more natural.
    
    
     ofEnableNormalizedTexCoords(); 
        ofSetColor(255,255,255);

        largeFbo.getTextureReference().bind();
    
        //bind the texture to 2 GL quads and give them variable texture coordinates so you can stretch and apply bezel compensation
        
         //WORKING FOR PORTRAIT
        //Left Screen
        ofPushMatrix();
        ofTranslate(0, 0);
        glBegin(GL_QUADS);
        glTexCoord2f(leftScreenXLeft,leftScreenYTop); glVertex3f(0,0,0); //TL
        glTexCoord2f(leftScreenXRight,leftScreenYTop); glVertex3f(1280,0,0); //TR
        glTexCoord2f(leftScreenXRight,leftScreenYBottom); glVertex3f(1280,720,0); //BR
        glTexCoord2f(leftScreenXLeft,leftScreenYBottom); glVertex3f(0,720,0); //BL
        glEnd();
        ofPopMatrix();
    
    
        //Right Screen
        ofPushMatrix();
        ofTranslate(1280, 0);
        glBegin(GL_QUADS);
        glTexCoord2f(rightScreenXLeft,rightScreenYTop); glVertex3f(0,0,0); //TL
        glTexCoord2f(rightScreenXRight,rightScreenYTop); glVertex3f(1280,0,0); //TR
        glTexCoord2f(rightScreenXRight,rightScreenYBottom); glVertex3f(1280,720,0); //BR
        glTexCoord2f(rightScreenXLeft,rightScreenYBottom); glVertex3f(0,720,0); //BL
        glEnd();
        ofPopMatrix();
       
    largeFbo.getTextureReference().unbind();
        
        largeFbo.begin();
        ofClear(255, 255, 255,0); //Clear the FBO after sticking it to the quads
        largeFbo.end();
    
}
//-----------------STANDARD CV---------------------------
void testApp::standardCVprocess(){
    
    //CAMERA 1------------------------------------------
        Mat camMat = toCv(cam); //use this if not rotating
        resize(camMat, cvScaleImage );
        faceFinder.update(cvScaleImage);
        
        if(faceFinder.size() > 0) {
            
            for(int i=0; i<ofClamp(faceFinder.size(), 0, maxFaces); i++){
                //Find bounding box around face
                cv::Rect roi = toCv(faceFinder.getObjectSmoothed(i)); //this ROI is scaled to cvWidth/Height
                
                //Make that box bigger - but scale to camWidth and Height to get proper sized image
                cv::Rect scaledRoi; //scale it to regular cam size
                cv::Rect nudgeRoi; //make it slightly bigger on top and bottom
                
                scaledRoi.x = ofMap(roi.x, 0, cvWidth, 0, camWidth);
                scaledRoi.y = ofMap(roi.y, 0, cvHeight, 0, camHeight);
                scaledRoi.width = ofMap(roi.width, 0, cvWidth, 0, camWidth);
                scaledRoi.height = ofMap(roi.height, 0, cvHeight, 0, camHeight);
                
                
                nudgeRoi.x = scaledRoi.x-ofMap(scaledRoi.width,0,camWidth, 0, boxSizeX*camWidth, true); //nudge it one tenth of the width of the found face to the right
                nudgeRoi.x = ofClamp(nudgeRoi.x, 0, camWidth);
                
                nudgeRoi.y = scaledRoi.y-ofMap(scaledRoi.height,0,camHeight, 0, boxSizeY*camHeight, true); //nudge it one tenth of the height of the found face to the top
                nudgeRoi.y = ofClamp(nudgeRoi.y, 0, camHeight);
                
                nudgeRoi.width = scaledRoi.width+2*ofMap(scaledRoi.width, 0, camWidth, 0, boxSizeX*camWidth, true);
                nudgeRoi.width = ofClamp(nudgeRoi.width, 0, camWidth);
                
                nudgeRoi.height = scaledRoi.height+2*ofMap(scaledRoi.height, 0, camHeight, 0, boxSizeY*camHeight, true);
                nudgeRoi.height = ofClamp(nudgeRoi.height, 0, camHeight);
                
                //make it a ofRectangle
                foundFace[i].x = nudgeRoi.x;
                foundFace[i].y = nudgeRoi.y;
                foundFace[i].width = nudgeRoi.width;
                foundFace[i].height = nudgeRoi.height;
                
                
                Mat camMat = toCv(cam); //use this if not rotating
                Mat croppedCamMat(camMat,nudgeRoi); //Todo: need to scale the roi to pick up the proper part of the larger image
                resize(croppedCamMat, cropFace[i]);
                
                cropFace[i].update();
                
                faceFbo[i].begin();
                // Cleaning everthing with alpha mask on 0 in order to make it transparent for default
                ofClear(0, 0, 0, 0);
                
                shader.begin();
                //shader.setUniformTexture("tex0", cropFace[i].getTextureReference(), 0);
                shader.setUniformTexture("maskTex", maskFbo.getTextureReference(), 1 );
                
                cropFace[i].draw(0,0);
                
                shader.end();
                faceFbo[i].end();
                
                //ToDo: autosave pictures every 30 seconds
                
            }
        }
    
}
//----------------------------
void testApp::rotatedCVprocess(){
    
    Mat camMat = toCv(cam); //turn full sized camera into a smaller mat that is set by cvWidth/height
    resize(camMat, cvScaleImage );

    //Since we're rotating, you can get weird behavior from how the iamge goes into CV. It seems to be simpler to just send it a square portion that is rotated instead of a rectangle for simplicity
    
    cv::Rect resizeRoi;
    resizeRoi.x = transCvX; //move the ROI up and down
    resizeRoi.y = 0;
    resizeRoi.width = cvHeight;
    resizeRoi.height = cvHeight; //keep it square
    
    Mat croppedSquareMat(toCv(cvScaleImage), resizeRoi); //put the scaled CV image into a square shaped CV material
        
    int rotAngle; //flip the camera up and down
    if (flopCam) {
        rotAngle = 90;
    } else{
        rotAngle = -90;
    }


    Mat rotatedCam;
    
    rotate(croppedSquareMat, rotatedCam, rotAngle); //Take resized ofImage and rotate it into a new material
    
    if(bDrawDebug){
    toOf(rotatedCam, testCapture); //update the OF texture so you can see what CV is seeing
    testCapture.update();
    }
    
    faceFinder.update(rotatedCam); //process final result 
    
    
    if(faceFinder.size() > 0) {

        for(int i=0; i<(int)ofClamp(faceFinder.size(), 0, maxFaces); i++){
            //Find bounding box around face
            cv::Rect roi = toCv(faceFinder.getObjectSmoothed(i)); //this ROI is scaled to cvWidth/Height
            
            //Make that box bigger - but scale to camWidth and Height to get proper sized image
            cv::Rect scaledRoi; //scale it to regular cam size
            cv::Rect nudgeRoi; //make it slightly bigger on top and bottom
            
            //Scale it to the camera from the resized CV - lots of weird scaling here for some project specific things - you may need to adjust this
            float scaledNudge;
            scaledNudge = ofMap(transCvX, 0, 280, 0, 560); //an additional offset for the height of the box
            scaledRoi.y = ofMap(roi.x, 0, cvHeight, 0, camHeight) +cvNudgeY; //the x moves up and down top right is 0
            scaledRoi.x = ofMap(roi.y, 0, cvHeight, camHeight+transCvY+scaledNudge, transCvY+(scaledNudge)) + ofMap(roi.width, 120, 250, cvNudgeX,-100) ; //y moves left and right, left is 360 - scale to square region - adding a width offset
            scaledRoi.width = ofMap(roi.width, 0, cvHeight, 0, camHeight);
            scaledRoi.height = ofMap(roi.height, 0, cvHeight, 0, camHeight);
            
            //Now nudge it even more to the outer edges of the face
            nudgeRoi.x = scaledRoi.x-ofMap(scaledRoi.width,0,camHeight, 0, boxSizeX*camHeight, true); //nudge it one tenth of the width of the found face to the right
            nudgeRoi.x = ofClamp(nudgeRoi.x, 0, camWidth); //keep it in range
            
            nudgeRoi.y = scaledRoi.y-ofMap(scaledRoi.width,0,camHeight, 0, boxSizeY*camHeight, true); //nudge it one tenth of the height of the found face to the top
            nudgeRoi.y = ofClamp(nudgeRoi.y, 0, camHeight);//keep it in range
            
            nudgeRoi.width = scaledRoi.width+2*ofMap(scaledRoi.width, 0, camWidth, 0, boxSizeX*camWidth, true);
            nudgeRoi.width = ofClamp(nudgeRoi.width, 0, camWidth);//keep it in range
            
            nudgeRoi.height = nudgeRoi.width; //it's square...


            //Some checks to make sure these don't go out of range of what CV expects
            if ((nudgeRoi.width + nudgeRoi.x) > camWidth) {
                nudgeRoi.width = (camWidth-nudgeRoi.x)-2;
            }
            
            if ((nudgeRoi.height + nudgeRoi.y) > camHeight) {
                nudgeRoi.height = (camHeight-nudgeRoi.y)-2;
            }
            
            //make it a ofRectangle
            foundFace[i].x = nudgeRoi.x;
            foundFace[i].y = nudgeRoi.y;
            foundFace[i].width = nudgeRoi.width;
            foundFace[i].height = nudgeRoi.width;
            
            
            Mat camMat = toCv(cam); //use this if not rotating
            
            Mat croppedCamMat(camMat, nudgeRoi); //grab the portion of the image that has the face in it
            resize(croppedCamMat, cropFace[i]);
            
            cropFace[i].update(); //store the face in an OF image
            
            faceFbo[i].begin();
            // Cleaning everthing with alpha mask on 0 in order to make it transparent for default
            ofClear(0, 0, 0, 0);
            shader.begin(); //mask the face with a feathered PNG
            shader.setUniformTexture("maskTex", maskFbo.getTextureReference(), 1 );
            cropFace[i].draw(0,0);
            shader.end();
            faceFbo[i].end();
            
        }
    }
    
}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void testApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void testApp::dragEvent(ofDragInfo dragInfo){ 

}

/*
 cout<< "ROI X: " + ofToString(roi.x) <<endl;
 cout<< "ROI Y: " + ofToString(roi.y) <<endl;
 cout<< "ROI Width: " + ofToString(roi.width) <<endl;
 cout<< "ROI Height: " + ofToString(roi.height) <<endl;
 
 cout<< "ADJUSTED ROI X: " + ofToString(nudgeRoi.x) <<endl;
 cout<< "ADJUSTED ROI Y: " + ofToString(nudgeRoi.y) <<endl;
 cout<< "ADJUSTED ROI Width: " + ofToString(nudgeRoi.width) <<endl;
 cout<< "ADJUSTED ROI Height: " + ofToString(nudgeRoi.height) <<endl;
 */
