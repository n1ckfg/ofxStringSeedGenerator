#pragma once

#include "ofMain.h"
#include "ofxStringSeedGenerator.h"

// Window size in logical pixels (points on HiDPI displays).
constexpr int WINDOW_W = 848;
constexpr int WINDOW_H = 1180;

// Monospace font drawn in logical coordinates but rasterized at the window's
// pixel density, so text stays sharp on HiDPI displays.
class UIFont {
public:
	void load(int sizePx, float letterSpacing, float pixelScale);
	void draw(const std::string & s, float x, float baseline) const;
	void drawGradient(const std::string & s, float x, float baseline, const ofColor & from, const ofColor & to) const;
	float width(const std::string & s) const;
	float advance() const { return charAdvance; }
	// Baseline that vertically centers a line of text in a line box.
	float baselineFor(float lineTop, float lineHeight) const;

private:
	ofTrueTypeFont ttf;
	float scale = 1;
	float charAdvance = 0;
};

struct NamedColor { std::string name; float h, s, b; };
struct NamedSize { std::string name; float r; };
struct NamedRotation { std::string name; float deg; };
struct NamedPosition { std::string name; float frac; };

struct Shape {
	std::string type;
	NamedColor color;
	float size, rotation, x, y;
	// for legend
	std::string sizeName, xName, yName;
};

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void draw() override;

	void keyPressed(ofKeyEventArgs & key) override;
	void mousePressed(int x, int y, int button) override;
	void mouseScrolled(int x, int y, float scrollX, float scrollY) override;
	void windowResized(int w, int h) override;

private:
	// ----- SSoT state -----
	void refreshFromSeed();
	void newSeed();
	void copyProvenance();

	ofxStringSeed resolver;
	std::string currentSeed;
	std::vector<Shape> shapes;  // derived from seed
	std::string provenance;

	// ----- Canvas -----
	void drawCanvas();
	void drawBackground();
	void drawGrid();
	void drawShape(const Shape & s, const ofColor & fill, const ofColor & stroke, float strokePx);
	void renderGlows();

	// Stand-in for the canvas 2D shadowBlur glow: each shape's silhouette is
	// blurred once per seed into its own layer, drawn beneath the shape.
	ofShader blurShader;
	ofFbo silhouetteFbo, blurFbo;
	std::vector<ofFbo> glowLayers;
	bool glowsDirty = true;

	// ----- Seed field -----
	void focusSeed(size_t caretPos);
	void blurSeed();
	void insertHex(const std::string & text);
	void onSeedEdited();

	std::string seedText;  // field contents; hex characters only
	size_t caret = 0;
	bool seedFocused = false;
	bool seedAllSelected = false;
	float caretBlinkStart = 0;

	// ----- Page layout -----
	struct Layout {
		float originX = 0;  // centers the page in wider windows
		ofRectangle canvas, panel, seedField, newButton, copyButton, legend, provBox;
		float seedLabelTop = 0, provLabelTop = 0;
		size_t seedCharsPerLine = 1;
		std::vector<glm::vec2> legendItems;
		std::vector<std::string> provLines;  // provenance, wrapped to the box
		size_t provVisibleLines = 1;
	} layout;

	void updatePixelScale();
	void updateLayout();
	void drawPanel();
	glm::vec2 toPage(float x, float y) const;
	std::string legendLabel(size_t i) const;

	float pixelScale = 0;  // framebuffer pixels per logical pixel
	float provScroll = 0;  // in lines
	float copiedUntil = 0;

	UIFont titleFont, subtitleFont, labelFont, seedFont, buttonFont, smallFont, canvasFont;
};
