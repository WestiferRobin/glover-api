# glover-api

Shared C++20 models and mock data for the SDSU CS 576 wireless stenography glove
project. This foundation lets teammates develop against logical finger states
without real ESP32 hardware. It does not contain completed sensor, communication,
chord-processing, or computer-client components.

## MVP architecture

Two physical gloves each provide five binary finger states. One glove has the
Dominant communication role and the other has the Supplementary role. The
Supplementary glove eventually transfers its state to the Dominant glove, which
combines the left and right hands. The system ultimately delivers recognized text
to a computer. Transport and placement of chord processing remain undecided.

Conceptual integration flow:

```text
Hardware/Sensors → five-finger logical states → inter-glove communication
→ Dominant glove combines both hands → ten-finger state → wireless communication
→ chord processing → decoded result → computer client
```

This flow describes component boundaries, not a deployment decision. Final chord
processing placement and the production wireless payload remain undecided.

## Prerequisites and quick start

- A C++20-capable C++ compiler available as `c++`.
- Make and a Unix-like shell environment. Other environments are not yet verified.
- On macOS, installed and selected Apple developer tools providing `c++` and
  `xcrun`. The Makefile selects their macOS SDK through `SDKROOT`.

Clone the repository, then build from its root:

```sh
git clone https://github.com/WestiferRobin/glover-api.git
cd glover-api
make
make run
```

`make` compiles all three source files with C++20, `-Wall`, `-Wextra`, and
`-Wpedantic`, producing `build/glove`. `make run` builds when needed and executes
that program. `make clean` separately removes the executable. You may select a
compiler with, for example, `make CXX=clang++`.

The application's expected output is:

```text
[1,0,0,0,0,0,0,0,0,1]
[0,1,0,0,0,0,0,0,1,0]
[0,0,1,0,0,0,0,1,0,0]
[1,0,1,0,0,0,0,0,1,0]
[1,1,1,1,1,1,1,1,1,1]
```

These are the dictionary states for “I,” “you,” “we,” “that,” and a single space;
the application does not translate them to text. It emits five snapshots and exits.

The mock CLI emits one bracketed, comma-separated ten-bit state per line to
stdout. **Stdout is a temporary mock interface, not the finalized production
communication protocol.** Make also echoes its commands, so use `./build/glove`
directly after building when consuming only state lines or piping to a component.

## Repository structure

```text
include/glove/glove.hpp  Public single-glove model
include/glove/pair.hpp   Public left/right pair model
src/glove/glove.cpp      Single-glove implementation
src/glove/pair.cpp       Pair implementation and role validation
src/main.cpp            Sandbox and five-state mock example
examples/decoded_results.txt  Temporary decoded results for independent client work
Makefile                Build, run, and clean commands
test/example.cpp        Empty placeholder; not built or a test suite
```

## Stable logical state contract

One glove contains exactly five binary finger states: `false` / `0` means
inactive, and `true` / `1` means active. The hand-specific ordering is:

| Index | Left Glove | Right Glove |
|---|---|---|
| 0 | Pinky | Thumb |
| 1 | Ring | Index |
| 2 | Middle | Middle |
| 3 | Index | Ring |
| 4 | Thumb | Pinky |

`GlovePair::getState()` returns exactly ten values, left hand first and right
hand second:

```text
[L pinky, L ring, L middle, L index, L thumb, R thumb, R index, R middle, R ring, R pinky]
```

The implementation concatenates the two arrays without reversing them. Callers
must supply the correct per-hand order. Dominant/Supplementary are communication
roles, not physical hand identities: either hand can be Dominant, and changing
roles never changes finger ordering. The mock chooses right Dominant and left
Supplementary only as an example.

### Public models

`Glove` stores five booleans and a `GloveRole`. Construction initializes all
fingers inactive. `setFinger()` and `getFinger()` use indexes `0..4` and throw
`std::out_of_range` for invalid indexes. `setState()` accepts
`std::array<bool, FINGER_COUNT>` (`FINGER_COUNT == 5`); `getRole()` returns the
communication role. Physical hand identity is supplied by the caller, not stored
as a separate field in `Glove`.

`GlovePair(leftGlove, rightGlove)` owns copies of the supplied gloves. It requires
exactly one Dominant and one Supplementary glove and throws `std::invalid_argument`
otherwise. Changing the original gloves after construction does not change the
pair; use `setLeftState()` and `setRightState()` to update its owned states.

`getLeftState()` and `getRightState()` return five-value arrays by value.
`getState()` returns `std::array<bool, PAIR_FINGER_COUNT>`
(`PAIR_FINGER_COUNT == 10`). State getters return snapshots, not detected
stenography chords. No synchronized sampling, freshness check, or chord-completion
logic is provided.

The logical C++ representation is not yet a wireless packet format. Do not assume
that raw C++ object memory defines serialized bytes.

## MVP dictionary

The following is the complete agreed contract: 20 word mappings plus one space
mapping. Compact bit strings below use the same left-to-right index order as the
ten-value arrays; they are table notation, not a replacement CLI format.

| 10-finger state | Expected output |
|---|---|
| 1000000001 | I |
| 0100000010 | you |
| 0010000100 | we |
| 0001001000 | they |
| 0000110000 | it |
| 1100000001 | this |
| 1010000010 | that |
| 1001000100 | can |
| 1000110000 | will |
| 0110000001 | is |
| 0101000010 | not |
| 0100110000 | and |
| 0011000001 | help |
| 0010110000 | make |
| 0010001010 | test |
| 0001100001 | use |
| 0001001100 | work |
| 1110000001 | project |
| 1101000010 | system |
| 1100110000 | now |
| 1111111111 | space |

“space” means one space character (`" "`), not the literal word “space.” The
all-zero state (`0000000000`) has no dictionary mapping. Repeated snapshots do
not by themselves specify repeated text insertion. Chord recognition and event
timing belong to Nikita's component; decoded-result delivery to Eduardo's client
still needs an agreed interface. No dictionary translation is implemented here.
Plover compatibility is a future goal, not an MVP requirement.

## Component boundaries

| Teammate | Responsibility and integration boundary |
|---|---|
| Skyler — Hardware/Sensors | Produces five logical finger states per physical glove in the documented order. Raw sensor readings, thresholds, and calibration remain hardware-side concerns. Fixed arrays can substitute for hardware during development. |
| Esteban — Inter-Glove Communication | Transfers Supplementary glove state to the Dominant glove while preserving hand identity. A mock can supply the corresponding left or right pair setter directly. |
| Wesley — Wireless Communication | Develops communication between the Dominant ESP32 and computer while preserving the agreed logical data contract. The state-printing example provides a temporary mock boundary. |
| Nikita — Chord Processing | Consumes ten-finger states and translates recognized chords using the MVP dictionary. Can start from fixed arrays or CLI state lines without hardware or networking. |
| Eduardo — Computer Client | Consumes decoded results and produces computer/keyboard input. Can use `examples/decoded_results.txt` independently of Nikita's implementation. The final result interface remains undecided. |

These models and examples establish shared data expectations, not implementations
of the five components. Each component can use fixed logical inputs while its
upstream implementation is unfinished; no transport technology is required by
the model API.

## Plug-and-go integration guide

### Skyler and Esteban: supply and route five-finger states

Skyler's logical output is `std::array<bool, 5>` per physical glove, after raw
sensor normalization outside this repository. Left order is pinky, ring, middle,
index, thumb; right order is thumb, index, middle, ring, pinky.

The following C++ example can be placed inside a component's `main()` with the
public headers included. Fixed arrays substitute for sensor input and a received
inter-glove message:

```cpp
GlovePair gloves{Glove(GloveRole::Supplementary), Glove(GloveRole::Dominant)};
std::array<bool, 5> receivedLeft{true, false, true, false, false};
std::array<bool, 5> localRight{false, false, false, true, false};
gloves.setLeftState(receivedLeft);
gloves.setRightState(localRight);
std::array<bool, 10> combined = gloves.getState();
// combined is [1,0,1,0,0,0,0,0,1,0], the documented "that" state.
(void)combined;
```

Esteban must preserve physical hand identity independently of roles. This example
receives the left Supplementary glove; when the right glove is Supplementary,
route its received state to `setRightState()` instead. The setters take the same
logical arrays whether supplied locally or received from a future transport.

### Wesley: replace the mock communication boundary

Build with `make`, then consume `./build/glove` stdout: exactly ten `0`/`1`
values per bracketed, comma-separated line, in the documented order. An adapter
can parse each line into `std::array<bool, 10>` for downstream development.
No parser or real transport is provided here. Real communication can eventually
replace mock input while preserving the downstream logical state representation;
these CLI lines do not prescribe the production wireless payload or framing.

### Nikita: consume snapshots and produce recognized results

Receive `std::array<bool, 10>` directly from `getState()` or from an input adapter.
The complete [MVP dictionary](#mvp-dictionary) above remains the authoritative
fixture. The following is **pseudocode only**, not a new production API or a
second dictionary implementation:

```text
input: state, a std::array<bool, 10>
result = unmapped
for each row in the README MVP dictionary:
    compare state[i] with (row.bitString[i] == '1') for every i from 0 to 9
    if all ten positions match:
        result = decoded row output (the space row yields exactly " ")
        stop searching
if result is unmapped:
    report no dictionary match in this example; do not invent a text value
else:
    pass the decoded result to a mock sink using a team-agreed interface
```

For example, `1010000010` has the expected result `"that"`, `1111111111`
has result `" "`, and `0000000000` is unmapped. This illustrates lookup only:
it does not decide when a snapshot becomes a completed chord, when held states
repeat, or how production code reports unknown chords. Nikita owns that processing.

### Eduardo: consume decoded results without a processor

Use [examples/decoded_results.txt](examples/decoded_results.txt) directly; it
requires neither a build nor Nikita's implementation. It contains five UTF-8
records corresponding to the five mock states, in order. Each line encloses one
literal result in double quotes. For this fixture only, remove the outer quotes
and the line ending; preserve every character inside the quotes. No escapes or
embedded quotes occur in this fixture.

- `"I"`, `"you"`, `"we"`, and `"that"` contain words without trailing spaces.
- `" "` contains exactly one space character, not an empty string or a space label.
- Quotes and line endings are fixture delimiters, not keyboard input.

Read one line at a time, check its opening and closing quotes, and pass its
interior text to your own mock client sink. No keyboard injection is provided.
The literal concatenation is `"Iyouwethat "`. Do not automatically insert spaces
between words.

This quoted-line format is an **explicitly temporary fixture convention requiring
team confirmation**, not an agreed production client protocol. It is a sample of
decoded results, not a dictionary lookup table or evidence of chord detection.
The state producer and decoded fixture are independently usable; no running
processor currently connects them.

## Using the models in another C++ component

Include the public headers:

```cpp
#include <glove/glove.hpp>
#include <glove/pair.hpp>
```

From this repository's root, compile your own entry point together with the two
model sources. Replace `path/to/component_main.cpp` with your component's source:

```sh
mkdir -p build
c++ -std=c++20 -Wall -Wextra -Wpedantic -Iinclude \
    path/to/component_main.cpp src/glove/glove.cpp src/glove/pair.cpp \
    -o build/component-demo
```

On macOS, first select the same SDK used by the Makefile:

```sh
export SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
```

Do not also link `src/main.cpp`, since it supplies the sandbox's own `main()`.
The models use only the standard library; no external dependencies are required.

## UNDECIDED

- Actual ESP32 sensor hardware and raw sensor representation.
- Sensor thresholds and calibration.
- Bluetooth versus Wi-Fi versus another wireless transport.
- Actual packet serialization and framing.
- Timing, debounce, and chord-completion detection.
- Placement of chord processing.
- Production wireless payload (logical states versus another agreed representation).
- Final computer-client integration interface, including decoded-result delivery.
- Plover compatibility details; compatibility is a future goal outside the MVP.

## Current limitations

The sandbox only supplies five fixed state snapshots. It does not read sensors,
transfer inter-glove data, perform networking, detect chords, translate dictionary
entries, or generate keyboard input. The decoded-result fixture substitutes for
processor output during independent client development; it is not generated by
the application. There is no formal test suite or end-to-end hardware integration.
Production integration and the remaining interface decisions are future work;
this documentation does not select protocols or resolve those decisions.

## Development roadmap

1. Confirm the decoded-result interface with Nikita and Eduardo, including the
   temporary fixture convention, unknown-chord handling, and chord completion.
2. Develop each component independently against the documented arrays and mock
   results. Keep hand identity separate from communication roles.
3. Agree on ESP32 hardware, transport, processing placement, and payload/framing
   before implementing production adapters.
4. Replace mock sources with sensor and communication implementations, then
   integrate chord processing and the computer client incrementally.
5. Verify the complete hardware-to-computer path on the selected platforms.
   Consider Plover compatibility after the MVP.

The v0.1 foundation provides model and mock contracts; it does not claim completed
production components. Local build verification covers macOS with Apple Clang;
Linux, Windows, and ESP32 builds have not been verified.
