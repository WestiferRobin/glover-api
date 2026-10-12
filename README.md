# glover-api

C++20 glove models, desktop node simulations, and TCP/UDP/macOS BLE transports
for the SDSU CS 576 wireless stenography glove project.

## Project status

Local five-state simulation, two-node TCP, and three-node TCP/UDP have been verified
on macOS localhost with both physical-hand assignments and exact Client output.
The macOS BLE API, node integration, and mixed-link configuration are implemented;
software validation, bounded no-peer failures, and local cleanup have been checked.
Physical BLE reads, notifications, end-to-end delivery, and disconnection remain
**NOT TESTED** because no second device was available.

ESP32 firmware and sensor integration are not implemented. Chord recognition belongs
to Nikita's component and keyboard integration to Eduardo's. This repository uses
finite mock snapshots; it does not detect chords or translate the dictionary.
The decoded-results fixture supports independent client development. Production
transport selection and hardware integration remain team decisions. macOS with
Apple Clang is verified; Linux, Windows, and ESP32 builds are not verified.
There is no committed automated test suite.

## Four-module architecture

| Module | Responsibility |
|---|---|
| `glove/` | Physical hand/role identity, five-finger models, and left/right pairs. |
| `nodes/` | Abstract `Node`, Primary/Secondary/Client execution and link coordination. |
| `transport/` | TCP/UDP/BLE APIs, socket/platform lifecycles, state serialization and message envelopes. |
| `cli/` | General command-line validation and application configuration; the separate feasibility entry point. |

Headers live under `include/`; C++ and Objective-C++ implementations live under
`src/`. `src/main.cpp` executes parsed actions and handles application errors.
Shared mock utilities sit at the include/source roots. There is no separate
`protocol/` module or common transport superclass.

## MVP architecture

Two physical gloves each provide five binary finger states. One glove has the
Primary communication role and the other has the Secondary role. The
Secondary glove eventually transfers its state to the Primary glove, which
combines the left and right hands. The system ultimately delivers recognized text
to a computer. Production transport selection and placement of chord processing remain undecided.

Conceptual integration flow:

```text
Hardware/Sensors → five-finger logical states → inter-glove communication
→ Primary glove combines both hands → ten-finger state → wireless communication
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

`make` compiles the CLI, mock, models, nodes, state codecs, and TCP/UDP sources with C++20,
`-Wall`, `-Wextra`, and `-Wpedantic`, producing `build/glove`.
`make run` builds when needed and executes
that program. `make clean` separately removes the executable. You may select a
compiler with, for example, `make CXX=clang++`.

Available commands:

```sh
./build/glove              # Default five-state mock
./build/glove mock         # Same mock explicitly
./build/glove --help       # Help (also -h)
./build/glove primary
./build/glove secondary
./build/glove client
```

`primary` now owns a `GlovePair` and emits the same five states as the mock.
Its `Primary(Hand hand = Hand::Right)` constructor supports either physical hand:
the configured hand has the Primary role and the opposite hand has the Secondary
role. The CLI defaults to Right and accepts `--hand left|right` in network modes.
Local and two-node modes use fixed fixtures for both hands. Three-node mode
receives the opposite hand over a separate TCP or UDP link.

Bare `secondary` and `client` commands print startup messages and exit; these only verify
CLI dispatch. Unknown commands and
unsupported options fail with diagnostics on stderr. Explicit help goes to stdout.
`make run` executes the default mock; unchanged subsequent runs do not rebuild.

The default and explicit mock commands produce exactly:

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
├── config/
│   └── macos/
│       └── glove-Info.plist
├── examples/
│   ├── ble_feasibility/
│   │   └── Info.plist
│   └── decoded_results.txt
├── include/
│   ├── cli/
│   │   └── parser.hpp
│   ├── glove/
│   │   ├── glove.hpp
│   │   └── pair.hpp
│   ├── nodes/
│   │   ├── client.hpp
│   │   ├── network.hpp
│   │   ├── node.hpp
│   │   ├── primary.hpp
│   │   └── secondary.hpp
│   ├── transport/
│   │   ├── ble/
│   │   │   ├── detail/
│   │   │   │   └── ble_message.hpp
│   │   │   ├── platform/
│   │   │   │   └── macos/
│   │   │   │       └── session.hpp
│   │   │   ├── central.hpp
│   │   │   ├── config.hpp
│   │   │   ├── experiment.hpp
│   │   │   └── peripheral.hpp
│   │   ├── detail/
│   │   │   ├── socket.hpp
│   │   │   └── state_codec.hpp
│   │   ├── tcp/
│   │   │   ├── detail/
│   │   │   │   └── framing.hpp
│   │   │   ├── client.hpp
│   │   │   └── server.hpp
│   │   └── udp/
│   │       ├── detail/
│   │       │   ├── socket.hpp
│   │       │   └── udp_envelope.hpp
│   │       ├── receiver.hpp
│   │       └── sender.hpp
│   └── mock.hpp
├── src/
│   ├── cli/
│   │   ├── ble_feasibility.cpp
│   │   └── parser.cpp
│   ├── glove/
│   │   ├── glove.cpp
│   │   └── pair.cpp
│   ├── nodes/
│   │   ├── ble.cpp
│   │   ├── client.cpp
│   │   ├── primary.cpp
│   │   └── secondary.cpp
│   ├── transport/
│   │   ├── ble/
│   │   │   └── platform/
│   │   │       └── macos/
│   │   │           ├── central.mm
│   │   │           ├── peripheral.mm
│   │   │           └── session.mm
│   │   ├── tcp/
│   │   │   ├── client.cpp
│   │   │   └── server.cpp
│   │   ├── udp/
│   │   │   ├── receiver.cpp
│   │   │   ├── sender.cpp
│   │   │   └── udp_envelope.cpp
│   │   └── state_codec.cpp
│   ├── main.cpp
│   └── mock.cpp
├── Makefile
└── README.md
```

The state codec is shared by all transports;
`U1` and `B1` envelopes live in their UDP and BLE transport modules. Native BLE
methods are implemented directly in the macOS `.mm` files. `src/nodes/ble.cpp`
coordinates BLE and mixed links. The feasibility entry point is built separately
from `src/main.cpp`; it retains its own app-bundle plist. Generated `build/`
artifacts are not source files.


The internal `cli::parse()` interface handles all existing local, TCP, UDP, BLE,
and mixed-link commands. It returns a help, mock, node, or unknown-command action;
node actions own a `std::unique_ptr<Node>`. Parsing constructs nodes but does not
run them. `main.cpp` executes the action and preserves the application error/exit
boundary. `cli::printHelp()` renders the existing help text. Command syntax and
transport behavior are unchanged.

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
must supply the correct per-hand order. Primary/Secondary are communication
roles, not physical hand identities: either hand can be Primary, and changing
roles never changes finger ordering. The mock chooses right Primary and left
Secondary only as an example.

### Public models

`Glove(Hand, GloveRole)` stores `std::array<bool, 5>`, physical hand identity
(`Hand::Left` or `Hand::Right`), and communication role (`GloveRole::Primary` or
`GloveRole::Secondary`). Construction initializes all fingers inactive and rejects
invalid hand or role values with `std::invalid_argument`.

`setFinger()` and `getFinger()` accept anatomical `Finger` values: `Pinky`, `Ring`,
`Middle`, `Index`, and `Thumb`. Invalid values throw `std::out_of_range`. Anatomical
access maps to the correct slot for each hand; right-hand indexing is reversed
only at this boundary. `setState()` and `getState()` use the physical array order
above without reversal. `getHand()` and `getRole()` return the stored identities.
`FINGER_COUNT` remains 5.

Compared with v0.1, callers must use the renamed roles, provide `Hand` during
construction, and replace numeric finger access with anatomical `Finger` values.
The five- and ten-value state ordering and dictionary remain unchanged.

`GlovePair(leftGlove, rightGlove)` owns copies of the supplied gloves. It requires
a Left glove first, a Right glove second, and exactly one Primary and one
Secondary role; otherwise it throws `std::invalid_argument`. Changing the original
gloves after construction does not change the pair; use `setLeftState()` and
`setRightState()` to update its owned states.

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
| Esteban — Inter-Glove Communication | Transfers Secondary glove state to the Primary glove while preserving hand identity. A mock can supply the corresponding left or right pair setter directly. |
| Wesley — Wireless Communication | Develops communication between the Primary ESP32 and computer while preserving the agreed logical data contract. The state-printing example provides a temporary mock boundary. |
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
GlovePair gloves{Glove(Hand::Left, GloveRole::Secondary),
                 Glove(Hand::Right, GloveRole::Primary)};
std::array<bool, 5> receivedLeft{true, false, true, false, false};
std::array<bool, 5> localRight{false, false, false, true, false};
gloves.setLeftState(receivedLeft);
gloves.setRightState(localRight);
std::array<bool, 10> combined = gloves.getState();
// combined is [1,0,1,0,0,0,0,0,1,0], the documented "that" state.
(void)combined;
```

Esteban must preserve physical hand identity independently of roles. This example
receives the left Secondary glove; when the right glove is Secondary,
route its received state to `setRightState()` instead. The setters take the same
logical arrays whether supplied locally or received from a future transport.

### Wesley: replace the mock communication boundary

Build with `make`, then consume `./build/glove` stdout: exactly ten `0`/`1`
values per bracketed, comma-separated line, in the documented order. An adapter
can parse each line into `std::array<bool, 10>` for downstream development.
The TCP modes below serialize and strictly parse these states between desktop
processes. Production communication can later replace this experimental link;
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

## Experimental Primary → Client TCP integration

`TcpClient` and `TcpServer` provide a blocking, single-client desktop transport.
They are linked into `build/glove` and used only when TCP options are supplied.
Primary initiates the connection as a TCP client; the laptop Client node listens
as a TCP server. These socket roles are independent of node names.

Start Client first in one terminal:

```sh
./build/glove client --transport tcp --bind 127.0.0.1 --port 9000
```

Then start Primary in another terminal:

```sh
./build/glove primary --transport tcp --peer 127.0.0.1 --port 9000
```

The three shown options are required for this two-node mode. Only `tcp` is supported;
addresses must be numeric IPv4 and ports decimal integers in 1–65535. Missing,
unknown, duplicate, or conflicting options fail with a nonzero exit. No options
are accepted by mock or help commands. Bare Primary remains local;
bare Client retains its startup message. C++ callers can select either Primary
hand with `Primary(hand, peer, port)`; the CLI defaults to Right and optionally
accepts `--hand left|right`.

Primary updates its owned pair from the shared fixtures and sends five states,
then closes its connection. It prints no state output in TCP mode. Client parses
complete payloads and prints exactly the five lines shown in the mock example
above, in order, then exits successfully at clean EOF. It accepts any number of
valid frames; it does not infer a fixed message count from the dictionary.

The node payload is exactly ten comma-separated `0`/`1` values enclosed in
brackets, with no whitespace or trailing content. Primary appends one LF per
payload. The internal state codec handles payloads only; TCP handles framing.
Empty payloads, malformed states, oversized frames, and incomplete final frames
are errors: Client exits nonzero and reports to stderr without printing the
invalid state or fabricating a replacement. Earlier valid states may already be
printed. Connection errors also reach stderr with nonzero exit status. There are
no readiness banners or other diagnostics on stdout.

The two- and three-process links have been tested on macOS localhost only.
These checks establish desktop TCP behavior; see Project status for hardware and
BLE verification scope.

## Experimental three-node TCP simulation

```text
Secondary (one Glove, TCP client)
  → five-state payload → Primary (TCP server, one GlovePair, TCP client)
  → ten-state payload → Client (TCP server, validated stdout)
```

Start these commands in separate terminals, in order: Client, Primary, Secondary.
Primary connects outbound before listening inbound; allow each listener to start
before launching its sender. There is no automatic connection retry.

```sh
./build/glove client --transport tcp --bind 127.0.0.1 --port 9000
./build/glove primary --transport tcp --hand right --bind 127.0.0.1 --listen-port 9001 --peer 127.0.0.1 --port 9000
./build/glove secondary --transport tcp --hand left --peer 127.0.0.1 --port 9001
```

For Left Primary and Right Secondary, use `--hand left` on Primary and
`--hand right` on Secondary; leave the endpoints unchanged. Both assignments
produce exactly the five Client lines shown in the mock example above. Primary
and Secondary emit no stdout in TCP mode. Bare Secondary keeps its startup message.

Primary defaults to Right if `--hand` is omitted. Secondary requires `--hand`
in TCP mode. Primary's `--bind` and `--listen-port` must appear together and
select three-node mode; omitting both preserves the existing two-node mode.
`--port` on Primary is the outbound Client port, while `--listen-port` is its
inbound Secondary port. All TCP network modes require `--transport tcp`, their
address option, and `--port`. Addresses must be numeric IPv4; both ports must
be decimal integers from 1 through 65535. Unknown, duplicate, missing, invalid,
or incompatible options fail before dispatch. Hand-only local options are not supported.

The experimental Secondary payload is exactly `S5 L 10100` or `S5 R 00010`:
`S5`, one space, `L`/`R`, one space, exactly five binary digits in physical-hand
order. The sender appends one LF. The codec handles payloads without LF and
rejects unknown types/hands, nonbinary digits, incorrect separators, empty
payloads, extra whitespace, and missing/trailing content. Primary also rejects
a hand matching its own. Parsing completes before either pair half is updated.
This desktop format is not the final ESP32 protocol.

Secondary updates its owned glove from the shared five-case fixtures. Primary
pairs the nth accepted remote sample with the nth local fixture, routes each
half by physical hand, and sends the pair in unchanged left-then-right order.
Remote data is used as received; it need not match the fixture. No combined
state is emitted before a valid remote sample. This deterministic pairing is
not synchronized sensor sampling, freshness validation, or chord timing.

Primary requires exactly five accepted samples followed by clean upstream EOF.
Early EOF, incomplete/oversized frames, malformed payloads, wrong hands, and
additional samples fail the session. Primary closes its outbound socket on
success and on failure; transport closes the inbound sockets. Errors propagate
to stderr and nonzero exit status on the failing node. Previously forwarded
states remain valid output; no replacement or artificial zero state is emitted.

Client still treats any clean EOF as success: on an upstream failure it may
print a valid prefix (or all five states before a rejected sixth) and exit zero.
Its exit code alone does not establish end-to-end success. Likewise, Secondary
send success does not acknowledge Primary acceptance. Check all three exit
codes. There is no application status/acknowledgment protocol. An upstream peer
that keeps its connection open can block completion even after five samples.
Automatic reconnection, live streaming, and production freshness policies remain
future work.

## TCP transport behavior

The temporary framing contract is one UTF-8-compatible text message per LF-ended
line. The receiver delivers payload bytes without LF, in connection order, once
per complete frame. It does not validate UTF-8 or interpret payload contents;
CR and other non-LF bytes are preserved. Empty lines deliver empty strings.
The maximum payload is **4,096 bytes**, excluding LF. Larger frames and EOF with
an unfinished frame throw errors. Clean EOF after complete frames (including no
frames) returns normally. Complete earlier frames may already have been delivered
when a later framing error occurs. Receive buffering is bounded.

`TcpClient::send(bytes)` preserves bytes exactly: callers must append LF for each
message. Calls may contain partial frames or several frames. A successful send
means bytes were accepted by the local socket, not acknowledged by the application.
The client is neither copyable nor movable; its destructor closes the connection.
Repeated `disconnect()` is safe. Sending while disconnected or connecting twice
is an error. Send failures close the connection and propagate; interrupted sends
retry. Failed or interrupted connects close their temporary socket, allowing a
fresh explicit connection attempt. Do not automatically replay a failed message:
a prefix may have reached the peer.

`TcpServer::run(onMessage)` accepts one connection and invokes
`onMessage(const std::string&)` synchronously for each complete line. Copy the
message if retaining it after the callback. Callback exceptions propagate and
close both sockets. Socket and framing errors also propagate. Transport code
prints neither payloads nor diagnostics. Each run creates and closes its sockets;
`SO_REUSEADDR` supports repeated local runs, not simultaneous ownership of a port.

This implementation uses POSIX IPv4 sockets with numeric addresses. SIGPIPE is
suppressed per socket on macOS (`SO_NOSIGPIPE`) or per send where `MSG_NOSIGNAL`
is available; no process-wide signal handler is installed. macOS is verified;
other POSIX platforms are not verified, and platforms without either mechanism
are rejected at compile time. Windows support is not implemented. There are no
timeouts, cancellation, reconnection, concurrency, or application acknowledgments;
blocking calls can wait indefinitely. This is not a production transport or the
final ESP32 wireless packet format. The UDP and optional BLE experiments are described below.

## Experimental UDP transport foundation

`UdpSender` (`include/transport/udp/sender.hpp`) and `UdpReceiver`
(`include/transport/udp/receiver.hpp`) provide a POSIX IPv4 datagram transport,
now linked into `build/glove` for the explicit UDP node mode below. For isolated
transport experiments, compile `src/transport/udp/sender.cpp` and
`src/transport/udp/receiver.cpp` with a separate experimental caller using
`-std=c++20 -Wall -Wextra -Wpedantic -Iinclude` and the macOS SDK described above.
No external libraries are needed.

```cpp
UdpSender sender("127.0.0.1", 9002);
for (int i = 0; i < 5; ++i) {
    sender.send(std::string("a\0b", 3)); // One binary datagram, no LF appended.
}

// In a separate process, start the receiver before sending:
UdpReceiver receiver("127.0.0.1", 9002);
receiver.run(5, std::chrono::milliseconds(2000),
             [](const std::string& payload) { /* consume one datagram */ });
```

The example receiver expects five datagrams and reports an incomplete session
if fewer arrive within its budget.
Addresses must be numeric IPv4 strings without embedded NULs. Constructor ports
are integers validated in 1..65535. Sender construction creates an unconnected
socket; each receiver `run()` creates, binds, and closes a fresh socket. Start
receiving before sending. Copying and moving either object are disabled. Socket
ownership reuses the existing internal POSIX descriptor owner, which has no
TCP-specific socket behavior; UDP does not use TCP stream framing.

Each `send()` sends exactly one datagram of at most **4,096 bytes**. Empty
payloads, embedded NULs, and LF bytes are preserved. Every received datagram is
one callback, with no splitting, concatenation, or delimiter processing. A
4,096-byte datagram is not guaranteed to avoid IP fragmentation on real networks.
Oversized outgoing payloads and truncated incoming datagrams throw
`std::length_error`; truncated prefixes are never delivered to callbacks.

`run(count, timeout, callback)` requires a positive count, a nonempty callback,
and a total session timeout of 1..INT_MAX milliseconds. It returns only after
that many callbacks complete within the budget. A monotonic deadline bounds
waiting even across interrupted receives; missing datagrams raise an incomplete
session error reporting the received and expected counts. The timeout is not
reset per packet. Callbacks run synchronously, so a slow or blocked callback
cannot be preempted; the deadline is checked again after it returns. Copy the
payload if retaining it after the callback. Earlier callbacks are not undone
when a later error occurs. Additional queued datagrams are discarded when the
finite session closes. Concurrent callers must not share an instance.

System failures preserve their error codes through `std::system_error`;
configuration errors throw `std::invalid_argument`, and timeout/short-send
conditions throw `std::runtime_error`. Callback exceptions propagate unchanged.
Receiver sockets close on success and exceptions; the sender socket remains
owned until destruction, including after a failed send. No diagnostic or payload
output is printed by the transport. EINTR retries a send only when the syscall
reports interruption before transmission; short sends and other errors are not
replayed. There are no application retransmissions or acknowledgments.

UDP does **not guarantee delivery or ordering and does not prevent duplicates**.
There is no connection establishment or EOF. Sending successfully only means the
local socket accepted the datagram; an absent receiver may produce no immediate
error. The finite count counts arrivals, including duplicates, and does not prove
that all distinct intended samples arrived. The receiver accepts datagrams from
any peer; there is no peer authentication, filtering, or sequence tracking.

Verified scope is macOS localhost, not wireless performance. Other POSIX systems
are unverified; Windows and ESP32 compatibility are not implemented or claimed.
UDP is experimental and has not been selected as the production protocol.

## Experimental three-node UDP simulation

```text
Secondary (one Glove, UDP sender)
  → indexed five-state datagram → Primary (UDP receiver, one GlovePair, UDP sender)
  → indexed ten-state datagram → Client (UDP receiver, validated ordered stdout)
```

Start Client, then Primary, then Secondary in separate terminals. Each receiver
must be bound before its sender transmits. There is no readiness handshake or
banner; an external read-only socket inspection (for example `lsof -nP -iUDP:9000`)
can confirm the listener. Manual startup must also fit within the receive budget.
For interactive launches, the commands below use 30 seconds; the default is
5,000 milliseconds when `--timeout-ms` is omitted.

```sh
./build/glove client --transport udp --bind 127.0.0.1 --port 9000 --timeout-ms 30000
./build/glove primary --transport udp --hand right --bind 127.0.0.1 --listen-port 9001 --peer 127.0.0.1 --port 9000 --timeout-ms 30000
./build/glove secondary --transport udp --hand left --peer 127.0.0.1 --port 9001
```

For Left Primary / Right Secondary, change Primary to `--hand left` and
Secondary to `--hand right`. Primary defaults to Right; network Secondary always
requires a hand. Both configurations print exactly the five mock lines above,
including the fourth asymmetric state. Primary and Secondary print no stdout.
Bare commands and all existing TCP commands remain compatible.

UDP Primary requires both inbound options (`--bind`, `--listen-port`) and both
outbound options (`--peer`, `--port`). There is no two-node UDP fixture mode.
UDP Client requires `--bind` and `--port`; UDP Secondary requires `--hand`,
`--peer`, and `--port`. All require `--transport udp`. Only UDP Primary and Client
accept `--timeout-ms`, a decimal integer from 1 through 2147483647; it is rejected
for TCP and Secondary. Address/port, missing-value, duplicate-option, and unknown
argument validation applies before dispatch. Both links use the selected transport.

### Indexed datagrams and finite completion

Each datagram has the exact experimental envelope `U1 <index> <payload>`:
`U1`, one ASCII space, a single digit **0..4**, one ASCII space, then the unchanged
state payload. Examples for the fourth fixture are:

```text
U1 3 S5 L 10100
U1 3 [1,0,1,0,0,0,0,0,1,0]
```

These displayed lines represent separate datagrams: **no LF or CR is sent**.
Index 0 is the first shared fixture, and index 4 is the fifth. The five-state
and ten-state codecs remain unchanged; the UDP envelope is separate node-level
serialization. Unknown versions, invalid indices (including signs, multiple
digits, and values outside 0..4), incorrect spacing, and malformed payloads fail.
The 4,096-byte transport limit includes the entire envelope.

Secondary sends each fixture once with its index. Primary validates the envelope,
five-state payload, physical hand, and uniqueness before changing either half of
its owned pair. It uses the **indexed** local fixture, never arrival position,
and forwards the combined state with the same index. Reordered samples therefore
retain their correct local/remote pairing. Primary forwards valid samples as they
arrive; Client buffers at most five validated snapshots and prints them in index
order **only after all five distinct indices are received within its deadline**.
On Client failure, no buffered snapshots are printed and no replacements are made.

Duplicate indices observed during an active session are fatal, even if their
payloads match. Invalid, wrong-hand, duplicate, oversized, or truncated packets
fail immediately rather than counting toward completion. A missing packet leads
to a timeout and nonzero exit. Diagnostics include the missing sample indices;
previous valid forwards from Primary cannot be recalled. Each receiving node has
its own total session deadline, starting when its receive run begins, not when
Secondary starts. Packet arrivals do not extend it. Sockets close on success,
callback errors, and timeout.

Success means five distinct valid indices were accepted locally. Receivers close
immediately on completion; later packets, including duplicates, are not inspected.
UDP has no EOF. There are no acknowledgments, retransmissions, sequence-based
retries, or delivery guarantees. Primary/Secondary may exit zero with an absent
downstream receiver. Check all three exit codes and Client output; sender success
alone does not establish delivery. Loss or a node failure can leave another node
waiting until its own timeout.

This envelope has no session identifier, peer authentication, or source filtering.
Use isolated endpoints and one sender per link; stale packets or multiple senders
can contaminate a session. Indices solve fixture pairing and completion checks,
not production reliability or sensor synchronization. UDP can lose, duplicate,
and reorder datagrams; localhost success does not establish Wi-Fi performance.
TCP remains unchanged, including LF framing and its documented EOF semantics.
BLE and mixed-link operation are documented below.

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

## Experimental BLE API and node integration

Every BLE path uses real Core Bluetooth. An absent peer produces a timeout, never
substitute data. See Project status for the current verification boundary.

### C++ API and platform boundary

`BleCentral::receive(const BleConfig&, size_t count)` returns `BleReceived`, containing
an initial `readValue` and an owned `vector<string> messages` of exactly `count`
notifications. Reads are separate from notification messages. No partial result is
returned on error. `BlePeripheral::publish(const BleConfig&, const vector<string>&)`
publishes an immutable batch: reads return the first message, then notifications
send each message once after that peer reads and subscribes. The selected peer is
exclusive for this session. Strings preserve explicit lengths, embedded NULs, and
empty payloads. The transport imposes no text encoding or newline framing.

`BleConfig` supplies canonical 128-bit service/characteristic UUID strings and a
1..60000 ms per-stage timeout (default 10000). Batches contain 1..64 messages, each
0..512 bytes, additionally limited by the peer's `maximumUpdateValueLength`.
Oversized messages fail; there is **no fragmentation or truncation**. A finite batch
is queued under one deadline, and reception uses one notification-stage deadline;
packet arrivals and backpressure retries do not restart those deadlines.

Both objects are noncopyable/nonmovable. Each call owns its ARC manager/delegate
session, requires the macOS main thread, services the native run loop, and blocks
until completion/failure. No application callback executes inside Core Bluetooth;
received bytes are returned for subsequent application validation. C++ allocation
failures inside Central's payload callback are captured as `std::exception_ptr`,
stop further processing, and are rethrown with their original type only after the
synchronous call cleans up. Failure recording performs no C++ allocation or error
formatting. Emergency cleanup requests cancellation without claiming a confirmed
physical disconnect. Objective-C exceptions are not translated into C++ errors. Exceptions report
configuration, initialization, permission, GATT, capacity, timeout, or teardown errors.
There is no public concurrent start/stop API, reconnect loop, or live-stream API.

Apple types are confined to the macOS `.mm` implementations and the internal
`include/transport/ble/platform/macos/session.hpp` header. Public BLE API headers
remain C++20. The `.mm` files define C++ methods directly, without forwarding wrappers.
The existing `run(timeout)` methods remain fixed `GLOVER_BLE_TEST` compatibility
experiments implemented through these same APIs.

### Experimental GATT contract

- Default service: `8A6E0001-5A2B-4D91-9CF0-6E46AFA58F21`.
- Default characteristic: `8A6E0002-5A2B-4D91-9CF0-6E46AFA58F21`.
- Peripheral advertises and publishes one dynamic, readable/notifiable characteristic.
- Central scans, connects, discovers, reads once, then subscribes.
- Peripheral sends one complete application message per notification. It retains the
  current message on queue backpressure and retries only after the readiness callback.
- Writes are unsupported by this unidirectional contract (ATT Write Not Permitted).
  No additional characteristic is needed for the implemented direction.
- Queue acceptance and unsubscribe are **not application delivery acknowledgments**.
  Central completion confirms reception, not node-level codec acceptance at the sender.

Node notifications use a separate envelope `B1 <index> <payload>`: exactly `B1`, one
space, one digit 0..4, one space, and the unchanged five-state or ten-state codec.
There is no trailing LF and no UDP `U1` envelope on BLE. Examples:

```text
B1 3 S5 L 10100
B1 3 [1,0,1,0,0,0,0,0,1,0]
```

Five-state messages retain `L`/`R` physical hand identity; combined messages retain
left-then-right ordering. The receiving node validates indices, uniqueness, exact
codec syntax, and (at Primary) opposite hand. It validates a complete batch before
mutating the pair or emitting output. Read values are informational and do not count
as a sample. The same first message can therefore appear in the read and notification.

Five-state notifications are 15 bytes; ten-state notifications are **26 bytes** with
the envelope. Link B requires a negotiated notification capacity of at least 26
bytes (normally ATT MTU at least 29). A peer limited to 20 bytes fails explicitly.
The application does not control MTU negotiation. There are no session identifiers,
authentication, retransmissions, freshness timestamps, or sensor synchronization.
Isolate one sender per link. Late extra packets after completion are not inspected.

### Roles and independent links

| Link | Sender / BLE role | Receiver / BLE role |
|---|---|---|
| A: Secondary → Primary | Secondary / Peripheral | Primary / Central |
| B: Primary → Client | Primary / Peripheral | Client / Central |

These are explicit experimental link roles, independent of physical hand and subject
to team/hardware confirmation. Primary first receives and validates five upstream
samples, combines them with indexed local fixtures, closes the inbound session, then
opens/publishes the outbound batch. This is sequential batch forwarding, not a
concurrent BLE bridge. It requires no extra threads.

Supported Primary paths: BLE→TCP, BLE→UDP, BLE→BLE, TCP→BLE, UDP→BLE, and local fixtures→BLE.
Independent link mode requires at least one BLE link. Existing TCP-only and UDP-only
commands keep their original streaming/buffering behavior. TCP legs retain their
existing blocking semantics: BLE stage timeouts do **not** impose a socket deadline.
UDP inbound budgets in independent mode are 1..60000 ms, default 5000; old UDP commands
retain their existing timeout range. Start network receivers before their senders.
BLE receivers need enough discovery budget to cover upstream batch collection.

Two BLE links require distinct service UUIDs to avoid selecting the wrong role. Both
endpoints on each link must agree. The default UUIDs are preserved for a single link;
an explicitly selected alternate service such as `8A6E0003-5A2B-4D91-9CF0-6E46AFA58F21`
can distinguish Link A. The same characteristic UUID can be reused within each service.
This does not authenticate devices. Three BLE roles on one Mac are not assumed to
constitute independent radio endpoints.

### Node configuration and execution modes

`NodeLink` configures one independent Primary link. TCP and UDP use its numeric
IPv4 `address` and `port`; BLE uses `ble.service`, `ble.characteristic`, and
`ble.timeout`. Irrelevant transport settings are ignored. `PrimaryLinks::inbound`
is optional (absence selects local two-hand fixtures); `outbound` is required.
At least one independent link must be BLE. Physical hand is configured separately.

Independent UDP reception uses `NodeLink::udpTimeout`, an overall receive-session
budget of 1..60000 ms. Its direct-C++ default is **10000 ms**, preserving the old
effective default. The CLI still supplies **5000 ms** when `--in-timeout-ms` is
omitted, or the explicitly requested valid duration. UDP sending ignores this
receive-only field, including its range. TCP ignores it and remains blocking.
BLE continues using `BleConfig::timeout`: **10000 ms** by default, 1..60000 ms
**per stage**. Legacy UDP constructors and `--timeout-ms` remain unchanged:
5000 ms by default and 1..INT_MAX ms for reception.

**Direct-C++ migration:** callers previously setting `link.ble.timeout` for UDP
must now set `link.udpTimeout`. There is no compatibility fallback to BLE settings.
The new field is appended to `NodeLink`; existing field order is retained, but
its object layout changes and dependents must be rebuilt. Existing aggregate
initializers keep their prior fields; custom UDP timeout assignments must still
be migrated explicitly. External teammate callers have not been inspected.

| Constructor | Execution mode |
|---|---|
| `Primary(hand)` | Local five-sample simulation; both hands from fixtures; default hand Right. |
| `Primary(hand, peer, port)` | Two-node TCP; both hands from fixtures. |
| `Primary(hand, bindAddress, listenPort, peer, port, transport, timeout)` | Legacy three-node TCP or UDP; receives the opposite hand and combines it with local fixtures. Timeout applies only to UDP reception. |
| `Primary(hand, PrimaryLinks)` | Independent links containing BLE; receives/validates a complete batch before forwarding, or sends local fixtures when inbound is absent. |
| `Secondary()` | Left Secondary startup smoke test. |
| `Secondary(hand, peer, port, transport)` | Legacy TCP or UDP transmission of one hand's fixtures. |
| `Secondary(hand, BleConfig)` | BLE publication of one hand's fixture batch. |
| `Client()` | Startup smoke test. |
| `Client(bindAddress, port, transport, timeout)` | Legacy TCP streaming output or UDP complete-batch output; timeout applies only to UDP. |
| `Client(BleConfig)` | BLE complete-batch validation and output. |

Constructors select mutually exclusive execution modes; they do not start network
sessions. Primary still owns a `GlovePair`, Secondary owns one `Glove`, and Client
owns neither. These modes use the shared finite five-state fixtures.

### Build and CLI

```sh
make clean
make                  # Standard C++20 build, no Apple BLE frameworks
make run              # Original mock
make ble-feasibility  # Separate macOS fixed-fixture app bundle
make ble-node         # Rebuilds the SAME build/glove with real macOS BLE support
./build/glove --help
```

`make BLE=1` also enables BLE. The Makefile tracks mode changes; plain `make` switches
back to the default build. For a BLE-enabled mock run use `make BLE=1 run`. No additional
production executable is created. The enabled executable embeds
`config/macos/glove-Info.plist`, links Foundation/CoreBluetooth, uses ARC, and is ad hoc
signed. Default builds parse BLE configuration but reject execution with a clear
`make ble-node` instruction; they never substitute a fake transport.

Single-link nodes:

```sh
./build/glove secondary --transport ble --hand left --ble-role peripheral --timeout-ms 30000
./build/glove client --transport ble --ble-role central --timeout-ms 30000
./build/glove primary --transport ble --hand right --out-timeout-ms 30000
```

The third command uses the existing local two-hand fixtures and publishes ten-state
messages. Secondary and Client accept `--service UUID`, `--characteristic UUID`,
and `--timeout-ms N`. Primary uses per-link options instead: `--in-service`,
`--out-service`, `--in-characteristic`, `--out-characteristic`, `--in-timeout-ms`,
`--out-timeout-ms`, and optional fixed-role assertions `--in-role central` /
`--out-role peripheral`. Wrong roles, unknown/duplicate options, malformed UUIDs,
missing values, and endpoint options irrelevant to the selected transport fail.

Example BLE→TCP (Secondary must be on a separate BLE-capable device):

```sh
./build/glove client --transport tcp --bind 127.0.0.1 --port 9000
./build/glove primary --hand right --in-transport ble --in-timeout-ms 30000 --out-transport tcp --peer 127.0.0.1 --port 9000
./build/glove secondary --transport ble --hand left --timeout-ms 30000
```

Example BLE→BLE with separate Link A service (requires suitable independent endpoints):

```sh
./build/glove secondary --transport ble --hand left --service 8A6E0003-5A2B-4D91-9CF0-6E46AFA58F21 --timeout-ms 30000
./build/glove primary --hand right --in-transport ble --in-service 8A6E0003-5A2B-4D91-9CF0-6E46AFA58F21 --out-transport ble --in-timeout-ms 30000 --out-timeout-ms 30000
./build/glove client --transport ble --timeout-ms 60000
```

For TCP/UDP inbound, specify `--bind ADDRESS --listen-port PORT`; for TCP/UDP
outbound, specify `--peer ADDRESS --port PORT`. Address/port options do not select a
BLE peer. Opposite hand selection remains mandatory at Secondary and validated by Primary.

`make ble-feasibility` builds
`build/ble-feasibility.app/Contents/MacOS/ble-feasibility`. It uses
`examples/ble_feasibility/Info.plist` as the bundle's `Contents/Info.plist`, retains
its feasibility identity, and is ad hoc signed. It never inherits the node's embedded
plist or `GLOVER_ENABLE_BLE` definition, whether invoked with `BLE=0` or `BLE=1`.
Both BLE targets share native sources, ARC, macOS 10.15 deployment settings, and
Foundation/CoreBluetooth; default node builds do not link these frameworks.

The fixed-fixture compatibility experiment exchanges `GLOVER_BLE_TEST` through the
same Central/Peripheral APIs. Run its executable on separate suitable endpoints:

```sh
./build/ble-feasibility.app/Contents/MacOS/ble-feasibility central --timeout-ms 10000
./build/ble-feasibility.app/Contents/MacOS/ble-feasibility peripheral --timeout-ms 10000
```

These are alternative roles for separate devices, not a same-Mac delivery test.
The executable accepts `central|peripheral [--timeout-ms N]`. `make clean` removes `build/glove`,
preserving its original target behavior. The feasibility bundle is not a production
executable or a substitute for node delivery verification.

### Authorization, lifecycle, and verification limits

The macOS adapters require Apple developer tools and Core Bluetooth. Both packaging
paths include Bluetooth usage descriptions; the executables are unsandboxed. A future
sandboxed application requires a Bluetooth entitlement. Permission depends on macOS
and the responsible Terminal/IDE host; embedding a plist does not guarantee all launch
contexts. Use normal authorization prompts/settings; no privacy bypass is implemented.
Deployment target 10.15 is a build setting, not a claim of testing all macOS versions.

After collecting its batch, Central cancels the connection **while still subscribed**
and waits for the local disconnect callback. It no longer explicitly unsubscribes
before canceling, reducing the previous teardown race. Peripheral waits for the
unsubscribe event, retains its service for a 250 ms grace period, requests advertising
stop, and observes local `isAdvertising` for up to 1000 ms before service removal.
Failure to observe stop adds a teardown error. Timers are invalidated and delegates
detached before owners leave scope. Cleanup can add up to 1.25 seconds to stage budgets.
A grace period is not proof of peer disconnect; another application's connection may
keep the physical radio link alive. Hardware ordering remains unverified.

**Software/local checks:** both standard and Objective-C++ builds; public headers;
configuration, codec, envelope, and capacity validation; node construction; legacy
mock/TCP/UDP regression; bounded no-peer failures through real node paths. This Mac
reported `allowedAlways`, `poweredOn`, scanning, service publication, and advertising
callbacks. Bounded Peripheral cleanup now observed `isAdvertising=NO` locally.

**Hardware pending:** real discovery/connection, remote GATT discovery, characteristic
read, subscription, notification delivery/backpressure, end-to-end glove messages,
physical advertising shutdown, and physical disconnection. Writes are unsupported,
not an untested implemented feature. No wireless success is inferred from local callbacks. Allocation-failure containment
checks use synthetic callback inputs solely to exercise exception handling.

### Future ESP32 adapter

Apple Objective-C++ code cannot run on ESP32. Skyler/Esteban and the team must confirm
the actual ESP32 model, firmware environment, BLE stack, radio/link roles, and payload
capacity before firmware work. An adapter must implement the selected service UUIDs,
read-before-notify sequence, one-message notification boundaries, binary lengths,
`B1` index semantics, unchanged hand/state codecs, backpressure, and finite completion.
Link B must support at least 26 notification bytes or an explicitly agreed future
fragmentation contract. Firmware, sensors, chords, keyboard input, authentication,
and production transport selection are outside this milestone.
