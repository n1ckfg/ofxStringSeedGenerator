/**
 * ofxStringSeed — C++ implementation of the SSoT (String Seed of Thought) protocol.
 *
 * Port of stringseed.js from https://github.com/n1ckfg/StringSeedGenerator
 *
 * Based on Misaki & Akiba (2025), "String Seed of Thought: Prompting LLMs for
 * Distribution-Faithful and Diverse Generation", arxiv 2510.21150.
 *
 * The protocol:
 *   1. Generate a real hex seed from cryptographic randomness.
 *   2. Enumerate axes — each axis lists 2+ concrete candidates.
 *   3. Map seed to indices — take non-overlapping 4-char hex slices,
 *      convert to int, apply modulo N.
 *   4. Write a provenance block with verifiable arithmetic.
 *
 * Usage:
 *   ofxStringSeed ss;
 *   ss.addAxis("shape", { "circle", "square", "triangle" });
 *   ss.addAxis("color", { "red", "blue", "green", "yellow" });
 *
 *   std::string seed = ofxStringSeed::generateSeed(8);   // 16 hex chars
 *   auto results = ss.resolve(seed);
 *   std::cout << ofxStringSeed::provenance(seed, results);
 *
 * The core depends only on the C++17 standard library (no openFrameworks
 * headers), so it can be used outside of an OF app as well.
 *
 * @version 1.0.0
 * @license MIT
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class ofxStringSeed {
public:

	/// A named dimension of choice and its candidate list.
	struct Axis {
		std::string name;
		std::vector<std::string> candidates;
	};

	/// Output of `mapToIndex()`.
	struct Mapping {
		uint64_t intValue;
		size_t index;
	};

	/// One entry per axis, fully traceable.
	struct Result {
		std::string axis;
		std::string hexSlice;
		uint64_t intValue = 0;
		size_t n = 0;
		size_t index = 0;
		std::string choice;
	};

	/// Optional provenance header fields. Empty fields are omitted.
	struct Meta {
		std::string piece;
		std::string version;
		std::string date;
	};

	// -------------------------------------------------------------------------
	// Seed generation
	// -------------------------------------------------------------------------

	/**
	 * Generate a cryptographically random hex seed.
	 *
	 * Uses the platform CSPRNG (arc4random_buf on Apple/BSD, /dev/urandom on
	 * Linux/Android, rand_s on Windows, getentropy on Emscripten). Each byte
	 * yields two hex characters, matching the `openssl rand -hex N` convention.
	 *
	 * @param numBytes Number of random bytes (output length = numBytes × 2).
	 * @returns Lower-case hex string.
	 */
	static std::string generateSeed(size_t numBytes = 8);

	// -------------------------------------------------------------------------
	// Axis management
	// -------------------------------------------------------------------------

	/**
	 * Register a named axis with its candidate list.
	 *
	 * Order matters: index 0 is the first candidate. To select richer data
	 * than strings, register the candidates' names and use `Result::index`
	 * to look up the corresponding item in your own array.
	 *
	 * @param name Human-readable axis name (e.g. "shape type").
	 * @param candidates At least 2 concrete, distinguishable options.
	 * @returns *this (for chaining).
	 * @throws std::invalid_argument If fewer than 2 candidates are supplied.
	 */
	ofxStringSeed & addAxis(const std::string & name, const std::vector<std::string> & candidates);

	/// Remove all registered axes.
	ofxStringSeed & clearAxes();

	/// The currently registered axes, in registration order.
	const std::vector<Axis> & getAxes() const;

	/**
	 * Minimum hex-string length needed for non-overlapping 4-char slices
	 * across all currently registered axes.
	 */
	size_t requiredSeedLength() const;

	/// Minimum byte count for `generateSeed()` to cover all axes without wrapping.
	size_t requiredSeedBytes() const;

	// -------------------------------------------------------------------------
	// Slice extraction & mapping
	// -------------------------------------------------------------------------

	/// Strip non-hex characters and lower-case the rest, as `resolve()` does.
	static std::string sanitizeSeed(const std::string & seed);

	/**
	 * Extract a 4-character hex slice for the given axis index.
	 *
	 * Uses non-overlapping slices (axis 0 → chars 0-3, axis 1 → chars 4-7, …).
	 * If the seed is shorter than needed the slice wraps modulo seed length,
	 * which introduces correlation — prefer a seed of `requiredSeedLength()`.
	 *
	 * @param seed Hex string.
	 * @param axisIndex 0-based axis position.
	 * @returns 4-character hex slice (lower-case), or "0000" for an empty seed.
	 */
	static std::string getSlice(const std::string & seed, size_t axisIndex);

	/**
	 * Convert a hex slice to a candidate index via integer conversion + modulo.
	 *
	 * Parses leading hex digits like JavaScript's `parseInt(slice, 16)`;
	 * if there are none, or `n` is 0, returns { 0, 0 }.
	 *
	 * @param hexSlice 4-char hex string.
	 * @param n Number of candidates on this axis.
	 */
	static Mapping mapToIndex(const std::string & hexSlice, size_t n);

	// -------------------------------------------------------------------------
	// Resolution
	// -------------------------------------------------------------------------

	/**
	 * Resolve the seed against all registered axes.
	 *
	 * For every axis, this method:
	 *   1. Extracts a non-overlapping 4-char hex slice.
	 *   2. Converts the slice to an integer.
	 *   3. Applies `int % candidateCount` to select a candidate.
	 *
	 * @param seed Hex string (non-hex characters are stripped).
	 * @returns One entry per axis, fully traceable.
	 */
	std::vector<Result> resolve(const std::string & seed) const;

	// -------------------------------------------------------------------------
	// Provenance
	// -------------------------------------------------------------------------

	/**
	 * Render a verifiable SSoT provenance block.
	 *
	 * A reviewer can verify any line with:
	 *   `python3 -c 'print(int("<hexSlice>", 16) % <n>)'`
	 *
	 * @param seed Original hex seed.
	 * @param results Output of `resolve()`.
	 * @param meta Optional { piece, version, date }.
	 * @returns Multi-line provenance block.
	 */
	static std::string provenance(const std::string & seed, const std::vector<Result> & results, const Meta & meta = {});

	/**
	 * Render a grouped provenance block — useful when axes are logically
	 * organised in repeating groups (e.g. per-shape properties).
	 *
	 * @param seed Hex seed.
	 * @param results Output of `resolve()`.
	 * @param groupSize Axes per group (0 puts every axis in a single group).
	 * @param groupLabel `(groupIndex) -> string`. Default: "Group N".
	 * @param meta Optional { piece, version, date }.
	 */
	static std::string groupedProvenance(const std::string & seed, const std::vector<Result> & results, size_t groupSize,
		const std::function<std::string(size_t)> & groupLabel = nullptr, const Meta & meta = {});

private:
	std::vector<Axis> axes;
};
