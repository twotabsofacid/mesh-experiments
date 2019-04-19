#include "ofApp.h"

#define LINE_SIZE 50
#define ROW_SIZE 250
#define FRAMERATE 24

//--------------------------------------------------------------
void ofApp::setup(){
	ofBackground(0);
	ofSetFrameRate(FRAMERATE);
	gui.setup();
	gui.add(noiseAmp.set("Noise Amp", 100.0, 0.0, 200.0));
	gui.add(noiseScale.set("Noise Scale", 0.1, 0.0, 10.0));
	gui.add(lineFrequency.set("Line Frequency", 10, 4, 100));
	gui.add(frameMultiplier.set("Frame Multiplier", 0.5, 0.0, 2.0));
	gui.add(noiseMultiplier.set("Noise Multiplier", 5.0, 0.0, 10.0));
	gui.add(randomRange.set("Random Range", 560.0, 0.0, 1200.0));
	gui.add(colorNear.set("Color Near", ofColor(101, 114, 235), ofColor(0,0,0), ofColor(255,255,255)));
	gui.add(colorFar.set("Color Far", ofColor(203, 255, 181), ofColor(0,0,0), ofColor(255,255,255)));
	float width = ofGetWidth();
	float height = ofGetHeight();
	valueIncrementer = (ROW_SIZE/LINE_SIZE) * (ROW_SIZE/LINE_SIZE);
	// setup the rtl sdr dongle
	int rowsColsVal = ROW_SIZE;
	cout << "HOW BIG IS OUR MESH? " << rowsColsVal << endl;
	// create the mesh
	for (int c = 0; c<rowsColsVal; c++){
	    for (int r = 0; r<rowsColsVal; r++){

	        glm::vec3 pos;      // grid centered at 0,0,0

	        float halfWidth     = width * 0.5;
	        float halfHeight    = height * 0.5;

	        pos.x = ofMap(r, 0, rowsColsVal-1, -halfWidth, halfWidth);
	        pos.y = ofMap(c, 0, rowsColsVal-1, halfHeight, -halfHeight);    // Y+ is up in 3D!
	        pos.z = 0;    // add depth later

	        // add the point to the mesh
	        mesh.addVertex(pos);

	        // add a color for the point
	        mesh.addColor(ofColor());

	        if (r > 0 && c > 0) {
	            int index = r * rowsColsVal + c;
	            // triangle 1
	            mesh.addIndex(index);               // A    - this pt
	            mesh.addIndex(index - 1);           // B    - + col
	            mesh.addIndex(index - rowsColsVal);        // C    - + row

	            // triangle 2
	            mesh.addIndex(index - 1);           // B
	            mesh.addIndex(index - 1 - rowsColsVal);    // D
	            mesh.addIndex(index - rowsColsVal);        // C
	        }
	    }
	}
	// Load shaders
	// shader.load("shader.vert", "shader.frag");
}

//--------------------------------------------------------------
void ofApp::update(){
	// Create LINE_SIZE random points
	if (ofGetFrameNum() % lineFrequency == 0) {
		for (int i = 0; i < LINE_SIZE; i++) {
			float ran = ofRandom(-randomRange/2.0, randomRange/2.0);
			ekgLines.push_back(ran);
			ekgLinesSaved.push_back(ran);
		}
	} else {
		for (int i = 0; i < LINE_SIZE; i++) {
			ekgLines.push_back(0.0);
			ekgLinesSaved.push_back(0.0);
		}
	}
	// Delete from the front of the vector if we're
	// larger than the mesh
	if (ekgLines.size() > LINE_SIZE * LINE_SIZE) {
		for (int i = 0; i < LINE_SIZE; i++) {
			ekgLines.erase(ekgLines.begin() + i);
			ekgLinesSaved.erase(ekgLinesSaved.begin() + i);
		}
	}
	// Give em all some noise
	for (int i = 0; i < ekgLines.size(); i++) {
		float signedNoise = ofSignedNoise(i * noiseScale, ofGetFrameNum() * frameMultiplier) * noiseMultiplier;
		ekgLines[i] += signedNoise;
		ekgLinesSaved[i] += signedNoise;
	}
	// Animate the random high/low points to swing up towards the center,
	// and back down towards the end
	for (int i = 0; i < mesh.getVertices().size(); i++) {
		if (i % valueIncrementer == 0 && i/valueIncrementer < ekgLines.size()) {
			int row = (int)i/ROW_SIZE;
			float lerpValue = ofMap(abs((float)row - (float)ROW_SIZE/2.0), 0.0, (float)ROW_SIZE/2.0, 0.0, 1.0);
			ekgLines[i/valueIncrementer] = ofLerp(ekgLinesSaved[i/valueIncrementer], 0.0, lerpValue);
		}
	}
	// Update the Z values and the colors
	updateZValue();
	updateColors();
}

//--------------------------------------------------------------
void ofApp::draw(){
	cam.begin();
    ofEnableDepthTest();
    mesh.draw();
    ofDisableDepthTest();
    cam.end();
    gui.draw();
}

//--------------------------------------------------------------
void ofApp::updateZValue(){
	// Here we ease the values in each line with each other,
	// picking the high/low points and then easing those around them
	// into the next high/low point
	for (int i = 0; i < mesh.getVertices().size(); i++) {
		glm::vec3& vertex = mesh.getVertices()[i];
		vertex.z = 0.0;
		// Every X points in the line, we use the value from ekgLines
		// as the z vertex for that point (where X is the valueIncrementer)
		if (i % valueIncrementer == 0 && i/valueIncrementer < ekgLines.size()) {
			vertex.z = ekgLines[i/valueIncrementer];
		} else { // Otherwise, we ease the values around that high/low point into the next one
			if (ceil(i/(float)valueIncrementer) <= ekgLines.size()) {
				float easedValue = easeInOutQuad((i % valueIncrementer)/(float)valueIncrementer);
				vertex.z = ofMap(easedValue, 0.0, 1.0, ekgLines[floor(i/(float)valueIncrementer)], ekgLines[ceil(i/(float)valueIncrementer)]);
			}
		}
	}
	// Here we want to ease the values from one line to the next,
	// which we do by getting the offset of the lines that have
	// meaningful data, ignoring those lines, and easing all values
	// in other lines to those lines
	//
	// THIS WHOLE THING IS EXTREMELY FUCKING BROKEN
	int valueLineOffset = ofGetFrameNum() % lineFrequency;
	int verticesSize = mesh.getVertices().size();
	for (int i = 0; i < verticesSize; i++) {
		int lineNumber = (int)i/ROW_SIZE;
		int easeDuration = (int)lineFrequency/2.0;
		int linePositionBetween = (lineNumber - valueLineOffset) % easeDuration;
		if (linePositionBetween != 0) {
			float easedValue = easeInOutQuad(linePositionBetween/easeDuration);
			int startEase = i - (ROW_SIZE * linePositionBetween);
			int endEase = i + (ROW_SIZE * (easeDuration - linePositionBetween));
			// This is not working, idk why
			// It's supposed to ease the value between the value
			// of the startEase and endEase
			if (startEase >= 0 && endEase >= 0 && startEase <= verticesSize && endEase <= verticesSize) {
				glm::vec3& newVertex = mesh.getVertices()[i];
				newVertex.z = ofMap(easedValue, 0.0, 1.0, mesh.getVertices()[startEase].z, mesh.getVertices()[endEase].z);
			}
		}
	}
}

// --------------------------------
void ofApp::updateColors(){

    // map colors based on vertex z / depth

    for (int i=0; i<mesh.getVertices().size(); i++){

        // 1 color per vertex
        glm::vec3& vertex = mesh.getVertices()[i];

        // get depth as percent of noise range
        float depthPercent = ofMap(vertex.z, -noiseAmp, noiseAmp, 0, 1, true);    // map 0-1

        // lerp color
        ofColor color = colorFar.get().getLerped( colorNear.get(), depthPercent );

        mesh.setColor(i, color);        // set mesh color
    }

}

// --------------------------------
// t = value between [0, 1] to add ease to
// https://github.com/jesusgollonet/ofpennereasing/blob/master/PennerEasing/Quad.cpp
float ofApp::easeInOutQuad(float t) {
	if (t < 0.5) {
		return (2.0 * t * t);
	} else {
		return (-1.0 + (4.0 - 2.0 * t) * t);
	}

}
