# Glover API

## Overview

Glover is a C++20 software foundation for the SDSU CS 576 two-glove
stenography project. Desktop simulations send mock finger states through:

```text
Secondary → Primary → Client
```

Secondary represents one physical glove. Primary combines its local hand with
Secondary's hand, and Client receives and validates ten-finger snapshots.
Local simulation, two-node TCP/HTTP, three-node TCP/UDP, mixed TCP/UDP-to-HTTP
relays, and direct individual HTTP submissions are implemented. In direct HTTP
mode both gloves send to Client, which pairs received snapshots by index.
Each simulation uses five mock samples; each glove has five fingers, and each
combined snapshot has ten values. Physical hardware integration remains future work.

[Build and Run](#build-and-run) · [Network Simulation](#network-simulation) ·
[Manual Verification](#manual-verification) ·
[HTTP Manual Verification](#http-manual-verification) ·
[Glove Models](#glove-models) · [State Serialization](#state-serialization) ·
[Chord Dictionary](#chord-dictionary) · [Team Integration](#team-integration)

## Requirements

- A C++20-capable compiler available as `c++`, Make, and a Unix-like shell.
- POSIX IPv4 sockets for networking.
- On macOS, installed and selected Apple developer tools providing `c++` and
  `xcrun`. The Makefile selects their SDK through `SDKROOT`.

The TCP/UDP workflow on macOS with Apple Clang and localhost networking is verified.
The HTTP API and all documented desktop simulation modes were manually verified
on macOS on November 13, 2026. Linux is unverified;
Windows and ESP32 support are not implemented.

## Build and Run

Clone the repository and build from its root:

```sh
git clone https://github.com/WestiferRobin/glover-api.git
cd glover-api
make
make run
```

`make` builds `build/glove` with C++20, `-Wall -Wextra -Wpedantic`, and `-pthread`.
HTTP uses vendored, header-only **cpp-httplib v0.66.0** under
`third_party/cpp-httplib/`, with its MIT license and version/hash record.
No package installation is required. HTTP JSON uses vendored nlohmann/json v3.12.0 (MIT); TLS and compression are not enabled.
`make run` builds when needed and runs the local mock.
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

## HTTP Manual Verification

The HTTP API foundation and two-node combined Primary HTTP simulation are
implemented, including TCP/UDP-to-HTTP relaying and direct individual HTTP
submission from both glove nodes. Final desktop verification passed on macOS. ESP32 communication
is not verified.

The application JSON adapter uses pinned, header-only nlohmann/json v3.12.0
(MIT), under `third_party/nlohmann/`; its README records provenance and hashes.
TCP/UDP text codecs are unchanged. Generic HTTP transport does not parse JSON.

Final verification on November 13, 2026 used a clean warning-free build and
independent Glover processes with bounded supervision. All seven network modes,
both physical-hand assignments, direct sender ordering/concurrency, and local
mock commands passed byte-for-byte comparisons against the five expected lines.
Health/state/role GETs, manual POST, curl headers, strict JSON/query validation,
duplicate/conflict rejection, missing samples, malformed responses, read timeout,
non-204 acknowledgments without retry, bind conflicts, final acknowledgments,
automatic completion, SIGINT/SIGTERM, terminal Ctrl-C and IPv4 port reuse passed.
No source corrections were needed in the final pass. Temporary check scripts
were outside the repository; there is no committed automated test suite.

Not exercised: Linux/Windows/ESP32, connection-establishment timeout to an
unreachable network, or an intentionally failing application handler producing
500. Controlled 500 responders did verify sender rejection. TCP still has no
receive deadline; UDP callbacks may extend deadline cleanup while HTTP completes.
An HTTP acknowledgment confirms in-memory acceptance, not durable delivery.

### API contract

| Request | Result |
|---|---|
| GET `/health` | 200, `{"status":"ok"}`, independent of glove availability |
| GET `/gloves` | 200 with latest complete `{index,left,right}`, or 204 if unavailable |
| GET `/gloves?role=primary` | 200 with latest `{role,hand,index,fingers}`, or 204 |
| GET `/gloves?role=secondary` | Same for Secondary |
| POST `/gloves` | 204 with empty body after acceptance |

JSON responses use `application/json`. GET never consumes samples. Latest means
highest accepted index for a role, and highest complete index for combined GET;
an incomplete newer pair never replaces the latest completed pair.

Individual payload (either role is allowed):

```json
{"role":"secondary","hand":"left","index":0,"fingers":[1,0,0,0,0]}
```

Combined payload (`hand` identifies Primary's physical hand):

```json
{"role":"primary","hand":"right","index":0,"left":[1,0,0,0,0],"right":[0,0,0,0,1]}
```

Every payload requires exactly the fields shown for its shape. Indices are
integers 0–4. Finger arrays contain exactly five integer 0/1 values; booleans,
floats, strings and nulls are rejected. Unknown/missing fields, duplicate JSON
keys, mixed shapes, invalid roles/hands and malformed JSON return 400.
Combined submissions require Primary role. POST requires `application/json`,
optionally `charset=utf-8` (quoted charset is also accepted); otherwise 415.

Client defaults to `--submission combined`. Use `--submission individual` for
separate role submissions. Wrong-mode submissions, conflicting role-to-hand
mappings and duplicate role/index or combined indices return 409 without mutation.
The first accepted submission locks the role mapping. Either hand can be Primary.
Individual states pair only at matching indices; out-of-order arrivals are allowed.

Role queries accept exactly one valid role parameter; unknown, empty, repeated
or malformed filters return 400. POST and health do not accept query parameters.
Unknown routes return 404, unsupported methods on known routes return 405.
Expected request errors leave the server available and accepted state unchanged.

### Two-terminal API inspection

Run from the repository root after `make`. All following commands are API-level
checks; they do not launch HTTP glove senders.

**Terminal 1 — Client:**

```sh
./build/glove client --transport http --bind 127.0.0.1 --port 9000 --submission combined --timeout-ms 30000 --stay-open true
```

Wait for `HTTP Client listening on 127.0.0.1:9000` on stderr.

**Terminal 2 — Inspect and submit:**

```sh
./build/glove http get --host 127.0.0.1 --port 9000 --path /health
./build/glove http get --host 127.0.0.1 --port 9000 --path /gloves
./build/glove http post --host 127.0.0.1 --port 9000 --path /gloves --content-type application/json --body '{"role":"primary","hand":"right","index":0,"left":[1,0,0,0,0],"right":[0,0,0,0,1]}'
./build/glove http get --host 127.0.0.1 --port 9000 --path /gloves
./build/glove http get --host 127.0.0.1 --port 9000 --path '/gloves?role=primary'
./build/glove http get --host 127.0.0.1 --port 9000 --path '/gloves?role=secondary'
```

Expected statuses: 200, 204, 204, 200, 200, 200. Health returns `{"status":"ok"}`;
204 responses are empty. State GETs report index 0, with Primary right and
Secondary left. JSON key ordering is not contractual.

For independent inspection use `curl -i http://127.0.0.1:9000/health`.
Negative compatibility checks only: `curl -i http://127.0.0.1:9000/test` and
`curl -i -X POST http://127.0.0.1:9000/test` must both return 404.

Generic `http get/post` print the exact body to stdout with no added newline,
and `HTTP <status>` to stderr. Complete responses, including 4xx/5xx, exit 0;
argument/transport failures exit 1. Inspect the status as well as `echo $?`.
The old demo-server command has been removed without an alias.

### Two-node HTTP glove simulation

Verified on macOS: both Primary-hand assignments, exact Client output, health
and role GETs, clean signal shutdown, automatic completion, and immediate IPv4
port reuse. Controlled responders verified rejection of 200/400/409/415/500
without retry; unavailable-server and unsupported-option checks also passed.

Client is the HTTP server; Primary uses its owned GlovePair and the five shared
mock fixtures to send sequential combined JSON submissions to `POST /gloves`.
Primary waits for HTTP 204 before continuing. Any other status (including 200)
or transport error stops the sender with exit 1, sample index and destination
context. There are no automatic retries. Successful Primary stdout/stderr are empty.

**Terminal 1 — Start a fresh Client session:**

```sh
./build/glove client --transport http --bind 127.0.0.1 --port 9000 --submission combined --timeout-ms 30000 --stay-open true
```

Wait for the readiness message. **Terminal 2:**

```sh
./build/glove http get --host 127.0.0.1 --port 9000 --path /health
./build/glove primary --transport http --hand right --peer 127.0.0.1 --port 9000 --submission combined
echo $?
./build/glove http get --host 127.0.0.1 --port 9000 --path /gloves
./build/glove http get --host 127.0.0.1 --port 9000 --path '/gloves?role=primary'
./build/glove http get --host 127.0.0.1 --port 9000 --path '/gloves?role=secondary'
```

Expect health 200, Primary exit 0, and all state GETs 200 at index 4. Primary
maps to right and Secondary to left. Client prints exactly:

```text
[1,0,0,0,0,0,0,0,0,1]
[0,1,0,0,0,0,0,0,1,0]
[0,0,1,0,0,0,0,1,0,0]
[1,0,1,0,0,0,0,0,1,0]
[1,1,1,1,1,1,1,1,1,1]
```

Stop Client with Ctrl-C; expect exit 0 after this complete session. Start a new
Client session and repeat with Primary `--hand left`. Output stays identical;
role GETs now map Primary to left and Secondary to right. Do not reuse the
completed session: duplicate indices are rejected. Primary defaults to right
and combined submission. Select individual submission explicitly for direct mode;
relays require combined submission. Receiver timeout options apply only when
Primary has UDP upstream.

Use `--stay-open false` on Client for automatic exit after the fifth sample.
This is desktop mock simulation; hardware is unverified.

### Three-node TCP/UDP-to-HTTP relay

Verified on macOS with independent processes: both upstream transports and both
hand assignments, exact five-state output, health/state/role GETs, clean shutdown
and IPv4 port reuse. Reordered UDP input retained correct indices. Negative checks
covered malformed/wrong-hand input, early/incomplete TCP EOF, a sixth TCP sample,
UDP duplicates/invalid indices/missing samples, downstream rejection/unavailability,
Client stopping during forwarding, and non-204 responses without retry. Existing
three-node TCP/UDP scenarios with both hands and two-node HTTP also passed.

Client is the only HTTP server. Primary receives the unchanged Secondary wire
format, combines it with its corresponding local fixture using its owned pair,
and forwards combined JSON to `POST /gloves`. Direct individual HTTP sending is
a separate mode described below. No new endpoint or transport framing is used.

Run each command in a separate terminal, in this order. Use a fresh Client
session for each transport and hand assignment.

**Terminal 1 — Client (both relay types):**

```sh
./build/glove client --transport http --bind 127.0.0.1 --port 9000 --submission combined --timeout-ms 30000 --stay-open true
```

Wait for `HTTP Client listening on 127.0.0.1:9000` on stderr.

**Terminal 2 — TCP-to-HTTP Primary:**

```sh
./build/glove primary --transport http --upstream-transport tcp --hand right --bind 127.0.0.1 --listen-port 9001 --peer 127.0.0.1 --port 9000 --submission combined
```

Primary waits silently. Its TCP receiver has no application readiness callback.
From Terminal 3, confirm its bound listener with
`lsof -nP -iTCP:9001 -sTCP:LISTEN`; check the process is `glove`, not another
service. Do not probe by connecting: TCP accepts only one connection.

**Terminal 3 — TCP Secondary:**

```sh
./build/glove secondary --transport tcp --hand left --peer 127.0.0.1 --port 9001
```

For UDP-to-HTTP, start a fresh Client, then use:

```sh
# Terminal 2 — UDP-to-HTTP Primary
./build/glove primary --transport http --upstream-transport udp --hand right --bind 127.0.0.1 --listen-port 9001 --peer 127.0.0.1 --port 9000 --submission combined --timeout-ms 30000
```

Confirm Primary's bound UDP socket from Terminal 3 with
`lsof -nP -iUDP:9001`, then run:

```sh
# Terminal 3 — UDP Secondary
./build/glove secondary --transport udp --hand left --peer 127.0.0.1 --port 9001
```

Repeat each scenario with Primary `--hand left` and Secondary `--hand right`.
All endpoint options remain unchanged. Client must print the same five snapshots
shown above, exactly once in index order. Primary and Secondary stdout/stderr
remain empty on success. Run `echo $?` immediately after each exits; expect 0.

After Secondary finishes, inspect from Terminal 3:

```sh
./build/glove http get --host 127.0.0.1 --port 9000 --path /health
./build/glove http get --host 127.0.0.1 --port 9000 --path /gloves
./build/glove http get --host 127.0.0.1 --port 9000 --path '/gloves?role=primary'
./build/glove http get --host 127.0.0.1 --port 9000 --path '/gloves?role=secondary'
```

All return HTTP 200 and exit 0; state responses identify index 4. Role responses
identify the configured physical hands. Stop the completed Client with Ctrl-C;
expect exit 0. Check Glover no longer owns the listeners before starting again.
An unrelated service can occupy a port after Glover stops; select isolated ports
when verifying connection refusal.

`--transport` selects outbound delivery. Explicit `--upstream-transport tcp|udp`
is supported only for HTTP Primary with both `--bind` and `--listen-port`.
HTTP relay requires it; omitting all inbound options retains two-node HTTP.
Existing TCP/UDP commands keep their original implicit same-transport behavior.
Explicit TCP-to-UDP/UDP-to-TCP routing and HTTP upstream are not supported.

TCP assigns HTTP indices by accepted sample order and still requires exactly
five valid frames followed by clean EOF. It rejects malformed/wrong-hand states,
early EOF, incomplete frames, and extra samples. It can wait indefinitely for
upstream EOF: Client's HTTP deadline does not cancel TCP reception. Client may
already have accepted all five states before Primary rejects a sixth frame;
check every process's exit status.

UDP preserves the original U1 index, pairs against that indexed local fixture,
and rejects duplicates. Its total receive deadline includes time spent forwarding
HTTP requests; callbacks cannot be interrupted by that deadline. A slow HTTP
request can therefore delay timeout reporting. Missing-index diagnostics refer
to samples not successfully forwarded. Client buffers indexed states, so arrival
order does not change output. UDP transport still provides no delivery guarantee.

HTTP forwarding requires 204; any other status or transport failure stops the
relay without retry. Errors identify reception/relay context and HTTP sample and
destination when forwarding failed. Secondary send success is not an end-to-end
acknowledgment. Partial delivery does not make Primary successful.

### Direct Primary and Secondary HTTP

Focused macOS verification passed for both hand assignments with Primary-first,
Secondary-first and concurrent senders. Exact output and GET inspection, both
shutdown modes, incomplete-session timeout, duplicate/conflict rejection and port
reuse were checked. Both senders rejected non-204, malformed and stalled responses
without retries. Combined HTTP and both mixed relays also passed focused checks.

Both nodes independently POST individual states to the same Client. Primary
updates only its assigned hand in its existing pair; Secondary uses its existing
glove. Client stores received snapshots and pairs matching indices without owning
another GlovePair. No TCP/UDP relay is involved.

**Terminal 1 — Fresh individual-mode Client:**

```sh
./build/glove client --transport http --bind 127.0.0.1 --port 9000 --submission individual --timeout-ms 30000 --stay-open true
```

Wait for actual Client readiness. **Terminal 2 — Primary:**

```sh
./build/glove primary --transport http --hand right --peer 127.0.0.1 --port 9000 --submission individual
echo $?
```

Primary sends five payloads such as:

```json
{"role":"primary","hand":"right","index":0,"fingers":[0,0,0,0,1]}
```

Expect exit 0, empty stdout/stderr. Client has no complete batch yet and prints
nothing. GET `/gloves` returns 204 while Primary role GET returns index 4.

**Terminal 3 — Secondary:**

```sh
./build/glove secondary --transport http --hand left --peer 127.0.0.1 --port 9000
echo $?
```

Secondary always sends individual states; it does not accept `--submission`.
Expect exit 0 with empty stdout/stderr. Client now prints the same five canonical
snapshots shown in the two-node section, exactly once in index order.

Inspect from either sender terminal:

```sh
./build/glove http get --host 127.0.0.1 --port 9000 --path /health
./build/glove http get --host 127.0.0.1 --port 9000 --path /gloves
./build/glove http get --host 127.0.0.1 --port 9000 --path '/gloves?role=primary'
./build/glove http get --host 127.0.0.1 --port 9000 --path '/gloves?role=secondary'
```

All return 200, state responses identify index 4, Primary is right and Secondary
left. Stop Client with Ctrl-C; completed session exits 0. Start a fresh Client and
repeat with Primary `--hand left` and Secondary `--hand right`; physical output
is identical and role GETs reflect the reversed assignment.

Either sender may finish first, or both may run concurrently. An individual POST
returns 204 on acceptance without waiting for the other glove. Sender success
alone does not prove that the other role arrived. A missing role causes Client's
finite session deadline to fail with exit 1 and no partial batch output.

Use a fresh session for each run. Duplicates and role/hand conflicts return 409
without overwriting valid state. Combined requests in individual mode and
individual requests in combined mode are rejected. Both senders require 204;
other statuses, refused connections, timeouts and malformed responses cause
exit 1 with sample/destination diagnostics and no retry.

`--stay-open true` permits GET inspection after completion. `--stay-open false`
automatically drains the final acknowledgment and exits after all five pairs.
No additional endpoints, fixtures, dependencies or hardware support are involved.

### Session completion and shutdown

Index 0 alone does not complete the simulation. Submit all five existing fixture
states at indices 0–4 to complete it. Client buffers received states and prints
one complete batch, exactly once in index order, using the canonical serializer.
It does not compare submitted values against fixtures or invent missing values.
No incomplete batch is printed. In individual mode both roles must arrive for
all five indices; one role's acknowledgment does not confirm a complete pair.

The finite receive deadline defaults to 30000 ms from HTTP execution startup;
GETs and rejected requests do not reset it. Missing samples or stopping an
incomplete session cause exit 1 with missing-index diagnostics. Increase
`--timeout-ms` for leisurely interactive inspection. Restart for a new batch.

`--stay-open true` is the default: after completion the deadline is disabled,
GET inspection remains available, and Ctrl-C or SIGTERM stops the process with
exit 0. `--stay-open false` closes automatically after the complete batch,
draining active responses and joining workers. Bind/listener failures exit 1.
No non-signal-safe work runs in a signal handler. Confirm port release with
`lsof -nP -iTCP:9000 -sTCP:LISTEN` (no output, exit 1 when no listener exists).

Limits remain plain HTTP, numeric IPv4 server binding, 1 MiB request body,
5-second server I/O timeouts, multipart rejection, and the library's 8 KiB form
limit. The generic client uses 5-second connection/read/write timeouts and a
15-second request budget; DNS timing is platform-dependent. No automatic retries
or redirects occur. Lost acknowledgments may follow accepted data. There is no
authentication, persistent history, live sensor synchronization or production
session protocol. Desktop verification does not establish wireless or hardware performance.

## Architecture

| Module | Responsibility | Entry points |
|---|---|---|
| `glove/` | Glove models and finger states | `include/glove/glove.hpp`, `include/glove/pair.hpp` |
| `nodes/` | Primary, Secondary, Client execution | `include/nodes/primary.hpp`, `secondary.hpp`, `client.hpp`, `node.hpp` |
| `transport/` | TCP, UDP, HTTP, sockets, framing, serialization | `include/transport/tcp/`, `include/transport/udp/`, `include/transport/http/`, `include/transport/detail/state_codec.hpp` |
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
`hand, bindAddress, listenPort, peer, port, transport, timeout, inbound`.
The optional inbound selection defaults to absent for existing call sites; HTTP
relay callers provide TCP or UDP explicitly.
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
