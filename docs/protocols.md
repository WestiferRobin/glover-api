# Glover protocols

This is the canonical TCP/UDP serialization and transport reference. It also
defines the boundary between generic HTTP transport and the application codec.
[Architecture](architecture.md) covers models, ownership, and supported flows.
Terminal procedures and expected results are in [testing](testing.md).

## Representation layers

Internal state uses fixed Boolean arrays. Text codecs convert these arrays to
payloads; TCP adds LF framing and UDP adds an indexed envelope. HTTP carries
JSON application payloads in request/response bodies. Raw C++ object memory is
not a wire format. Transport selection does not determine physical hand or
communication role.

All representations preserve left/right storage order described in
[architecture](architecture.md#finger-ordering). Combined arrays are left then
right; serialization does not reverse either hand.

## Shared text codec

Declarations are in [state_codec.hpp](../include/transport/detail/state_codec.hpp),
with implementation in [state_codec.cpp](../src/transport/state_codec.cpp).
The header is under `transport/detail/`, not directly under `transport/`.

| Function/type | Purpose |
|---|---|
| `HandState` | Physical `Hand` and five-value `state` array |
| `serializeHandState()` | Produces a five-finger hand payload |
| `parseHandState()` | Validates and returns a `HandState` |
| `serializeState()` | Produces a combined ten-value payload |
| `parseState()` | Validates and returns a ten-value array |

Hand payloads have exactly ten ASCII characters:

```text
S5 L 10100
S5 R 10100
```

`S5` identifies the five-state format; single spaces separate it from `L` or
`R` and the five binary digits. The hand marker identifies physical identity,
not role. The same digit string therefore refers to different anatomical slots
for Left and Right.

Combined payloads have exactly 21 ASCII characters:

```text
[1,0,1,0,0,0,0,0,1,0]
```

Ten binary digits are comma-separated inside brackets, with no spaces. Text
parsers reject wrong length, invalid hand markers, nonbinary digits, incorrect
separators, extra whitespace, and trailing content with `std::invalid_argument`.
Hand serialization also rejects invalid physical-hand enum values. The codecs
do not append or strip LF. They contain no sample index or timestamp.

## Standalone text codec example

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

Use the [macOS SDK setup in the model example](architecture.md#standalone-model-example).
Add the model source files too if your component constructs or calls `Glove`
or `GlovePair`.

## TCP transport and framing

`TcpClient` connects to a numeric IPv4 endpoint. `TcpServer::run(onMessage)`
binds, listens, accepts one connection, and receives until clean EOF. Start
receivers before senders. Node names are independent of socket roles: Glover
Client is a TCP server, and relay Primary is both a server and a sender.

TCP is an ordered byte stream. One read can contain part of a frame or several
frames. `tcp_detail::LineFramer` accumulates bytes and delivers a callback for
each LF-delimited frame, excluding LF. It preserves CR and other non-LF bytes
and delivers empty frames; node codecs reject these when they are invalid state
payloads. A CRLF-terminated state retains CR and therefore fails codec parsing.

The maximum payload is **4096 bytes excluding LF**, from
`TcpServer::MAX_MESSAGE_BYTES`. Exceeding it throws `std::length_error`. EOF
with pending non-LF-terminated bytes throws; EOF at a frame boundary succeeds,
including an empty connection. No UTF-8 validation occurs in the transport.

`TcpClient::send()` sends bytes unchanged; node callers append LF. It loops over
partial writes and retries interrupted sends. Other send failures disconnect
and throw. A failed send may already have delivered a prefix, so it is not safe
to infer that no data arrived. There is no automatic replay or reconnect.
`disconnect()` is repeatable; destruction also closes the connection.

Server callbacks run synchronously, and their payload references last only for
the invocation. Socket, framing, and callback exceptions propagate; owned
sockets close on exit. Earlier delivered frames are not rolled back. Each run
uses fresh sockets; `SO_REUSEADDR` supports restart, not simultaneous ownership
of a listening port. Transport classes do not print application output.

TCP execution has no receive/session timeout, cancellation facility, application
acknowledgment, or concurrent-client handling. Transport-level reliability does
not prove application validation or five-sample completion.

### Two-node TCP

Primary loads both fixture hands, sends five combined frames, and disconnects.
Client parses and prints each frame immediately, then returns at clean EOF.
**TCP Client does not independently require exactly five samples.** Clean EOF
after zero or fewer than five valid frames can succeed; verify the expected
output separately. A later error can leave earlier snapshots already printed.

### Three-node TCP and TCP-to-HTTP

Secondary sends five hand frames and closes. Primary rejects a remote hand
matching its local hand, pairs each accepted frame with the local fixture at
that arrival position, updates its existing pair, and forwards combined state.
There is no sample index on the TCP wire.

Primary requires exactly five accepted hand frames and clean upstream EOF.
Early EOF, a sixth frame, malformed data, wrong hand, or an incomplete final
frame fails. Receiving five frames without EOF leaves the relay waiting.
For TCP output, Primary closes its downstream connection after success or
failure; downstream Client cannot infer relay success from EOF alone.

For HTTP output, accepted TCP positions become JSON indices 0–4. Primary checks
each HTTP acknowledgment before accepting the next sample. HTTP Client can
complete after five submissions while Primary still waits for upstream EOF;
a later sixth frame can fail Primary after downstream completion. There is no
distributed rollback or all-process completion acknowledgment.

`TcpClient` is neither copyable nor movable. Sending while disconnected or
connecting twice fails; a failed connect closes its temporary socket and permits
an explicit fresh attempt. SIGPIPE is suppressed with `SO_NOSIGPIPE` on macOS or
`MSG_NOSIGNAL` where supported; platforms with neither fail compilation.

## UDP envelope and transport

Node datagrams use this exact envelope, with no appended LF or CR:

```text
U1 <index> <payload>
```

```text
U1 0 S5 L 10000
U1 0 [1,0,0,0,0,0,0,0,0,1]
```

`U1` is the version marker. The index is one digit from 0 to 4, separated by
single spaces from the marker and unchanged text payload. Index 0 identifies
the first sample. `serializeUdpSample()` checks the index and nonempty payload;
`parseUdpSample()` validates the envelope and returns `UdpSample`. The node then
validates its payload with the appropriate text parser. Unknown versions,
signed/multidigit/out-of-range indices, and invalid payloads fail. The returned
payload is a `std::string_view` into the original datagram and must not outlive it.

`UdpSender` and `UdpReceiver` require numeric IPv4 and ports 1–65535. Sender
construction creates an unconnected socket. `send()` sends exactly one datagram
without adding delimiters. UDP has no connection handshake or EOF.

The maximum is **4096 bytes including the envelope**, defined by both transport
classes. Oversized sends and truncated receives throw `std::length_error`;
truncated prefixes are not delivered. Generic transport preserves empty/binary
payloads, but empty datagrams are not valid node envelopes. Successful send
means local acceptance, not receipt or processing by another node.

Each receiver run binds a fresh socket and closes it on success or failure.
One datagram produces one synchronous callback. Callback exceptions propagate;
earlier callbacks are not undone. The transport counts arrivals, while nodes
enforce unique sample indices. Queued extras are discarded when reception ends.
Sender sockets remain owned until destruction; interrupted sends retry only
when interruption indicates no data was transmitted. Short sends and other
errors are not automatically replayed.

Sender and receiver objects are noncopyable and nonmovable; do not share an
instance among concurrent callers. System errors retain their codes through
`std::system_error`; invalid configuration uses `std::invalid_argument`, and
timeout/short-send errors use `std::runtime_error`. Transport emits no diagnostics.
A 4096-byte datagram may require IP fragmentation on a real network.
Standalone callers compile `src/transport/udp/sender.cpp` and `receiver.cpp`
with the C++20/include/SDK settings above; no external library is needed for UDP.

### Ordering and pairing

UDP does not guarantee arrival, uniqueness, or order. Primary validates the
remote hand, rejects duplicate indices, and combines with the local fixture
selected by index rather than arrival position. It forwards that same index
over UDP or HTTP.

UDP Client rejects duplicate indices and buffers valid combined states. Only
after successful reception of all five does it print, ordered 0–4. Duplicate
rejection is fatal to the node session, not silent deduplication. Malformed or
missing samples never produce a partial Client batch.

### Deadline and completion

`UdpReceiver::run(count, timeout, callback)` requires positive count, nonempty
callback, and timeout in 1..INT_MAX milliseconds. Nodes request five arrivals
and validate five unique indices. A monotonic total deadline starts at run
entry; receiving packets does not reset it. Checks occur around waits and after
callbacks. A callback cannot be preempted, so slow forwarding may exceed the
budget before a timeout is reported, even after its side effects occurred.

Timeout errors report received/expected counts. Nodes add missing-index context
using `missingUdpSamples()`. Completion closes the receiver socket; there is no
EOF message or final acknowledgment. Failures can occur after Primary already
forwarded earlier states.

| Configuration surface | Default receive budget |
|---|---|
| Pure UDP Primary/Client CLI | 5000 ms |
| UDP-to-HTTP Primary CLI | 30000 ms |
| C++ Primary relay constructor's `timeout` parameter | `UDP_SESSION_TIMEOUT` (5000 ms), including HTTP outbound if omitted |
| C++ TCP/UDP Client constructor's `timeout` parameter | `UDP_SESSION_TIMEOUT` (5000 ms); used for UDP |

The CLI selects `HTTP_SESSION_TIMEOUT` when HTTP transport is chosen, then
passes that value to the UDP-to-HTTP relay. Constructor defaults and CLI defaults
are therefore not interchangeable. Detailed flag compatibility is in the
[CLI reference](cli.md#options-and-defaults).

## Generic HTTP transport boundary

`HttpClient` provides `get(path)` and `post(path, body, contentType)`. Host is a
hostname or IPv4 address without a scheme or port. Paths start with `/` and may
include an already percent-encoded query. Each request uses its own connection.
Bodies are opaque bytes; the transport does not parse glove JSON.

`HttpResponse` contains status and body. Complete HTTP error responses are
returned normally; invalid arguments and transport failures throw. Application
senders decide which statuses count as success. Current glove senders require
204; generic CLI requests print any complete response, including error statuses.

The client uses 5-second connection/read/write timeouts and a 15-second request
budget; DNS resolution can follow platform resolver timing. It does not retry,
follow redirects, or decompress responses. HTTP is plain, without TLS.

`HttpServer` construction validates numeric IPv4 and a nonzero port but does
not bind. `get()` and `post()` register exact decoded paths before execution;
queries and patterns are not registration paths. Duplicate method/path pairs
and empty handlers are rejected.

Handlers receive `HttpRequest` (method, decoded path, body, content type, and
original target including query) and return `HttpServerResponse` (status, body,
content type). Callbacks may execute concurrently; captured application state
belongs to the caller and requires appropriate synchronization. Unknown routes
return 404, unsupported methods 405 with `Allow`, and handler exceptions a
generic 500 response. Multipart requests are rejected with 415.

Server limits include a 1 MiB request body and 5-second read/write timeouts;
the vendored library also imposes its 8 KiB form limit. `run()` blocks until
stopped and workers finish. It is single-use, including failed runs.
`isRunning()` reports accept-loop readiness. `stop()` is thread-safe and
repeatable, can cancel a not-yet-started run, and is not signal-safe. It does not
interrupt application callbacks. Stop and join a caller-owned run thread before
destroying the server; destruction alone is not thread coordination.

### Application placement and session semantics

Glover's **Client node** registers `GET /health`, `GET /gloves`, and
`POST /gloves`. These are application operations, not generic transport
primitives. Primary and Secondary use generic HTTP clients; they do not add
glove logic to the transport classes.

HTTP uses indexed submissions and in-memory acceptance acknowledgments. GET
provides latest-state inspection, not a queue. The [HTTP API reference](api.md)
is authoritative for schemas, validation, pairing, concurrency, session completion,
and shutdown. [CLI](cli.md) defines configuration; [testing](testing.md) provides
reproducible workflows. A 204 does not imply durable storage or recognition.

## JSON codec boundary

`glove_json` in [glove_json.hpp](../include/codec/glove_json.hpp) and
[glove_json.cpp](../src/codec/glove_json.cpp) uses the vendored JSON parser and
performs no network I/O. `parse()` validates incoming application submissions,
including field sets, role/hand values, integer indices, five-element integer
binary arrays, duplicate keys, and excessive nesting.

Primary serializes through `combinedSubmission()` or `individual()`; Secondary
uses `individual()`. Client parses POSTs and uses `combined()` or `individual()`
for inspection responses. Combined inspection and combined submission are
distinct representations; the latter includes sender role/hand identity.
`isJsonContentType()` and `roleFilter()` validate application media-type/query
rules. The node, not the codec, enforces session mode, duplicates, mapping,
completion, and storage. Existing TCP/UDP text formats remain unchanged.

## Transport comparison

| Property | TCP | UDP | HTTP |
|---|---|---|---|
| Communication model | Connected ordered byte stream | Unconnected datagrams | Request/response |
| Framing | LF, added by caller | Datagram boundary plus node envelope | HTTP framing; JSON body |
| Sample identity | Accepted position in relay | Explicit envelope index | Explicit JSON index |
| Ordering | Byte order preserved per connection | Arrival may reorder; Client sorts by index | Requests may arrive concurrently; Client pairs/sorts by index |
| Completion | Client: clean EOF; relay: five plus EOF | Five unique indices within deadline | Five complete indexed states; server lifetime configured separately |
| Acknowledgment | No application acknowledgment | None | 204 for in-memory submission acceptance |
| Errors | Exceptions; partial output possible | Exceptions/deadline; no partial Client batch | HTTP errors or transport exceptions; no partial Client batch |
| Glover usage | Combined delivery and hand relay | Indexed hand/combined delivery | Combined delivery, TCP/UDP relay output, direct individual submissions, inspection |

## Simulation-scope limitations

There is no production device discovery, authentication, automatic reconnection,
application TCP acknowledgment, or UDP retransmission/source filtering. Fixed
five-sample UDP/HTTP indices are not a continuous hardware-streaming contract.
There are no session identifiers, persistent storage, or cross-process rollback.
These boundaries describe the current simulation scope; they do not specify
future hardware behavior.

## Source references and related documentation

- TCP: [client](../src/transport/tcp/client.cpp), [server](../src/transport/tcp/server.cpp), [framer](../include/transport/tcp/detail/framing.hpp).
- UDP: [sender](../src/transport/udp/sender.cpp), [receiver](../src/transport/udp/receiver.cpp), [envelope header](../include/transport/udp/detail/udp_envelope.hpp), [envelope implementation](../src/transport/udp/udp_envelope.cpp).
- HTTP: [client header](../include/transport/http/client.hpp), [server header](../include/transport/http/server.hpp), [client implementation](../src/transport/http/client.cpp), [server implementation](../src/transport/http/server.cpp).
- Node semantics: [Primary](../src/nodes/primary.cpp), [Secondary](../src/nodes/secondary.cpp), [Client](../src/nodes/client.cpp).
- Defaults: [node configuration](../include/nodes/network.hpp), [Primary declarations](../include/nodes/primary.hpp), [Client declarations](../include/nodes/client.hpp), [CLI parser](../src/cli/parser.cpp).
- Dependency provenance: [cpp-httplib](../third_party/cpp-httplib/README.md), [nlohmann/json](../third_party/nlohmann/README.md).

Related references: [API](api.md), [CLI](cli.md), [testing](testing.md), and
[integration](integration.md). This document owns text wire formats, framing,
and transport semantics; the API reference owns HTTP application contracts.
