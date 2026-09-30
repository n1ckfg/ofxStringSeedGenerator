#include "ofMain.h"
#include "ofApp.h"

//========================================================================
int main( ){

	// Keep the default GL 2.1 renderer: ofApp's glow shaders are GLSL 1.20.
	// The size is in logical pixels; ofApp::setup() adjusts it for HiDPI.
	ofGLWindowSettings settings;
	settings.setSize(WINDOW_W, WINDOW_H);
	settings.windowMode = OF_WINDOW; //can also be OF_FULLSCREEN

	auto window = ofCreateWindow(settings);

	ofRunApp(window, std::make_shared<ofApp>());
	ofRunMainLoop();

}
