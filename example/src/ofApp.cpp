#include "ofApp.h"

#include <ctime>

// ============================================================================
//  Configuration
// ============================================================================

namespace {

constexpr float CANVAS_W = 800;
constexpr float CANVAS_H = 480;
constexpr size_t NUM_SHAPES = 5;
constexpr size_t AXES_PER_SHAPE = 6;
constexpr size_t SEED_BYTES = (NUM_SHAPES * AXES_PER_SHAPE * 4 + 1) / 2; // 60 bytes

// ----- Axis candidate lists -----

const std::vector<std::string> SHAPE_TYPES = {
	"circle", "square", "triangle", "diamond",
	"pentagon", "hexagon", "star", "cross"
};

const std::vector<NamedColor> COLORS = {
	{ "red",    0,   80, 100 },
	{ "orange", 28,  85, 100 },
	{ "gold",   48,  80, 100 },
	{ "green",  140, 65, 85  },
	{ "teal",   175, 65, 85  },
	{ "blue",   220, 70, 95  },
	{ "indigo", 255, 60, 90  },
	{ "purple", 280, 60, 90  },
	{ "pink",   335, 55, 95  },
	{ "white",  0,   0,  100 },
};

const std::vector<NamedSize> SIZES = {
	{ "tiny",   22  },
	{ "small",  36  },
	{ "medium", 52  },
	{ "large",  72  },
	{ "big",    90  },
	{ "huge",   112 },
};

const std::vector<NamedRotation> ROTATIONS = {
	{ "0°",   0   },
	{ "45°",  45  },
	{ "90°",  90  },
	{ "135°", 135 },
	{ "180°", 180 },
	{ "225°", 225 },
	{ "270°", 270 },
	{ "315°", 315 },
};

// Positions as fractions of canvas — 7 evenly spaced zones.
const std::vector<NamedPosition> X_POSITIONS = {
	{ "far-left",     1 / 8.f },
	{ "left",         2 / 8.f },
	{ "center-left",  3 / 8.f },
	{ "center",       4 / 8.f },
	{ "center-right", 5 / 8.f },
	{ "right",        6 / 8.f },
	{ "far-right",    7 / 8.f },
};

const std::vector<NamedPosition> Y_POSITIONS = {
	{ "top",          1 / 8.f },
	{ "upper",        2 / 8.f },
	{ "mid-upper",    3 / 8.f },
	{ "center",       4 / 8.f },
	{ "mid-lower",    5 / 8.f },
	{ "lower",        6 / 8.f },
	{ "bottom",       7 / 8.f },
};

// ----- Page style (from the original page's CSS) -----

const ofColor BG       = ofColor::fromHex(0x0f0f1a);
const ofColor PANEL_BG = ofColor::fromHex(0x1a1a2e);
const ofColor BORDER   = ofColor::fromHex(0x2d2d4a);
const ofColor TEXT     = ofColor::fromHex(0xe0e0f0);
const ofColor DIM      = ofColor::fromHex(0x7878a0);
const ofColor ACCENT   = ofColor::fromHex(0x6c63ff);
const ofColor ACCENT2  = ofColor::fromHex(0xff6584);
const ofColor VIOLET   = ofColor::fromHex(0x8b5cf6);

constexpr float PAGE_PAD = 24;
constexpr float PANEL_PAD_X = 24;
constexpr float PANEL_PAD_Y = 20;
constexpr float TITLE_LH = 27;
constexpr float SUBTITLE_LH = 15;
constexpr float LABEL_LH = 13;
constexpr float SEED_LH = 16;
constexpr float LEGEND_LH = 14;
constexpr float PROV_LH = 16.5f;

// Canvas 2D shadowBlur of the glow; its Gaussian sigma is half this.
constexpr float GLOW_BLUR = 18;

// GLSL 1.20 to match the default GL 2.1 renderer (see main.cpp).
const std::string BLUR_VERT = R"(#version 120
void main() {
	gl_TexCoord[0] = gl_MultiTexCoord0;
	gl_Position = ftransform();
}
)";

// Separable Gaussian blur over the red channel, output as `tint`-colored
// alpha. The kernel radius of 27 taps covers 3 sigma for GLOW_BLUR = 18.
const std::string BLUR_FRAG = R"(#version 120
#extension GL_ARB_texture_rectangle : enable
uniform sampler2DRect tex0;
uniform vec2 direction;
uniform float sigma;
uniform vec4 tint;
void main() {
	vec2 st = gl_TexCoord[0].st;
	float sum = 0.0;
	float total = 0.0;
	for (int i = -27; i <= 27; i++) {
		float w = exp(-0.5 * float(i * i) / (sigma * sigma));
		sum += texture2DRect(tex0, st + direction * float(i)).r * w;
		total += w;
	}
	gl_FragColor = vec4(tint.rgb, tint.a * sum / total);
}
)";

// Equivalent of p5's colorMode(HSB, 360, 100, 100, 100).
ofColor hsb(float h, float s, float b, float a = 100) {
	return ofFloatColor::fromHsb(h / 360.f, s / 100.f, b / 100.f, a / 100.f);
}

template <typename T>
std::vector<std::string> names(const std::vector<T> & items) {
	std::vector<std::string> out;
	for (const auto & item : items) out.push_back(item.name);
	return out;
}

std::string todayUtc() {
	const std::time_t now = std::time(nullptr);
	char buf[11];
	std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::gmtime(&now));
	return buf;
}

// Hard-wrap each line at `width` code points, like CSS `word-break: break-all`.
std::vector<std::string> wrapLines(std::string text, size_t width) {
	if (!text.empty() && text.back() == '\n') text.pop_back();
	std::vector<std::string> out;
	for (const std::string & line : ofSplitString(text, "\n")) {
		std::string current;
		size_t count = 0;
		for (char c : line) {
			const bool leadByte = (static_cast<unsigned char>(c) & 0xC0) != 0x80;
			if (leadByte && count == width) {
				out.push_back(current);
				current.clear();
				count = 0;
			}
			current += c;
			if (leadByte) count++;
		}
		out.push_back(current);
	}
	return out;
}

void drawBox(const ofRectangle & r, float radius, const ofColor & fill, const ofColor & border, float pixelScale) {
	ofFill();
	ofSetColor(fill);
	ofDrawRectRounded(r, radius);
	ofSetLineWidth(pixelScale);  // 1 logical px
	ofNoFill();
	ofSetColor(border);
	ofDrawRectRounded(r, radius);
	ofFill();
}

void drawGradientBox(const ofRectangle & r, float radius, const ofColor & from, const ofColor & to) {
	ofPath path;
	path.rectRounded(r, radius);
	ofMesh mesh = path.getTessellation();
	for (const auto & v : mesh.getVertices()) {
		mesh.addColor(from.getLerped(to, ofMap(v.x, r.getLeft(), r.getRight(), 0, 1, true)));
	}
	mesh.draw();
}

bool isKey(const ofKeyEventArgs & e, char c) {
	return (e.key < 128 && std::tolower(e.key) == c) || e.keycode == std::toupper(c);
}

} // namespace

// ============================================================================
//  UIFont
// ============================================================================

void UIFont::load(int sizePx, float letterSpacing, float pixelScale) {
	scale = pixelScale;
	ofTrueTypeFontSettings settings(OF_TTF_MONO, sizePx);
	settings.dpi = 72 * pixelScale;  // 1pt == 1 logical px, rasterized at device resolution
	settings.antialiased = true;
	settings.addRanges({ ofUnicode::Latin1Supplement, ofUnicode::GeneralPunctuation });
	ttf.load(settings);
	ttf.setLetterSpacing(letterSpacing);
	charAdvance = (ttf.stringWidth(std::string(200, '0')) - ttf.stringWidth(std::string(100, '0'))) / 100.f / scale;
}

void UIFont::draw(const std::string & s, float x, float baseline) const {
	ofPushMatrix();
	// Snap to the device pixel grid so glyphs aren't resampled.
	ofTranslate(std::round(x * scale) / scale, std::round(baseline * scale) / scale);
	ofScale(1 / scale);
	ttf.drawString(s, 0, 0);
	ofPopMatrix();
}

void UIFont::drawGradient(const std::string & s, float x, float baseline, const ofColor & from, const ofColor & to) const {
	ofMesh mesh = ttf.getStringMesh(s, 0, 0);
	if (mesh.getNumVertices() == 0) return;

	float minX = mesh.getVertex(0).x, maxX = minX;
	for (const auto & v : mesh.getVertices()) {
		minX = std::min(minX, v.x);
		maxX = std::max(maxX, v.x);
	}
	mesh.clearColors();
	for (const auto & v : mesh.getVertices()) {
		mesh.addColor(from.getLerped(to, ofMap(v.x, minX, maxX, 0, 1)));
	}

	ofPushMatrix();
	ofTranslate(std::round(x * scale) / scale, std::round(baseline * scale) / scale);
	ofScale(1 / scale);
	ttf.getFontTexture().bind();
	mesh.draw();
	ttf.getFontTexture().unbind();
	ofPopMatrix();
}

float UIFont::width(const std::string & s) const {
	return ttf.stringWidth(s) / scale;
}

float UIFont::baselineFor(float lineTop, float lineHeight) const {
	const float ascender = ttf.getAscenderHeight() / scale;
	const float descender = ttf.getDescenderHeight() / scale;  // negative
	return lineTop + (lineHeight + ascender + descender) / 2;
}

// ============================================================================
//  Setup
// ============================================================================

void ofApp::setup() {
	ofSetWindowTitle("String Seed Generator — SSoT Visual Demo");
	ofSetFrameRate(60);
	ofSetVerticalSync(true);
	ofSetBackgroundColor(BG);
	ofSetCircleResolution(72);
	ofSetEscapeQuitsApp(false);  // Esc leaves the seed field first

	// OF measures windows in framebuffer pixels, so on HiDPI displays the
	// window opens at half size; restore it to WINDOW_W x WINDOW_H points,
	// shortened to fit the screen.
	updatePixelScale();
	const float maxH = ofGetScreenHeight() / pixelScale - 100;
	ofSetWindowShape(WINDOW_W * pixelScale, std::max(600.f, std::min<float>(WINDOW_H, maxH)) * pixelScale);

	blurShader.setupShaderFromSource(GL_VERTEX_SHADER, BLUR_VERT);
	blurShader.setupShaderFromSource(GL_FRAGMENT_SHADER, BLUR_FRAG);
	blurShader.linkProgram();

	silhouetteFbo.allocate(CANVAS_W, CANVAS_H, GL_RGBA32F_ARB);
	blurFbo.allocate(CANVAS_W, CANVAS_H, GL_RGBA32F_ARB);
	glowLayers.resize(NUM_SHAPES);
	for (auto & layer : glowLayers) {
		layer.allocate(CANVAS_W, CANVAS_H, GL_RGBA);
	}

	// ----- Build the StringSeed resolver -----
	for (size_t i = 0; i < NUM_SHAPES; i++) {
		const std::string label = "shape-" + ofToString(i + 1);
		resolver.addAxis(label + " type",     SHAPE_TYPES);
		resolver.addAxis(label + " color",    names(COLORS));
		resolver.addAxis(label + " size",     names(SIZES));
		resolver.addAxis(label + " rotation", names(ROTATIONS));
		resolver.addAxis(label + " x-pos",    names(X_POSITIONS));
		resolver.addAxis(label + " y-pos",    names(Y_POSITIONS));
	}

	currentSeed = ofxStringSeed::generateSeed(SEED_BYTES);
	refreshFromSeed();
}

void ofApp::updatePixelScale() {
	float scale = 1;
	if (auto glfw = dynamic_cast<ofAppGLFWWindow *>(ofGetWindowPtr())) {
		scale = std::max(1, glfw->getPixelScreenCoordScale());
	}
	if (scale == pixelScale) return;

	pixelScale = scale;
	titleFont.load(22, 1.07f, pixelScale);
	subtitleFont.load(12, 1.1f, pixelScale);
	labelFont.load(10, 1.2f, pixelScale);
	seedFont.load(13, 1.13f, pixelScale);
	buttonFont.load(12, 1.07f, pixelScale);
	smallFont.load(11, 1, pixelScale);
	canvasFont.load(10, 1, pixelScale);
}

void ofApp::windowResized(int w, int h) {
	updatePixelScale();
}

// ============================================================================
//  State
// ============================================================================

void ofApp::refreshFromSeed() {
	const auto results = resolver.resolve(currentSeed);

	shapes.clear();
	for (size_t i = 0; i < NUM_SHAPES; i++) {
		const size_t base = i * AXES_PER_SHAPE;
		const auto & size = SIZES[results[base + 2].index];
		const auto & xPos = X_POSITIONS[results[base + 4].index];
		const auto & yPos = Y_POSITIONS[results[base + 5].index];

		Shape s;
		s.type     = SHAPE_TYPES[results[base + 0].index];
		s.color    = COLORS[results[base + 1].index];
		s.size     = size.r;
		s.rotation = ROTATIONS[results[base + 3].index].deg;
		s.x        = xPos.frac * CANVAS_W;
		s.y        = yPos.frac * CANVAS_H;
		s.sizeName = size.name;
		s.xName    = xPos.name;
		s.yName    = yPos.name;
		shapes.push_back(s);
	}

	// Provenance
	ofxStringSeed::Meta meta;
	meta.piece = "visual demo";
	meta.date = todayUtc();
	provenance = ofxStringSeed::groupedProvenance(currentSeed, results, AXES_PER_SHAPE,
		[this](size_t g) { return "Shape " + ofToString(g + 1) + " (" + (g < shapes.size() ? shapes[g].type : "?") + ")"; },
		meta);

	glowsDirty = true;

	// Sync field
	if (!seedFocused) {
		seedText = currentSeed;
		caret = seedText.size();
	}
}

void ofApp::newSeed() {
	currentSeed = ofxStringSeed::generateSeed(SEED_BYTES);
	blurSeed();
	refreshFromSeed();
}

void ofApp::copyProvenance() {
	ofSetClipboardString(provenance);
	copiedUntil = ofGetElapsedTimef() + 1.5f;
}

// ============================================================================
//  Draw
// ============================================================================

void ofApp::draw() {
	if (glowsDirty) renderGlows();
	updateLayout();

	ofPushMatrix();
	ofScale(pixelScale);
	ofTranslate(layout.originX, 0);

	// ----- Canvas -----
	ofPushMatrix();
	ofTranslate(layout.canvas.getPosition());
	drawCanvas();
	ofPopMatrix();

	// Clip the canvas to its rounded frame by painting the page around it.
	ofPath mask;
	mask.rectangle(-layout.originX, 0, ofGetWidth() / pixelScale, ofGetHeight() / pixelScale);
	mask.rectRounded(layout.canvas, 12);
	mask.setPolyWindingMode(OF_POLY_WINDING_ODD);
	mask.setFillColor(BG);
	mask.draw();

	ofSetLineWidth(pixelScale);
	ofNoFill();
	ofSetColor(BORDER);
	ofDrawRectRounded(layout.canvas, 12);
	ofFill();

	// ----- Header -----
	const std::string title = "String Seed Generator";
	const std::string subtitle = "SSoT — String Seed of Thought · Visual Demo";
	titleFont.drawGradient(title, (WINDOW_W - titleFont.width(title)) / 2,
		titleFont.baselineFor(PAGE_PAD, TITLE_LH), ACCENT, ACCENT2);
	ofSetColor(DIM);
	subtitleFont.draw(subtitle, (WINDOW_W - subtitleFont.width(subtitle)) / 2,
		subtitleFont.baselineFor(PAGE_PAD + TITLE_LH + 4, SUBTITLE_LH));

	drawPanel();

	ofPopMatrix();
}

void ofApp::drawPanel() {
	const glm::vec2 mouse = toPage(ofGetMouseX(), ofGetMouseY());
	const ofRectangle & f = layout.seedField;

	drawBox(layout.panel, 12, PANEL_BG, BORDER, pixelScale);

	// ----- Seed row -----
	ofSetColor(DIM);
	labelFont.draw("HEX SEED — EDIT TO UPDATE SHAPES", f.x, labelFont.baselineFor(layout.seedLabelTop, LABEL_LH));
	const std::string hint = seedFocused ? "return: done · cmd/ctrl+a, v" : "n: new · c: copy · scroll box";
	labelFont.draw(hint, layout.copyButton.getRight() - labelFont.width(hint), labelFont.baselineFor(layout.seedLabelTop, LABEL_LH));

	drawBox(f, 8, ofColor(0, 89), seedFocused ? ACCENT : BORDER, pixelScale);

	const size_t cpl = layout.seedCharsPerLine;
	const float tx = f.x + 14;
	const float ty = f.y + 10;
	const size_t rows = std::max<size_t>(1, (seedText.size() + cpl - 1) / cpl);
	for (size_t row = 0; row < rows; row++) {
		const std::string line = seedText.substr(std::min(row * cpl, seedText.size()), cpl);
		if (seedAllSelected) {
			ofSetColor(ACCENT, 110);
			ofDrawRectangle(tx, ty + row * SEED_LH, line.size() * seedFont.advance(), SEED_LH);
		}
		ofSetColor(TEXT);
		seedFont.draw(line, tx, seedFont.baselineFor(ty + row * SEED_LH, SEED_LH));
	}
	if (seedFocused && !seedAllSelected && std::fmod(ofGetElapsedTimef() - caretBlinkStart, 1.f) < 0.5f) {
		const size_t row = std::min(caret / cpl, rows - 1);
		// Snapped to a whole device pixel so the 1px caret stays crisp.
		const float cx = std::round((tx + (caret - row * cpl) * seedFont.advance() - 1) * pixelScale) / pixelScale;
		ofSetColor(TEXT);
		ofDrawRectangle(cx, ty + row * SEED_LH + 1, 1, SEED_LH - 2);
	}

	// Buttons
	const ofRectangle & nb = layout.newButton;
	const bool newHover = nb.inside(mouse);
	drawGradientBox(nb, 8, newHover ? ACCENT.getLerped(ofColor::white, 0.12f) : ACCENT, newHover ? VIOLET.getLerped(ofColor::white, 0.12f) : VIOLET);
	ofSetColor(ofColor::white);
	buttonFont.draw("New", nb.getCenter().x - buttonFont.width("New") / 2, buttonFont.baselineFor(nb.y, nb.height));

	const ofRectangle & cb = layout.copyButton;
	const bool copyHover = cb.inside(mouse);
	const std::string copyLabel = ofGetElapsedTimef() < copiedUntil ? "Copied" : "Copy";
	drawBox(cb, 8, ofColor(255, 15), copyHover ? ACCENT : BORDER, pixelScale);
	ofSetColor(copyHover ? TEXT : DIM);
	buttonFont.draw(copyLabel, cb.getCenter().x - buttonFont.width(copyLabel) / 2, buttonFont.baselineFor(cb.y, cb.height));

	// ----- Legend -----
	ofSetColor(0, 51);
	ofDrawRectRounded(layout.legend, 8);
	for (size_t i = 0; i < shapes.size(); i++) {
		const glm::vec2 & p = layout.legendItems[i];
		const auto & c = shapes[i].color;
		drawBox({ p.x, p.y + (LEGEND_LH - 12) / 2, 12, 12 }, 3, hsb(c.h, c.s, c.b), ofColor(255, 26), pixelScale);
		ofSetColor(DIM);
		smallFont.draw(legendLabel(i), p.x + 18, smallFont.baselineFor(p.y, LEGEND_LH));
	}

	// ----- Provenance -----
	const ofRectangle & box = layout.provBox;
	ofSetColor(DIM);
	labelFont.draw("PROVENANCE BLOCK — VERIFIABLE ARITHMETIC", box.x, labelFont.baselineFor(layout.provLabelTop, LABEL_LH));

	drawBox(box, 8, ofColor(0, 77), BORDER, pixelScale);

	const size_t total = layout.provLines.size();
	const size_t visible = layout.provVisibleLines;
	const size_t first = static_cast<size_t>(provScroll);
	ofSetColor(DIM);
	for (size_t i = 0; i < visible && first + i < total; i++) {
		smallFont.draw(layout.provLines[first + i], box.x + 16, smallFont.baselineFor(box.y + 14 + i * PROV_LH, PROV_LH));
	}

	// Scrollbar
	if (total > visible) {
		const float trackH = box.height - 12;
		const float thumbH = std::max(20.f, trackH * visible / total);
		const float thumbY = box.y + 6 + (trackH - thumbH) * provScroll / (total - visible);
		ofSetColor(BORDER);
		ofDrawRectRounded(box.getRight() - 10, thumbY, 6, thumbH, 3);
	}
}

// ============================================================================
//  Canvas
// ============================================================================

void ofApp::drawCanvas() {
	// Dark gradient background
	drawBackground();

	// Subtle grid
	drawGrid();

	// Shapes, each over its own glow
	for (size_t i = 0; i < shapes.size(); i++) {
		const Shape & s = shapes[i];
		ofSetColor(255);
		glowLayers[i].draw(0, 0, CANVAS_W, CANVAS_H);
		drawShape(s,
			hsb(s.color.h, s.color.s, s.color.b, 80),
			// Stroke — lighter version
			hsb(s.color.h, std::max(0.f, s.color.s - 30), std::min(100.f, s.color.b + 10), 50),
			2 * pixelScale);
	}

	// Shape index labels
	ofSetColor(hsb(0, 0, 100, 40));
	for (size_t i = 0; i < shapes.size(); i++) {
		const Shape & s = shapes[i];
		const std::string label = ofToString(i + 1);
		canvasFont.draw(label, s.x - canvasFont.width(label) / 2,
			canvasFont.baselineFor(s.y - s.size / 2 - 14 - LEGEND_LH / 2, LEGEND_LH));
	}
}

void ofApp::drawBackground() {
	ofFill();
	ofSetColor(hsb(230, 30, 8));
	ofDrawRectangle(0, 0, CANVAS_W, CANVAS_H);

	// Subtle vignette
	const float cx = CANVAS_W / 2, cy = CANVAS_H / 2;
	for (float r = 400; r > 0; r -= 20) {
		ofSetColor(hsb(250, 40, 18, ofMap(r, 0, 400, 0, 6)));
		ofDrawEllipse(cx, cy, r * 2.5f, r * 2);
	}
}

void ofApp::drawGrid() {
	// The original uses 0.5px lines. GL lines can't be thinner than one
	// device pixel, so thinner lines are faded to keep the same weight.
	const float px = 0.5f * pixelScale;
	ofSetColor(hsb(240, 20, 30, 15 * std::min(1.f, px)));
	ofSetLineWidth(std::max(1.f, px));
	for (float x = CANVAS_W / 8; x < CANVAS_W; x += CANVAS_W / 8) {
		ofDrawLine(x, 0, x, CANVAS_H);
	}
	for (float y = CANVAS_H / 8; y < CANVAS_H; y += CANVAS_H / 8) {
		ofDrawLine(0, y, CANVAS_W, y);
	}
}

void ofApp::drawShape(const Shape & s, const ofColor & fill, const ofColor & stroke, float strokePx) {
	// Like p5, each primitive is filled and then stroked before the next.
	auto styled = [&](const std::function<void()> & primitive) {
		ofFill();
		ofSetColor(fill);
		primitive();
		ofSetLineWidth(strokePx);  // before ofNoFill(), which reads it
		ofNoFill();
		ofSetColor(stroke);
		primitive();
		ofFill();
	};

	auto polygon = [](const std::vector<glm::vec2> & points) {
		ofBeginShape();
		for (const auto & p : points) ofVertex(p);
		ofEndShape(true);
	};

	ofPushMatrix();
	ofTranslate(s.x, s.y);
	ofRotateDeg(s.rotation);

	const float r = s.size / 2;

	if (s.type == "circle") {
		styled([&] { ofDrawCircle(0, 0, r); });

	} else if (s.type == "square") {
		styled([&] { ofDrawRectRounded(-r, -r, s.size, s.size, 4); });

	} else if (s.type == "triangle") {
		const float h = s.size * 0.866f;
		styled([&] { ofDrawTriangle(-r, h / 2, r, h / 2, 0, -h / 2); });

	} else if (s.type == "diamond") {
		styled([&] { polygon({ { 0, -r }, { r, 0 }, { 0, r }, { -r, 0 } }); });

	} else if (s.type == "pentagon" || s.type == "hexagon") {
		const int sides = s.type == "pentagon" ? 5 : 6;
		std::vector<glm::vec2> points;
		for (int i = 0; i < sides; i++) {
			const float angle = TWO_PI / sides * i - HALF_PI;
			points.push_back({ std::cos(angle) * r, std::sin(angle) * r });
		}
		styled([&] { polygon(points); });

	} else if (s.type == "star") {
		const int tips = 5;
		std::vector<glm::vec2> points;
		for (int i = 0; i < tips * 2; i++) {
			const float angle = TWO_PI / (tips * 2) * i - HALF_PI;
			const float pr = i % 2 == 0 ? r : r * 0.45f;
			points.push_back({ std::cos(angle) * pr, std::sin(angle) * pr });
		}
		styled([&] { polygon(points); });

	} else if (s.type == "cross") {
		const float arm = s.size * 0.3f;
		styled([&] { ofDrawRectRounded(-arm / 2, -r, arm, s.size, 3); });
		styled([&] { ofDrawRectRounded(-r, -arm / 2, s.size, arm, 3); });
	}

	ofPopMatrix();
}

void ofApp::renderGlows() {
	for (size_t i = 0; i < shapes.size(); i++) {
		const Shape & s = shapes[i];

		// 1. Silhouette, with the shape's own fill and stroke alpha. Drawing
		//    white over black makes the red channel accumulate coverage the
		//    same way alpha does on the real canvas.
		silhouetteFbo.begin();
		ofClear(0, 0, 0, 0);
		drawShape(s, ofColor(255, 255 * 0.8f), ofColor(255, 255 * 0.5f), 2);
		silhouetteFbo.end();

		// 2. Horizontal blur, keeping coverage in the red channel.
		blurFbo.begin();
		ofClear(0, 0, 0, 0);
		blurShader.begin();
		blurShader.setUniformTexture("tex0", silhouetteFbo.getTexture(), 0);
		blurShader.setUniform2f("direction", 1, 0);
		blurShader.setUniform1f("sigma", GLOW_BLUR / 2);
		blurShader.setUniform4f("tint", 1, 1, 1, 1);
		silhouetteFbo.draw(0, 0);
		blurShader.end();
		blurFbo.end();

		// 3. Vertical blur in the shadow color (60% alpha, as in the original),
		//    written without blending so the layer keeps straight alpha.
		const ofFloatColor shadow = ofFloatColor::fromHsb(s.color.h / 360.f, s.color.s / 100.f, s.color.b / 100.f);
		glowLayers[i].begin();
		ofClear(0, 0, 0, 0);
		ofDisableAlphaBlending();
		blurShader.begin();
		blurShader.setUniformTexture("tex0", blurFbo.getTexture(), 0);
		blurShader.setUniform2f("direction", 0, 1);
		blurShader.setUniform1f("sigma", GLOW_BLUR / 2);
		blurShader.setUniform4f("tint", shadow.r, shadow.g, shadow.b, 0.6f);
		blurFbo.draw(0, 0);
		blurShader.end();
		ofEnableAlphaBlending();
		glowLayers[i].end();
	}
	glowsDirty = false;
}

// ============================================================================
//  Layout
// ============================================================================

void ofApp::updateLayout() {
	const float winW = ofGetWidth() / pixelScale;
	const float winH = ofGetHeight() / pixelScale;
	layout.originX = std::max(0.f, std::round((winW - WINDOW_W) / 2));

	float y = PAGE_PAD + TITLE_LH + 4 + SUBTITLE_LH + 20;
	layout.canvas.set(PAGE_PAD, y, CANVAS_W, CANVAS_H);
	y += CANVAS_H + 20;

	// ----- Info panel -----
	const float panelTop = y;
	const float ix = PAGE_PAD + PANEL_PAD_X;
	const float iw = CANVAS_W - PANEL_PAD_X * 2;
	y += PANEL_PAD_Y;

	layout.seedLabelTop = y;
	y += LABEL_LH + 6;

	const float newW = buttonFont.width("New") + 32;
	const float copyW = buttonFont.width("Copied") + 32;
	const float fieldW = iw - newW - copyW - 16;
	const size_t cpl = std::max(1, static_cast<int>((fieldW - 28) / seedFont.advance()));
	const size_t rows = std::max<size_t>(1, (seedText.size() + cpl - 1) / cpl);
	const float rowH = rows * SEED_LH + 20;
	layout.seedCharsPerLine = cpl;
	layout.seedField.set(ix, y, fieldW, rowH);
	layout.newButton.set(ix + fieldW + 8, y, newW, rowH);
	layout.copyButton.set(ix + fieldW + 8 + newW + 8, y, copyW, rowH);
	y += rowH + 16;

	// Legend items wrap like a flex row.
	layout.legendItems.clear();
	const float left = ix + 14;
	const float right = ix + iw - 14;
	float lx = left;
	float ly = y + 10;
	for (size_t i = 0; i < shapes.size(); i++) {
		const float itemW = 18 + smallFont.width(legendLabel(i));
		if (lx > left && lx + itemW > right) {
			lx = left;
			ly += LEGEND_LH + 8;
		}
		layout.legendItems.push_back({ lx, ly });
		lx += itemW + 16;
	}
	layout.legend.set(ix, y, iw, ly + LEGEND_LH + 10 - y);
	y += layout.legend.height + 14;

	layout.provLabelTop = y;
	y += LABEL_LH + 6;

	// The provenance box fills the rest of the window, up to its content.
	const size_t provCpl = std::max(1, static_cast<int>((iw - 32 - 10) / smallFont.advance()));
	layout.provLines = wrapLines(provenance, provCpl);
	const float contentH = layout.provLines.size() * PROV_LH + 28;
	const float availableH = winH - PAGE_PAD - PANEL_PAD_Y - y;
	const float boxH = std::min(contentH, std::max(28 + 3 * PROV_LH, availableH));
	layout.provBox.set(ix, y, iw, boxH);
	layout.provVisibleLines = std::max(1, static_cast<int>((boxH - 28) / PROV_LH + 0.01f));
	provScroll = ofClamp(provScroll, 0, std::max<float>(0, layout.provLines.size() - layout.provVisibleLines));

	layout.panel.set(PAGE_PAD, panelTop, CANVAS_W, layout.provBox.getBottom() + PANEL_PAD_Y - panelTop);
}

glm::vec2 ofApp::toPage(float x, float y) const {
	return { x / pixelScale - layout.originX, y / pixelScale };
}

std::string ofApp::legendLabel(size_t i) const {
	const Shape & s = shapes[i];
	return ofToString(i + 1) + ": " + s.type + " · " + s.color.name + " · " + s.sizeName;
}

// ============================================================================
//  Input
// ============================================================================

void ofApp::mousePressed(int x, int y, int button) {
	updateLayout();
	const glm::vec2 p = toPage(x, y);

	if (layout.newButton.inside(p)) {
		newSeed();
	} else if (layout.copyButton.inside(p)) {
		blurSeed();
		copyProvenance();
	} else if (layout.seedField.inside(p)) {
		// Place the caret at the clicked character.
		const size_t cpl = layout.seedCharsPerLine;
		const size_t rows = std::max<size_t>(1, (seedText.size() + cpl - 1) / cpl);
		const int row = ofClamp(std::floor((p.y - layout.seedField.y - 10) / SEED_LH), 0, rows - 1);
		const int col = ofClamp(std::round((p.x - layout.seedField.x - 14) / seedFont.advance()), 0, cpl);
		focusSeed(std::min(row * cpl + col, seedText.size()));
	} else {
		blurSeed();
	}
}

void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY) {
	if (layout.provBox.inside(toPage(x, y))) {
		provScroll -= scrollY;  // clamped in updateLayout()
	}
}

void ofApp::keyPressed(ofKeyEventArgs & e) {
	const bool shortcut = e.hasModifier(OF_KEY_COMMAND) || e.hasModifier(OF_KEY_CONTROL);

	if (!seedFocused) {
		if (e.key == OF_KEY_ESC) {
			ofExit();
		} else if (e.key == OF_KEY_RETURN) {
			focusSeed(seedText.size());
		} else if (!shortcut && (isKey(e, 'n') || e.key == ' ')) {
			newSeed();
		} else if (isKey(e, 'c')) {
			copyProvenance();
		}
		return;
	}

	caretBlinkStart = ofGetElapsedTimef();

	if (e.key == OF_KEY_RETURN || e.key == OF_KEY_ESC) {
		blurSeed();
	} else if (shortcut && isKey(e, 'a')) {
		seedAllSelected = !seedText.empty();
	} else if (shortcut && isKey(e, 'c')) {
		if (seedAllSelected) ofSetClipboardString(seedText);
	} else if (shortcut && isKey(e, 'v')) {
		insertHex(ofGetClipboardString());
	} else if (e.key == OF_KEY_BACKSPACE || e.key == OF_KEY_DEL) {
		if (seedAllSelected) {
			seedText.clear();
			caret = 0;
		} else if (e.key == OF_KEY_BACKSPACE && caret > 0) {
			seedText.erase(--caret, 1);
		} else if (e.key == OF_KEY_DEL && caret < seedText.size()) {
			seedText.erase(caret, 1);
		}
		onSeedEdited();
	} else if (e.key == OF_KEY_LEFT || e.key == OF_KEY_RIGHT || e.key == OF_KEY_UP || e.key == OF_KEY_DOWN
		|| e.key == OF_KEY_HOME || e.key == OF_KEY_END) {
		const size_t cpl = layout.seedCharsPerLine;
		if (seedAllSelected) {
			caret = (e.key == OF_KEY_LEFT || e.key == OF_KEY_UP || e.key == OF_KEY_HOME) ? 0 : seedText.size();
		} else if (e.key == OF_KEY_LEFT) {
			caret = caret > 0 ? caret - 1 : 0;
		} else if (e.key == OF_KEY_RIGHT) {
			caret = std::min(caret + 1, seedText.size());
		} else if (e.key == OF_KEY_UP) {
			caret = caret >= cpl ? caret - cpl : 0;
		} else if (e.key == OF_KEY_DOWN) {
			caret = std::min(caret + cpl, seedText.size());
		} else {
			caret = e.key == OF_KEY_HOME ? 0 : seedText.size();
		}
		seedAllSelected = false;
	} else if (!shortcut && e.codepoint < 128 && std::isxdigit(static_cast<int>(e.codepoint))) {
		insertHex(std::string(1, static_cast<char>(e.codepoint)));
	}
}

void ofApp::focusSeed(size_t caretPos) {
	seedFocused = true;
	seedAllSelected = false;
	caret = caretPos;
	caretBlinkStart = ofGetElapsedTimef();
}

void ofApp::blurSeed() {
	seedFocused = false;
	seedAllSelected = false;
	seedText = currentSeed;
	caret = seedText.size();
}

void ofApp::insertHex(const std::string & text) {
	std::string hex;
	for (char c : text) {
		if (std::isxdigit(static_cast<unsigned char>(c))) hex += c;
	}
	if (hex.empty()) return;

	if (seedAllSelected) {
		seedText.clear();
		caret = 0;
	}
	seedText.insert(caret, hex);
	caret += hex.size();
	onSeedEdited();
}

void ofApp::onSeedEdited() {
	seedAllSelected = false;
	// As in the original, an emptied field keeps the last seed.
	if (!seedText.empty()) {
		currentSeed = seedText;
		refreshFromSeed();
	}
}
