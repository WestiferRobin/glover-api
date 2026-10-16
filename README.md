# Glover API

## Overview

Glover is a C++20 software foundation for the SDSU CS 576 two-glove
stenography project. Desktop simulations send mock finger states through:

```text
Secondary → Primary → Client
```

Secondary represents one physical glove. Primary combines its local hand with
Secondary's hand, and Client receives and validates ten-finger snapshots.
Local simulation, two-node TCP, and three-node TCP/UDP are implemented.
Each simulation uses five mock samples; each glove has five fingers, and each
combined snapshot has ten values. Physical hardware integration remains future work.

[Build and Run](#build-and-run) · [Network Simulation](#network-simulation) ·
[Manual Verification](#manual-verification) ·
[Glove Models](#glove-models) · [State Serialization](#state-serialization) ·
[Chord Dictionary](#chord-dictionary) · [Team Integration](#team-integration)

## Requirements

- A C++20-capable compiler available as `c++`, Make, and a Unix-like shell.
- POSIX IPv4 sockets for networking.
- On macOS, installed and selected Apple developer tools providing `c++` and
  `xcrun`. The Makefile selects their SDK through `SDKROOT`.

macOS with Apple Clang and localhost networking is verified. Linux is unverified;
Windows and ESP32 support are not implemented.

## Build and Run

Clone the repository and build from its root:

```sh
git clone https://github.com/WestiferRobin/glover-api.git
cd glover-api
make
make run
```

`make` builds `build/glove` with C++20, `-Wall -Wextra -Wpedantic`, and no
third-party libraries. `make run` builds when needed and runs the local mock.
An unchanged build does not recompile. Select another compiler with
`make CXX=clang++`. To remove the executable, run `make clean`.

After building:

```sh
./build/glove              # Five mock snapshots
./build/glove mock         # Same simulation
./build/glove primary      # Same output from a local Primary
./build/glove secondary    # Startup message only
./build/glove client       # Startup message only
./build/glove --help       # Also accepts -h
```

The local simulation emits exactly:

```text
[1,0,0,0,0,0,0,0,0,1]
[0,1,0,0,0,0,0,0,1,0]
[0,0,1,0,0,0,0,1,0,0]
[1,0,1,0,0,0,0,0,1,0]
[1,1,1,1,1,1,1,1,1,1]
```

These snapshots correspond to “I,” “you,” “we,” “that,” and a space in the
dictionary, but the application does not translate them into text.
Use `./build/glove` directly when consuming state lines: Make also echoes commands.
Help goes to stdout; invalid commands/options and runtime errors go to stderr
with a nonzero exit. Mock and help commands accept no additional options.

## Network Simulation

All addresses must be numeric IPv4; ports must be decimal integers in 1–65535.
Missing, duplicate, unknown, or incompatible options are rejected before execution.
The examples use loopback and ports 9000/9001; choose unused ports if needed.

Successful runs produce the five lines above on Client, with no stdout from
Primary or Secondary. Check every process's exit code as well as Client output.
Start each command in a separate terminal from the repository root.
Two-node TCP needs **2 terminals**; three-node TCP and UDP need **3 terminals**.
These are separate processes on the same computer, not separate computers.
Open terminal tabs with Command-T in macOS Terminal, and run `cd` to your clone's
`glover-api` directory in each tab. Do not paste an entire launch sequence into
one terminal: the first receiver waits for the other processes.

Client and relay Primary initially wait silently; that is expected. Keep them
running while starting the next terminal. On successful completion, all processes
return to their shell prompts. Immediately run `echo $?` in **each** terminal;
expect `0`. Run it before `clear` or any other command, which would replace the
exit status you want to inspect.

### Two-Node TCP

Primary supplies fixtures for both hands and connects to Client.

Start Client first, then Primary after Client is listening:

```sh
# Terminal 1 — Client; leave running, then switch to Terminal 2.
./build/glove client --transport tcp --bind 127.0.0.1 --port 9000
# Terminal 2 — Primary.
./build/glove primary --transport tcp --peer 127.0.0.1 --port 9000
```

Two-node mode supports TCP only. Primary defaults to the right hand; add
`--hand left` to select the left. Either assignment produces the same output.
Client exits at clean EOF after Primary sends its five snapshots and disconnects.

### Three-Node TCP

Start Client, then Primary, then Secondary. Primary connects outbound before
listening for Secondary; wait for each listener before starting its sender.
There is no automatic connection retry.

```sh
# Terminal 1 — Client; leave running, then switch to Terminal 2.
./build/glove client --transport tcp --bind 127.0.0.1 --port 9000
# Terminal 2 — Primary; leave running, then switch to Terminal 3.
./build/glove primary --transport tcp --hand right --bind 127.0.0.1 --listen-port 9001 --peer 127.0.0.1 --port 9000
# Terminal 3 — Secondary.
./build/glove secondary --transport tcp --hand left --peer 127.0.0.1 --port 9001
```

For left Primary/right Secondary, change Primary to `--hand left` and Secondary
to `--hand right`; leave the endpoints unchanged. Primary's `--port` is outbound
to Client, while `--listen-port` is inbound from Secondary. Its `--bind` and
`--listen-port` must appear together; omitting both selects two-node mode.
Network Secondary requires `--hand`; Primary defaults to right. Hand-only
options are not supported for local commands.

Primary pairs each accepted remote sample with the next local fixture and routes
both by physical hand. Received states need not match the fixtures. It requires
exactly five samples followed by clean upstream EOF. Wrong hands, malformed
messages, early EOF, and extra samples fail the session.

Client accepts any number of valid frames and treats clean EOF as success.
If Primary fails, Client may print a valid prefix—or all five snapshots before
Primary rejects a sixth—and still exit zero. Secondary send success likewise
does not confirm Primary acceptance. TCP can wait indefinitely, including when
Secondary leaves its connection open after five samples.

### Three-Node UDP

Start Client, then Primary, then Secondary. Each receiver must bind before its
sender transmits. There is no readiness handshake; for example,
`lsof -nP -iUDP:9000` can confirm the Client socket.

```sh
# Terminal 1 — Client; leave running, then switch to Terminal 2.
./build/glove client --transport udp --bind 127.0.0.1 --port 9000 --timeout-ms 30000
# Terminal 2 — Primary; leave running, then switch to Terminal 3.
./build/glove primary --transport udp --hand right --bind 127.0.0.1 --listen-port 9001 --peer 127.0.0.1 --port 9000 --timeout-ms 30000
# Terminal 3 — Secondary; start all three within 30 seconds.
./build/glove secondary --transport udp --hand left --peer 127.0.0.1 --port 9001
```

For left Primary/right Secondary, change the two `--hand` values as in TCP.
UDP Primary requires both inbound and outbound endpoints; there is no two-node
UDP fixture mode. Secondary requires its hand and outbound endpoint.

The default UDP receive timeout is **5000 milliseconds**. Primary and Client
accept `--timeout-ms` from 1 through 2147483647; TCP and Secondary reject it.
The examples use 30000 ms to allow manual startup. Each receiver's total deadline
starts when its receive run begins, and incoming datagrams do not restart it.
Missing samples cause failure with missing-index diagnostics. Client prints
nothing from an incomplete or invalid UDP batch.

Indices preserve pairing when packets arrive out of order. Primary forwards
validated samples as they arrive; Client prints them in index order only after
all five distinct indices arrive within its deadline. A duplicate observed during
an active session is fatal, even with identical data. Receivers close immediately
after completion, so later packets are not inspected.

UDP send success does not establish delivery: Primary or Secondary can exit zero
without a downstream receiver. Use isolated endpoints and one sender per link.
The envelope has no session identifier or source filtering, so stale packets or
multiple senders can contaminate a session.

## Manual Verification

Wesley manually checked the desktop workflow on macOS on October 15, 2026:
clean compilation without warnings, exact local mock output, two-node TCP output,
three-node TCP/UDP with both hand assignments, UDP timeout handling, invalid-command
rejection, and help. Both hand assignments produced the expected five snapshots;
exit codes were explicitly checked for reversed-hand TCP and both UDP assignments.
This records a manual check of the working tree, not an automated test suite or
hardware validation. Rerun these checks after changes.

1. In one terminal, run `make clean`, `make`, then `./build/glove`.
   Compare all five lines with [Build and Run](#build-and-run).
2. Run [Two-Node TCP](#two-node-tcp) in two terminals.
3. Run [Three-Node TCP](#three-node-tcp) in three terminals, then repeat with
   left Primary/right Secondary.
4. Run [Three-Node UDP](#three-node-udp) in three terminals, then repeat with
   left Primary/right Secondary. Start each complete sequence within 30 seconds.
5. For each network run, compare Client output with the same five expected lines
   and check `echo $?` in every terminal immediately after completion. All should
   be `0`; Primary and Secondary should print no state lines.
6. With all senders stopped, use one terminal for these error/help checks:

```sh
./build/glove client --transport udp --bind 127.0.0.1 --port 9000 --timeout-ms 1000
echo $?
```

Expect no state output, exit `1`, and this error after about one second:

```text
Error: UDP session incomplete: received 0 of 5 datagrams before timeout; missing sample indices: 0,1,2,3,4
```

```sh
./build/glove invalid
echo $?
./build/glove --help
echo $?
```

The invalid command prints `Unknown command: invalid`, help, and exit `1`.
Explicit help prints usage without an error and exits `0`.

If a run gets stuck, press Control-C in its terminals before restarting the
sequence. `Connection refused` usually means the TCP listener was not ready;
`Address already in use` means another socket owns the endpoint. Stop the old
process or choose unused ports. A UDP timeout during manual startup means you
should restart the complete sequence and launch the senders before the deadline.

## Architecture

| Module | Responsibility | Entry points |
|---|---|---|
| `glove/` | Glove models and finger states | `include/glove/glove.hpp`, `include/glove/pair.hpp` |
| `nodes/` | Primary, Secondary, Client execution | `include/nodes/primary.hpp`, `secondary.hpp`, `client.hpp`, `node.hpp` |
| `transport/` | TCP, UDP, sockets, framing, serialization | `include/transport/tcp/`, `include/transport/udp/`, `include/transport/detail/state_codec.hpp` |
| `cli/` | Argument parsing and configuration | `include/cli/parser.hpp`, `src/cli/parser.cpp` |

Implementations live under the corresponding `src/` directories.
`src/main.cpp` runs parsed actions and reports application errors.
`cli::parse()` constructs but does not run nodes; `cli::printHelp()` renders help.
Shared fixtures are in `include/mock.hpp` and `src/mock.cpp`.
Generated files belong under the ignored `build/` directory.

Primary owns a `GlovePair`; Secondary owns one `Glove`. Client owns neither.
All nodes implement `Node::run()`. TCP node names are independent of socket
roles: Secondary connects to Primary, and Primary connects to the listening Client.

C++ callers can use `Primary(Hand hand = Hand::Right)` locally or
`Primary(hand, peer, port)` for two-node TCP. The relay constructor takes
`hand, bindAddress, listenPort, peer, port, transport, timeout`.
Transport defaults to `NodeTransport::Tcp`; the shared UDP timeout default is
declared in `include/nodes/network.hpp`.

## Glove Models

One glove stores `std::array<bool, FINGER_COUNT>`, with `FINGER_COUNT == 5`.
`false`/`0` means inactive; `true`/`1` means active.

| Index | Left hand | Right hand |
|---|---|---|
| 0 | Pinky | Thumb |
| 1 | Ring | Index |
| 2 | Middle | Middle |
| 3 | Index | Ring |
| 4 | Thumb | Pinky |

`GlovePair::getState()` concatenates left then right without reversing either
array, returning `std::array<bool, PAIR_FINGER_COUNT>` with
`PAIR_FINGER_COUNT == 10`. Physical `Hand` identity and communication
`GloveRole` are independent: either hand can be Primary.

`Glove(Hand, GloveRole)` initializes all fingers inactive and rejects invalid
hand/role values with `std::invalid_argument`. `setFinger(Finger, bool)` and
`getFinger(Finger)` use anatomical names and map them to the hand-specific slot;
invalid fingers throw `std::out_of_range`. `setState()` and `getState()` use
the array order above. `getHand()` and `getRole()` return the identities.

`GlovePair(leftGlove, rightGlove)` owns copies and requires Left then Right,
with exactly one Primary and one Secondary; otherwise it throws
`std::invalid_argument`. Later changes to the originals do not affect the pair.
Use `setLeftState()` and `setRightState()` to update its copies.
`getLeftState()`, `getRightState()`, and `getState()` return snapshots by value.

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

## State Serialization

The shared codec is declared in `include/transport/detail/state_codec.hpp`
and implemented in `src/transport/state_codec.cpp`:

```cpp
std::string serializeState(const std::array<bool, PAIR_FINGER_COUNT>& state);
std::array<bool, PAIR_FINGER_COUNT> parseState(std::string_view payload);

struct HandState {
    Hand hand;
    std::array<bool, FINGER_COUNT> state;
};
std::string serializeHandState(const HandState& sample);
HandState parseHandState(std::string_view payload);
```

Payloads have exact syntax:

| Payload | Example |
|---|---|
| Five-finger hand state | `S5 L 10100` or `S5 R 00010` |
| Combined ten-finger state | `[1,0,1,0,0,0,0,0,1,0]` |

Hand messages contain `S5`, one space, `L` or `R`, one space, and five binary
digits in physical-hand order. Combined messages contain ten comma-separated
binary values in brackets. Parsers reject incorrect lengths, separators, hands,
nonbinary values, extra whitespace, and trailing content. Primary additionally
rejects a received hand matching its own before changing the pair.

Example inside a component's `main()`:

```cpp
#include <transport/detail/state_codec.hpp>

// Inside main():
const std::string line = "[1,0,1,0,0,0,0,0,1,0]";
auto state = parseState(line);
auto payload = serializeState(state);
auto hand = parseHandState("S5 L 10100");
auto handPayload = serializeHandState(hand);
(void)payload;
(void)handPayload;
```

`line` contains the payload without TCP's terminating LF. Compile the codec
alongside your own component:

```sh
mkdir -p build
c++ -std=c++20 -Wall -Wextra -Wpedantic -Iinclude \
    path/to/component_main.cpp src/transport/state_codec.cpp \
    -o build/codec-demo
```

Use the macOS SDK setup above. Add the model source files too if your component
constructs or calls `Glove` or `GlovePair`.

### TCP Framing and Transport API

The APIs are in `include/transport/tcp/client.hpp` and
`include/transport/tcp/server.hpp`. Framing lives in
`include/transport/tcp/detail/framing.hpp`; socket ownership is shared through
`include/transport/detail/socket.hpp`.

TCP adds one LF after each payload; the codec itself does not add delimiters.
`TcpServer::run(onMessage)` accepts one connection and delivers each complete
frame without LF. It preserves CR and other non-LF bytes, does not validate
UTF-8, and delivers empty frames; node codecs reject invalid payloads.

The maximum frame payload is **4096 bytes**, excluding LF. Oversized frames and
EOF with an unfinished frame fail. Clean EOF, including no frames, returns
normally. Earlier valid frames may already have been delivered when a later
error occurs. Receive buffering is bounded.

`TcpClient::send(bytes)` sends bytes unchanged; callers may send partial or
multiple frames and must supply LF boundaries. It retries interrupted sends,
closes on send failure, and rejects sending while disconnected or connecting
twice. A failed/interrupted connect closes its temporary socket and permits a
fresh explicit attempt. Do not automatically replay failed messages: a prefix
may already have reached the peer. `disconnect()` is safe to repeat; destruction
also closes the connection. `TcpClient` is neither copyable nor movable.

Callbacks are synchronous; copy a payload if retaining it. Callback, socket, and
framing errors propagate and close the server's sockets. Each server run creates
fresh sockets; `SO_REUSEADDR` permits repeated local runs, not simultaneous port
ownership. Transport classes print neither messages nor diagnostics.
SIGPIPE is suppressed with `SO_NOSIGPIPE` on macOS or `MSG_NOSIGNAL` where
available; platforms with neither mechanism are rejected at compile time.

### UDP Envelope and Transport API

Node datagrams use `U1 <index> <payload>`: one space between fields, a single
index digit from 0 through 4, and the unchanged state payload. Index 0 identifies
the first mock sample. No LF or CR is appended:

```text
U1 3 S5 L 10100
U1 3 [1,0,1,0,0,0,0,0,1,0]
```

Unknown versions, signed/multidigit/out-of-range indices, incorrect spacing, and
invalid state payloads fail. Primary combines each remote sample with the
**indexed** local fixture, not the next arrival. Envelope declarations live in
`include/transport/udp/detail/udp_envelope.hpp`; `UdpSample::payload` is a
view into the original datagram and must not outlive it.

`UdpSender(address, port)` and `UdpReceiver(address, port)` require numeric IPv4
without embedded NULs and an integer port in 1–65535. Both are noncopyable and
nonmovable. Sender construction creates an unconnected socket, owned until
destruction even after send failure. Each receiver run creates, binds, and closes
a fresh socket.

`UdpSender::send(bytes)` sends one datagram of at most **4096 bytes**, including
any envelope. Empty payloads, embedded NULs, and LF bytes are preserved by the
transport. There is no stream framing. Oversized sends and truncated receives
throw `std::length_error`; truncated prefixes are never delivered.
A 4096-byte datagram can require IP fragmentation on real networks.
Each received datagram produces one callback. UDP has no connection handshake or EOF.

`UdpReceiver::run(count, timeout, callback)` requires a positive count, a nonempty
callback, and a total timeout in 1..INT_MAX milliseconds. See
[Three-Node UDP](#three-node-udp) for deadline and node completion rules.
The monotonic deadline also bounds interrupted waits; timeout errors report
received and expected datagram counts.
Callbacks run synchronously and cannot be preempted; the deadline is checked
after they return. Copy payloads if retaining them. Earlier callbacks are not
undone on failure. Do not share an instance between concurrent callers.

The transport counts arrivals, including duplicates; node logic validates
unique indices. Extra queued datagrams are discarded on completion.
System errors retain codes in `std::system_error`, configuration errors use
`std::invalid_argument`, and timeout/short-send errors use `std::runtime_error`.
Transport callbacks propagate exceptions unchanged and receiver sockets close
on success or failure. Nodes add missing-index context to receive errors.
Interrupted sends retry only when no data was transmitted; other failures and
short sends are not replayed. UDP transport prints no diagnostics.

For standalone transport callers, include `include/transport/udp/sender.hpp`
and `include/transport/udp/receiver.hpp`, then compile their corresponding
`src/transport/udp/sender.cpp` and `receiver.cpp` with the same C++20/include/SDK
settings used above. No external libraries are required.

## Chord Dictionary

The agreed dictionary contains 20 words and one space. Patterns use the same
left-to-right order as the ten-value arrays; compact bit strings are table
notation, not CLI payload syntax. The application does not execute this lookup.

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

“space” means one literal space character (`" "`). The all-zero state has no
mapping. Repeated snapshots alone do not specify repeated text insertion;
chord completion, held-state repetition, and unknown-chord handling belong to
the recognition component.

## Team Integration

| Teammate | Input/output and usable interface | Remaining responsibility |
|---|---|---|
| Skyler — Sensors | Produce five logical values per hand using `Glove::setState()` or `setFinger()` in `include/glove/glove.hpp` | Sensor acquisition, thresholds, and calibration |
| Esteban — Secondary Communication | Transfer `HandState` with `serializeHandState()` / `parseHandState()`; route the received hand to the corresponding `GlovePair` setter | Hardware transport adapter |
| Wesley — Nodes and Networking | Maintain `Primary`, `Secondary`, `Client`, TCP/UDP, and `cli::parse()`; see the architecture entry points | Production node/transport integration |
| Nikita — Chord Recognition | Consume `GlovePair::getState()` or `parseState()` snapshots and use the dictionary above | Recognition, chord timing, and decoded-result delivery |
| Eduardo — Computer Client | Consume decoded text; use `examples/decoded_results.txt` independently | Keyboard/computer output and the final result interface |

Fixed arrays and fixtures allow each component to develop independently.
Nodes currently use mock samples; there is no live sensor-input or chord callback
API on Primary. Pair setters and codecs provide integration boundaries for adapters.

### Working With an AI Assistant

Give your assistant this repository and your assigned component, not just a CLI
transcript. Start with the model, codec, and node headers listed above, then inspect
their implementations before proposing changes. The dictionary and wire formats
in this README are shared contracts; coordinate any changes with the team.

You can use this prompt:

```text
Read README.md and the current source in include/, src/, and examples/.
I am working on [component / teammate responsibility]. Identify the existing
input/output contract and APIs I can reuse, then implement only my requested task.
Preserve existing work and the documented finger order, dictionary mappings,
wire formats, and CLI behavior unless we explicitly agree to change them.
Do not assume sensors, BLE, chord recognition, or keyboard output already exist.
Use fixed inputs or the documented fixtures while upstream components are missing.
Run the relevant verification steps and report actual commands, output, and exit
codes. Distinguish what you executed from what you only inspected or did not test.
For manual instructions, state the number of terminals and label each command
by terminal; guide me through one test at a time. Do not commit or push unless asked.
```

### Decoded-Results Fixture

`examples/decoded_results.txt` contains five UTF-8 records:

```text
"I"
"you"
"we"
"that"
" "
```

Remove each line's outer quotes and line ending; preserve all characters inside.
There are no escapes or embedded quotes. Words have no trailing spaces, and
the last record contains exactly one space. Concatenation produces `"Iyouwethat "`;
do not insert extra spaces. Quotes and line endings are not keyboard input.

This is a temporary fixture convention requiring team confirmation.
The running Client prints state arrays, not these decoded results; no recognition
component currently connects the two. The production decoded-result interface
remains a decision for Nikita and Eduardo.

## Limitations and Future Work

- ESP32 firmware, physical sensors, and calibration are not implemented.
- Hardware transport selection, production payload/framing, processing placement,
  synchronization, freshness, debounce, and chord-completion policies remain open.
- BLE is deferred. The previous macOS experiment is preserved at commit
  `7dd12177081c26dcbe8b4203c8b9711db111a043` and is not active code.
- Chord recognition and keyboard output remain teammate integration work.
  Plover compatibility is a future goal outside the MVP.
- TCP is blocking and has no timeout, cancellation, automatic reconnection,
  concurrent-client handling, or application acknowledgment.
- UDP has no delivery/order guarantee, acknowledgments, retransmissions, peer
  authentication, or source filtering. Localhost results do not establish wireless
  performance or hardware compatibility.
- There is no committed automated test suite.

Agree on hardware and production interfaces, develop components against the
documented contracts, then validate the complete hardware-to-computer path.
