# Glover architecture

This is the component, model, and ownership reference for the implemented
desktop C++20 application. [Protocols](protocols.md) defines serialization and
transport behavior. Build instructions are in [README](../README.md); terminal
procedures and expected output are in [testing](testing.md).

## System overview

Glover represents five finger states per glove and communicates ten-finger
snapshots through TCP, UDP, or HTTP. Desktop simulations reuse five fixed
samples. Networking and the JSON HTTP API are implemented; physical sensor
acquisition, ESP32 integration, chord recognition, and keyboard output are not.
Successful desktop communication does not establish hardware compatibility.

## Components and dependencies

| Component | Responsibility |
|---|---|
| `Glove` | Owns one hand's five Boolean states, physical hand, and communication role |
| `GlovePair` | Owns complementary left/right gloves and constructs combined snapshots |
| `Primary` | Owns a pair; runs local simulation, sends combined states, relays a remote hand, or sends its own hand over HTTP |
| `Secondary` | Owns one glove and sends that physical hand's state |
| `Client` | Receives and validates states; prints snapshots; HTTP mode hosts the application API |
| TCP transport | Connects senders and receives LF-framed messages |
| UDP transport | Sends and receives bounded datagrams |
| HTTP transport | Performs generic GET/POST requests and dispatches registered handlers |
| Text codecs | Convert hand and combined states to/from strict text payloads |
| JSON codec | Serializes HTTP glove payloads and validates submissions |
| CLI | Validates configuration and constructs nodes or generic HTTP actions |
| Mock fixtures | Supply the same five reproducible samples to every simulation |

All nodes implement `Node::run()`. `cli::parse()` constructs nodes but does not
run them; `main.cpp` dispatches actions and reports exceptions. Model objects do
not perform network I/O. Node implementations coordinate models, codecs, and
transports. Generic HTTP classes have no glove state or built-in glove routes.

## Glove model and physical identity

`Glove(Hand, GloveRole)` stores `std::array<bool, FINGER_COUNT>`, where
`FINGER_COUNT` is 5. All fingers initially hold `false` (inactive); `true` means
active. The constructor rejects invalid hand or role enum values with
`std::invalid_argument`.

The actual interface is:

| Member | Behavior |
|---|---|
| `setFinger(Finger, bool)` / `getFinger(Finger)` | Anatomical access mapped to this hand's storage slot; invalid finger throws `std::out_of_range` |
| `setState()` / `getState()` | Replace the raw five-value array / return a copy |
| `getHand()` | Returns `Hand::Left` or `Hand::Right` |
| `getRole()` | Returns `GloveRole::Primary` or `GloveRole::Secondary` |

Primary and Secondary are communication roles, not physical hands. Both
assignments are valid:

| Primary | Secondary |
|---|---|
| Right | Left |
| Left | Right |

Primary constructs its local glove with the Primary role and the opposite
physical hand with the Secondary role. Secondary constructs its own glove with
the Secondary role. Role changes must not change the physical ordering of data.

## Finger ordering

`Finger` enumerates `Pinky`, `Ring`, `Middle`, `Index`, `Thumb` with values 0–4.
Anatomical access reverses the slot calculation for the right hand. Raw arrays
already use the hand-specific order below:

| Stored index | Left | Right |
|---|---|---|
| 0 | Pinky | Thumb |
| 1 | Ring | Index |
| 2 | Middle | Middle |
| 3 | Index | Ring |
| 4 | Thumb | Pinky |

Combined state is `[Left 5 values, Right 5 values]`. Neither pair construction
nor serialization reverses the right array. Changing which hand is Primary
does not reorder combined output.

## GlovePair and ownership

`GlovePair(Glove leftGlove, Glove rightGlove)` owns copies. It requires Left
then Right and exactly one Primary and one Secondary; invalid combinations
throw `std::invalid_argument`. Later changes to the original gloves do not
update the pair.

`setLeftState()` and `setRightState()` update the owned copies.
`getLeftState()` and `getRightState()` return five-value snapshots by value.
`getState()` concatenates them into `std::array<bool, PAIR_FINGER_COUNT>`, where
`PAIR_FINGER_COUNT` is 10. These are snapshots, not synchronized sensor readings
or detected chords.

```text
Primary owns GlovePair
  GlovePair owns Left Glove and Right Glove
Secondary owns its own Glove
Client owns received/session data, not a Glove or GlovePair
```

Each process has independent memory. Primary's copy of the remote hand changes
only when its execution path explicitly updates it; it does not share
Secondary's glove. HTTP Client stores received snapshots and session bookkeeping,
not another authoritative `GlovePair`.

## Node responsibilities

### Primary

Primary always owns one pair and records its local `Hand`. Local simulation and
two-node TCP/combined HTTP load both hands from each `MockSample`. Local mode
prints; network modes send combined state.

Relay modes receive Secondary hand messages over TCP or UDP, reject a hand
matching Primary's own, and combine remote data with the corresponding local
fixture. TCP pairs by accepted message position; UDP pairs by explicit index.
The existing pair receives both updates before combined output is sent.

HTTP relay output uses combined submissions. Direct individual HTTP updates
and submits only Primary's local hand; it does not send the other hand's
placeholder state. Primary does not host an HTTP receiving endpoint.

### Secondary

Secondary owns one `Glove`. For each shared mock sample, it selects the left or
right array according to `getHand()`. It sends hand text over TCP, indexed hand
text over UDP, or individual JSON over HTTP. It does not combine hands.
Bare Secondary execution is a startup smoke test, not a network session.

### Client

TCP Client parses and prints each combined frame immediately. UDP Client
validates unique indices, buffers five states, and prints only a complete batch
in index order. Their completion rules differ; see [protocols](protocols.md).

HTTP Client hosts the application API. Combined mode accepts complete states;
individual mode pairs Primary and Secondary submissions with equal indices.
The first accepted submission fixes the role-to-hand mapping for the session.
Client places left values first regardless of sender role or arrival order.

Its local session contains five slots, received hand snapshots, completed pairs,
highest available indices, the role/hand mapping, a deadline, and lifecycle
flags. A mutex protects this state from concurrent HTTP callbacks. The execution
thread prints the complete batch exactly once in index order using
`serializeState()`. No incomplete batch is printed. Validation checks structure
and session consistency, not equality with the mock fixtures.

Bare Client execution is a startup smoke test. Client does not recognize chords
or emit decoded text or keyboard events.

## Supported desktop workflows

In the table, arrows identify senders and receivers. Primary combines through
its pair except in direct HTTP, where Client assembles received hand arrays.

| Workflow | Sample identification and combination | Completion |
|---|---|---|
| Local: Mock → Primary → output | Fixture iteration; Primary loads both hands | Five snapshots printed |
| Two-node TCP: Primary → Client | Stream position; Primary loads both hands | Primary sends five and closes; Client accepts clean EOF |
| Three-node TCP: Secondary → Primary → Client | Accepted upstream frame position; Primary combines | Primary requires five hand samples and clean upstream EOF |
| Three-node UDP: Secondary → Primary → Client | Envelope index; Primary combines matching local fixture | Receivers require five unique indices within deadline |
| Two-node HTTP: Primary → Client | JSON index; Primary sends combined fixture state | Five accepted combined samples complete Client session |
| TCP-to-HTTP: Secondary → Primary → Client | TCP position becomes HTTP index; Primary combines | Upstream five plus clean EOF; downstream requires HTTP acceptance |
| UDP-to-HTTP: Secondary → Primary → Client | UDP index becomes HTTP index; Primary combines | Upstream five unique indices within deadline; downstream requires HTTP acceptance |
| Direct HTTP: Primary → Client ← Secondary | Each sends its own hand; Client pairs by JSON index | Both roles required at all five indices |

The mixed workflows use TCP or UDP on the first hop and HTTP on the second.
There is no HTTP-to-Primary relay path or two-node UDP Primary mode. For network
verification, start Client first, then relay Primary if present, then Secondary.
In direct HTTP, start Client first; either sender may run first or concurrently.
Wait for receivers to bind before sending.

HTTP senders require a 204 response for each sample. Individual acceptance does
not wait for the opposite hand. Client's completion and process lifetime are
separate: default stay-open mode permits GET inspection after completion; the
alternative closes after the complete batch. See [HTTP API](api.md) for session
deadlines, failure behavior, and acknowledgment limits.

## Fixtures and output

`MockSample` contains left and right five-value arrays. `mockSamples()` returns
a const reference to the five definitions in `src/mock.cpp`; nodes do not define
transport-specific fixtures. `runMock()` constructs a local pair directly,
while bare Primary uses its own pair for equivalent local output.

Local output uses `printMockState()`; receiving nodes use `serializeState()`
and a newline. Both produce the same bracketed, comma-separated ten-value
format. Expected sample values and complete terminal procedures are in
[testing](testing.md#canonical-output).

## Implemented scope and integration limits

Implemented: desktop models, shared fixtures, TCP/UDP/HTTP networking, JSON
validation, the Client HTTP API, indexed UDP/HTTP pairing, mixed relays, and
both physical-hand assignments.

Not implemented: ESP32 sensor acquisition, calibration, live hardware streaming,
chord recognition, keyboard event generation, production device discovery, or
authentication. There is no live sensor-input or recognition callback on
Primary. Model setters and codecs are available building blocks; they do not
establish a finished hardware or recognition interface. Sampling frequency,
hardware recovery, and decoded-result delivery require team decisions.

## Standalone model example

Include the model headers and place this example inside your component's `main()`:

```cpp
#include <glove/glove.hpp>
#include <glove/pair.hpp>

// Inside main():
GlovePair gloves{Glove(Hand::Left, GloveRole::Secondary),
                 Glove(Hand::Right, GloveRole::Primary)};
gloves.setLeftState({true, false, true, false, false});
gloves.setRightState({false, false, false, true, false});
auto combined = gloves.getState(); // [1,0,1,0,0,0,0,0,1,0]
(void)combined;
```

From the repository root, compile your own entry point with the model sources.
On macOS, first select the SDK:

```sh
export SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
```

```sh
mkdir -p build
c++ -std=c++20 -Wall -Wextra -Wpedantic -Iinclude \
    path/to/component_main.cpp src/glove/glove.cpp src/glove/pair.cpp \
    -o build/component-demo
```

Replace `path/to/component_main.cpp` with your source. Do not link
`src/main.cpp`, which defines the application's own entry point.
These models use only the standard library. Raw C++ object memory is not a wire format.

## Source map and related documentation

The implementation is authoritative for behavior. Relevant sources:

- Models: [Glove header](../include/glove/glove.hpp), [implementation](../src/glove/glove.cpp), [pair header](../include/glove/pair.hpp), [implementation](../src/glove/pair.cpp).
- Nodes: [Primary](../src/nodes/primary.cpp), [Secondary](../src/nodes/secondary.cpp), [Client](../src/nodes/client.cpp), and [node configuration](../include/nodes/network.hpp).
- Fixtures: [mock header](../include/mock.hpp) and [definitions](../src/mock.cpp).
- Execution: [parser](../src/cli/parser.cpp), [entry point](../src/main.cpp), and [Makefile](../Makefile).
- Serialization and transport: [protocol reference](protocols.md), [JSON codec](../src/codec/glove_json.cpp).

Related references: [HTTP API](api.md) (routes and schemas), [CLI](cli.md)
(commands and flags), [testing](testing.md) (terminal procedures and expected
output), and [integration](integration.md) (team decisions and unfinished interfaces).
