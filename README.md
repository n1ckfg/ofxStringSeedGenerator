# ofxStringSeedGenerator

An openFrameworks port of [StringSeedGenerator](https://github.com/n1ckfg/StringSeedGenerator), an implementation of the **SSoT (String Seed of Thought)** protocol: every discretionary choice is derived from a cryptographic hex seed with arithmetic anyone can check, and a provenance block records how each choice was made.

Based on Misaki & Akiba (2025), *String Seed of Thought: Prompting LLMs for Distribution-Faithful and Diverse Generation*, [arxiv 2510.21150](https://arxiv.org/abs/2510.21150).

Tested with oF 0.12.1 on macOS.

## Usage

```cpp
#include "ofxStringSeedGenerator.h"

ofxStringSeed ss;
ss.addAxis("shape", { "circle", "square", "triangle" });
ss.addAxis("color", { "red", "blue", "green", "yellow" });

std::string seed = ofxStringSeed::generateSeed(ss.requiredSeedBytes());
auto results = ss.resolve(seed);

for (const auto & r : results) {
	ofLogNotice() << r.axis << " -> " << r.choice << " (index " << r.index << ")";
}
ofLogNotice() << ofxStringSeed::provenance(seed, results);
```

```
SSoT provenance
  seed: edc19ab7
  axis 1 (shape, n=3): int(edc1,16)=60865, 60865 % 3 = 1 -> square
  axis 2 (color, n=4): int(9ab7,16)=39607, 39607 % 4 = 3 -> yellow
```

Any line can be checked from a shell: `python3 -c 'print(int("edc1", 16) % 3)'`.

## API

| Method | Description |
| --- | --- |
| `static generateSeed(numBytes = 8)` | Random lower-case hex string from the platform CSPRNG, two characters per byte. |
| `addAxis(name, candidates)` | Registers an axis. Needs at least 2 candidates; throws `std::invalid_argument` otherwise. Chainable. |
| `clearAxes()`, `getAxes()` | Removes or returns the registered axes. |
| `requiredSeedLength()`, `requiredSeedBytes()` | Hex characters or bytes needed for one non-overlapping 4-character slice per axis. |
| `resolve(seed)` | Returns one `Result { axis, hexSlice, intValue, n, index, choice }` per axis. Non-hex characters in the seed are ignored. |
| `static provenance(seed, results, meta = {})` | Provenance block. `Meta { piece, version, date }` fills the header; empty fields are left out. |
| `static groupedProvenance(seed, results, groupSize, groupLabel, meta)` | Provenance with axes grouped under labels, e.g. per shape. |
| `static sanitizeSeed(seed)`, `getSlice(seed, i)`, `mapToIndex(slice, n)` | The individual steps of `resolve()`. |

A seed shorter than `requiredSeedLength()` wraps around, which correlates axes. Use `requiredSeedBytes()` when generating seeds.

### Differences from `stringseed.js`

- Candidates are strings. To choose richer data (colors, sizes, structs), register their names and use `Result::index` to look them up in your own array, as the example does.
- `generateSeed()` uses `arc4random_buf` on Apple/BSD, `/dev/urandom` on Linux/Android, `rand_s` on Windows and `getentropy` on Emscripten, in place of `crypto.getRandomValues()`.
- `groupedProvenance()` with a `groupSize` of 0 puts all axes in one group; the JS version loops forever.
- The core has no openFrameworks dependency and builds with any C++17 compiler.

Otherwise the output is identical: for the same axes and seed, `resolve()`, `provenance()` and `groupedProvenance()` produce the same text as the JavaScript version, byte for byte.

## Example

`example/` is a port of the original visual demo (`index.html`). Five shapes each get six axes (type, color, size, rotation, x and y position) resolved from a 120-character seed, and the provenance block is shown next to the canvas.

- Click the seed to edit it. Only hex digits are accepted, and the shapes update on every keystroke. Cmd/Ctrl+A selects the whole seed and Cmd/Ctrl+V pastes one. Return or Esc finishes editing.
- **New** (or `n`) generates a fresh seed. **Copy** (or `c`) copies the provenance block to the clipboard.
- Scroll over the provenance box to see all of it. Esc quits when the seed isn't being edited.

The example uses the default GL 2.1 renderer, because its glow shaders are GLSL 1.20.
