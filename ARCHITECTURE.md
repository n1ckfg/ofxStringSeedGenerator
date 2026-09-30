# Architecture: ofxStringSeedGenerator

## Overview

ofxStringSeedGenerator is a C++ port of [StringSeedGenerator](https://github.com/n1ckfg/StringSeedGenerator), a lightweight implementation of the **SSoT (String Seed of Thought)** protocol for enforcing diversity in generative outputs. It has two parts: a core library with no dependencies (`src/`) and an openFrameworks port of the interactive visual demo (`example/`).

As in the original, the goal is **verifiable randomness**: discretionary choices are derived mathematically from a cryptographic seed, which prevents distribution collapse, where a generator falls back on its most likely "prior" response.

## Core Components

### 1. The Core Library (`src/ofxStringSeedGenerator.h/.cpp`)

The `ofxStringSeed` class is a line-for-line port of the `StringSeed` class in `stringseed.js`. It uses only the C++17 standard library and the platform's random number API, so it also builds outside openFrameworks.

**Key Responsibilities:**
- **Seed Generation:** `generateSeed()` reads the platform CSPRNG (`arc4random_buf`, `/dev/urandom`, `rand_s` or `getentropy`) and returns a lower-case hex string. If none is available it falls back to `std::random_device`.
- **Axis Management:** Keeps a registry of named axes (dimensions of choice), each with a list of candidate strings.
- **Deterministic Resolution:** `resolve()` implements the SSoT mapping arithmetic:
  1. Strips non-hex characters from the seed and lower-cases it.
  2. Takes non-overlapping 4-character hex slices (axis *i* → characters 4*i* to 4*i*+3, wrapping if the seed is short).
  3. Parses each slice as an integer and selects `candidates[int % candidates.size()]`.
- **Provenance Generation:** `provenance()` and `groupedProvenance()` render the plaintext block that documents every derivation, in exactly the format `stringseed.js` produces.

### 2. The Visualizer (`example/`)

An openFrameworks app that ports `index.html` and draws with oF's GL renderer where the original used p5.js.

**Key Responsibilities:**
- **Axis Implementation:** Registers the same 6 axes per shape (Type, Color, Size, Rotation, X-Position, Y-Position) for 5 shapes, using the original candidate lists.
- **Rendering:** Draws the background, vignette, grid, shapes and labels to match the p5.js sketch. The canvas `shadowBlur` glow is reproduced with a separable Gaussian blur shader. Each shape's glow is rendered to its own FBO once per seed rather than every frame.
- **Application State:** Keeps the current seed and re-derives the shapes whenever the seed is edited, pasted or regenerated.
- **Transparency:** Shows the provenance block and a shape legend below the canvas, so the picture can be checked against the arithmetic.
- **HiDPI:** oF 0.12 measures windows in framebuffer pixels, so the page is laid out in logical pixels and scaled by the window's pixel density. Fonts are rasterized at device resolution.

## Data Flow

1. **Entropy Collection:** A random hex seed is generated, or the user types or pastes one.
2. **Schema Definition:** The application registers its axes and their candidates.
3. **Resolution (`ofxStringSeed::resolve`):** The seed is sliced into 4-character chunks and each chunk is mapped to a candidate index.
4. **Execution:** The application uses the selected indices, for example to draw a scene or compose a prompt.
5. **Provenance Validation:** The provenance block is kept so that anyone can confirm the output was determined by the seed.

## Design Principles

- **Zero-Dependency Core:** The library needs only C++17, with no oF headers.
- **Parity with the Original:** For the same axes and seed, the library's output is byte-identical to `stringseed.js`.
- **Verifiability:** Every line of the provenance block can be checked with simple external arithmetic (e.g. `echo $((16#edc1 % 8))`).
- **Mathematical Independence:** Non-overlapping slices ensure that the choice on one axis is not correlated with the choice on another.
