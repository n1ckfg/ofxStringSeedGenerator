// rand_s() is only declared when this is defined before the first <stdlib.h>.
#if defined(_WIN32) && !defined(_CRT_RAND_S)
#define _CRT_RAND_S
#endif

#include "ofxStringSeedGenerator.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <stdexcept>

#if defined(__EMSCRIPTEN__)
#include <sys/random.h>
#endif

namespace {

constexpr size_t SLICE_LEN = 4;

bool isHexDigit(char c) {
	return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

char toLower(char c) {
	return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

int hexValue(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	return toLower(c) - 'a' + 10;
}

// Fill `buf` from the platform CSPRNG. Returns false if it is unavailable.
bool fillSecureRandom(unsigned char * buf, size_t len) {
#if defined(__EMSCRIPTEN__)
	// getentropy() maps to crypto.getRandomValues(), like stringseed.js.
	// It accepts at most 256 bytes per call.
	for (size_t off = 0; off < len; off += 256) {
		if (getentropy(buf + off, std::min<size_t>(256, len - off)) != 0) return false;
	}
	return true;
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
	arc4random_buf(buf, len);
	return true;
#elif defined(__linux__)
	FILE * f = std::fopen("/dev/urandom", "rb");
	if (!f) return false;
	size_t got = std::fread(buf, 1, len, f);
	std::fclose(f);
	return got == len;
#elif defined(_WIN32)
	for (size_t i = 0; i < len; i += sizeof(unsigned int)) {
		unsigned int r;
		if (rand_s(&r) != 0) return false;
		for (size_t b = 0; b < sizeof(unsigned int) && i + b < len; b++) {
			buf[i + b] = static_cast<unsigned char>(r >> (8 * b));
		}
	}
	return true;
#else
	(void)buf;
	(void)len;
	return false;
#endif
}

std::string header(const ofxStringSeed::Meta & meta) {
	std::string joined;
	for (const std::string * part : { &meta.piece, &meta.version, &meta.date }) {
		if (part->empty()) continue;
		if (!joined.empty()) joined += ", ";
		joined += *part;
	}
	return joined.empty() ? "SSoT provenance" : "SSoT provenance -- " + joined;
}

std::string axisLine(const std::string & indent, size_t i, const ofxStringSeed::Result & r) {
	const std::string intStr = std::to_string(r.intValue);
	const std::string nStr = std::to_string(r.n);
	return indent + "axis " + std::to_string(i + 1) + " (" + r.axis + ", n=" + nStr + "): "
		+ "int(" + r.hexSlice + ",16)=" + intStr + ", "
		+ intStr + " % " + nStr + " = " + std::to_string(r.index) + " -> " + r.choice + "\n";
}

} // namespace

// -----------------------------------------------------------------------------
// Seed generation
// -----------------------------------------------------------------------------

std::string ofxStringSeed::generateSeed(size_t numBytes) {
	std::vector<unsigned char> bytes(numBytes);
	if (numBytes > 0 && !fillSecureRandom(bytes.data(), numBytes)) {
		// Non-deterministic on all mainstream toolchains, but not guaranteed
		// to be cryptographically secure.
		std::random_device rd;
		for (auto & b : bytes) {
			b = static_cast<unsigned char>(rd() & 0xFF);
		}
	}

	static const char * digits = "0123456789abcdef";
	std::string seed;
	seed.reserve(numBytes * 2);
	for (unsigned char b : bytes) {
		seed += digits[b >> 4];
		seed += digits[b & 0x0F];
	}
	return seed;
}

// -----------------------------------------------------------------------------
// Axis management
// -----------------------------------------------------------------------------

ofxStringSeed & ofxStringSeed::addAxis(const std::string & name, const std::vector<std::string> & candidates) {
	if (candidates.size() < 2) {
		throw std::invalid_argument("Axis \"" + name + "\" must have at least 2 candidates.");
	}
	axes.push_back({ name, candidates });
	return *this;
}

ofxStringSeed & ofxStringSeed::clearAxes() {
	axes.clear();
	return *this;
}

const std::vector<ofxStringSeed::Axis> & ofxStringSeed::getAxes() const {
	return axes;
}

size_t ofxStringSeed::requiredSeedLength() const {
	return axes.size() * SLICE_LEN;
}

size_t ofxStringSeed::requiredSeedBytes() const {
	return (requiredSeedLength() + 1) / 2;
}

// -----------------------------------------------------------------------------
// Slice extraction & mapping
// -----------------------------------------------------------------------------

std::string ofxStringSeed::sanitizeSeed(const std::string & seed) {
	std::string clean;
	clean.reserve(seed.size());
	for (char c : seed) {
		if (isHexDigit(c)) clean += toLower(c);
	}
	return clean;
}

std::string ofxStringSeed::getSlice(const std::string & seed, size_t axisIndex) {
	if (seed.empty()) return std::string(SLICE_LEN, '0');

	const size_t start = (axisIndex * SLICE_LEN) % seed.size();
	std::string slice;
	for (size_t i = 0; i < SLICE_LEN; i++) {
		slice += toLower(seed[(start + i) % seed.size()]);
	}
	return slice;
}

ofxStringSeed::Mapping ofxStringSeed::mapToIndex(const std::string & hexSlice, size_t n) {
	uint64_t intValue = 0;
	size_t digits = 0;
	while (digits < hexSlice.size() && isHexDigit(hexSlice[digits])) {
		intValue = intValue * 16 + hexValue(hexSlice[digits]);
		digits++;
	}
	if (digits == 0 || n < 1) {
		return { 0, 0 };
	}
	return { intValue, static_cast<size_t>(intValue % n) };
}

// -----------------------------------------------------------------------------
// Resolution
// -----------------------------------------------------------------------------

std::vector<ofxStringSeed::Result> ofxStringSeed::resolve(const std::string & seed) const {
	const std::string clean = sanitizeSeed(seed);

	std::vector<Result> results;
	results.reserve(axes.size());

	for (size_t i = 0; i < axes.size(); i++) {
		const Axis & axis = axes[i];
		Result r;
		r.axis = axis.name;
		r.n = axis.candidates.size();

		if (clean.empty()) {
			// Degenerate case: every axis falls back to candidate 0.
			r.hexSlice = std::string(SLICE_LEN, '0');
		} else {
			r.hexSlice = getSlice(clean, i);
			const Mapping m = mapToIndex(r.hexSlice, r.n);
			r.intValue = m.intValue;
			r.index = m.index;
		}

		r.choice = axis.candidates[r.index];
		results.push_back(r);
	}

	return results;
}

// -----------------------------------------------------------------------------
// Provenance
// -----------------------------------------------------------------------------

std::string ofxStringSeed::provenance(const std::string & seed, const std::vector<Result> & results, const Meta & meta) {
	std::string block = header(meta) + "\n  seed: " + seed + "\n";

	for (size_t i = 0; i < results.size(); i++) {
		block += axisLine("  ", i, results[i]);
	}

	return block;
}

std::string ofxStringSeed::groupedProvenance(const std::string & seed, const std::vector<Result> & results, size_t groupSize,
	const std::function<std::string(size_t)> & groupLabel, const Meta & meta) {
	if (groupSize == 0) groupSize = std::max<size_t>(1, results.size());

	std::string block = header(meta) + "\n  seed: " + seed + "\n";

	for (size_t g = 0; g * groupSize < results.size(); g++) {
		const std::string label = groupLabel ? groupLabel(g) : "Group " + std::to_string(g + 1);
		block += "\n  " + label + ":\n";
		const size_t start = g * groupSize;
		const size_t end = std::min(start + groupSize, results.size());
		for (size_t i = start; i < end; i++) {
			block += axisLine("    ", i, results[i]);
		}
	}

	return block;
}
