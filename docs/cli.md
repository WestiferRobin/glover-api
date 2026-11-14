# Glover command-line reference

Run commands from the repository root using `./build/glove`. The CLI selects
local simulation, a Primary/Secondary/Client node, or a generic HTTP request.
`cli::parse()` validates options and constructs configuration/nodes without
running them; `main.cpp` executes the selected action and reports errors.

See [architecture](architecture.md) for ownership, [protocols](protocols.md)
for TCP/UDP wire behavior, and [HTTP API](api.md) for routes and JSON contracts.

## Build and help

The existing Makefile uses C++20 and pthread support, supplies vendored include
paths, and selects the Apple SDK through `xcrun` on macOS:

```bash
make clean
make
./build/glove --help
./build/glove -h
```

`make` defaults to `all`; `make run` builds and runs the default simulation.
`make clean` removes the executable. These are existing build commands, not new
targets. See [README prerequisites](../README.md#requirements) for platform
requirements. Help aliases are top-level commands only: `primary --help` and
`http --help` are not supported help forms.

## Command inventory

| Command after `./build/glove` | Bare behavior |
|---|---|
| No arguments | Runs the five-state mock simulation |
| `mock` | Same simulation; no options accepted |
| `primary` | Local five-state simulation using Primary's pair |
| `secondary` | Prints `Secondary node started.` and exits; no network |
| `client` | Prints `Client node started.` and exits; no network |
| `http get` | Requires generic HTTP options |
| `http post` | Requires generic HTTP options including body |
| `--help` or `-h` | Prints help and exits 0 |

Names and enum values are case-sensitive. No command aliases beyond the two
help forms are defined. Bare `http` fails because a request action is required.

## Options and defaults

All options use separate flag/value tokens, not `--flag=value`. Options may be
ordered freely but may not repeat. Values and flags are case-sensitive unless
the HTTP media-type contract explicitly says otherwise.

| Flag | Values / validation | Applicable commands and requirement | CLI default / meaning |
|---|---|---|---|
| `--transport` | `tcp`, `udp`, `http` | Required for any network node invocation | No implicit network mode; selects node transport (Primary outbound) |
| `--hand` | `left`, `right` | Network Primary optional; network Secondary required | Primary: `right`; Secondary: no network default |
| `--peer` | Numeric IPv4 | Required for network Primary/Secondary | No default; destination address |
| `--host` | Hostname or IPv4, without scheme or port | Required for generic HTTP | No default; HTTP destination |
| `--bind` | Numeric IPv4 | Required for network Client and relay Primary | No default; local receiving address |
| `--port` | Decimal integer 1–65535 | Required for all network nodes and generic HTTP | No default; Client listening port, otherwise destination port |
| `--listen-port` | Decimal integer 1–65535 | Required with Primary `--bind` | No default; relay upstream listener port |
| `--upstream-transport` | `tcp`, `udp` | Required for HTTP Primary relay; invalid elsewhere | Absent for direct sending; TCP/UDP relays implicitly match outbound transport |
| `--submission` | `combined`, `individual` | Optional for HTTP Primary/Client only | `combined`; HTTP relay must remain combined |
| `--timeout-ms` | Decimal integer 1..INT_MAX (2147483647 on the current target) | Optional for UDP Primary/Client, HTTP Client, HTTP Primary with UDP upstream | Pure UDP: 5000; HTTP Client and UDP-to-HTTP relay: 30000; total receive budget |
| `--stay-open` | `true`, `false` | Optional for HTTP Client only | `true`; retain completed session for inspection |
| `--path` | Absolute request path starting `/`, optionally encoded query | Required for generic HTTP | No default; no spaces, fragments, controls, or leading `//` |
| `--body` | One shell argument, including empty or option-like text | Required for generic HTTP POST only | No default; request bytes available through argv |
| `--content-type` | Nonempty, no control characters | Optional for generic HTTP POST only | `text/plain`; explicitly use `application/json` for glove submissions |

These are CLI defaults, not C++ constructor defaults. In particular, the CLI
selects a 30000 ms budget for UDP-to-HTTP Primary even though the C++ relay
constructor's omitted timeout is 5000 ms. TCP has no configurable session
timeout. HTTP request I/O timeouts are separate fixed transport settings;
`--timeout-ms` does not change them.

Node addresses, including HTTP node peers, require numeric IPv4. Generic HTTP
`--host` permits DNS hostnames but not a URL, embedded port, credentials, or
IPv6 literal. `--bind` is local; `--peer` is remote; a relay's `--listen-port`
and `--port` identify different hops. Invalid addresses/ports fail rather than
falling back. Shell arguments cannot contain embedded NUL bytes.

## Primary

Bare `primary` runs locally. Supplying any option enters network configuration;
even `primary --hand left` alone fails. Every network mode requires
`--transport`, `--peer`, and `--port`; `--hand` is optional and defaults right.

| Mode | Additional configuration | Behavior and completion |
|---|---|---|
| TCP sender | None | Loads both fixture hands, sends five combined states, closes |
| TCP relay | `--bind`, `--listen-port` | Receives hand frames, pairs by arrival position, forwards; requires five plus clean upstream EOF |
| UDP relay | `--bind`, `--listen-port`; optional timeout | Receives indexed hands and forwards combined datagrams; five unique indices within deadline |
| HTTP combined sender | Optional `--submission combined` | Sends five combined fixture states; requires 204 for each |
| HTTP individual sender | `--submission individual` | Sends only Primary's hand; requires individual-mode Client and 204 for each |
| TCP-to-HTTP relay | `--upstream-transport tcp`, `--bind`, `--listen-port` | Receives TCP hands and sends combined HTTP; five plus EOF upstream |
| UDP-to-HTTP relay | `--upstream-transport udp`, `--bind`, `--listen-port`; optional timeout | Indexed UDP reception and combined HTTP forwarding |

Primary relay bind and listen-port options must appear together. UDP has no
two-node Primary mode. HTTP relay requires explicit upstream selection and
combined submission. `--stay-open` is not a Primary option. Successful network
Primary is silent on stdout/stderr; local Primary prints five snapshots.
Failures report diagnostics and exit 1; earlier forwarded states are not undone.

## Secondary

Network Secondary requires transport, peer, port, and an explicit hand. TCP/UDP
send that hand to Primary; HTTP sends individual JSON directly to Client.
Secondary selects the same physical hand from every shared fixture. It sends
five samples and exits 0 on successful execution, with empty stdout/stderr.
TCP closes after sending; UDP send success is not remote acknowledgment; HTTP
requires a 204 for each request. Sender success does not prove Client completion.

Secondary accepts neither `--submission` nor `--timeout-ms`, regardless of
transport. It does not accept listener, upstream, or stay-open options.
Bare Secondary remains a startup smoke test.

## Client

Network Client requires transport, bind, and port. It accepts no hand, peer,
listen-port, or upstream option.

| Transport | Optional configuration | Behavior |
|---|---|---|
| TCP | None | Parses/prints combined frames immediately; clean EOF succeeds without enforcing a count of five |
| UDP | Timeout | Buffers five unique indexed states and prints only a successful complete batch in index order |
| HTTP | Submission, timeout, stay-open | Hosts API; combines indexed individual submissions or accepts combined states; prints complete batch once |

HTTP Client defaults to combined submission, 30000 ms, and stay-open true.
Individual mode requires both roles at every index. With stay-open true, a
complete session remains available for inspection until Ctrl-C/SIGTERM; that
completed shutdown exits 0. With false, it stops automatically after completion.
Incomplete timeout or HTTP shutdown exits 1 without partial batch output.
See [API lifecycle](api.md#session-lifecycle-and-concurrency) for details.

`--submission` and `--stay-open` are invalid for TCP/UDP. TCP Client rejects
`--timeout-ms`. Bare Client only prints its startup message.

## Configuration compatibility

“Submission” below describes application data; the `--submission` flag itself
is accepted only for HTTP Primary/Client.

| Node | Transport | Upstream | Submission / operation |
|---|---|---|---|
| Primary | TCP | None | Combined sender |
| Primary | TCP | TCP (implicit) | Combined relay |
| Primary | UDP | UDP (implicit) | Combined relay |
| Primary | HTTP | None | Combined sender |
| Primary | HTTP | None | Individual sender |
| Primary | HTTP | TCP (explicit) | Combined relay |
| Primary | HTTP | UDP (explicit) | Combined relay |
| Secondary | TCP | None | Individual hand to Primary |
| Secondary | UDP | None | Individual hand to Primary |
| Secondary | HTTP | None | Individual submission to Client |
| Client | TCP | None | Combined receiver |
| Client | UDP | None | Combined receiver |
| Client | HTTP | None | Combined or individual server |

Local simulation is separate: no arguments, `mock`, or bare `primary`. There is
no HTTP upstream relay or explicit TCP-to-UDP/UDP-to-TCP relay configuration.
Primary and Secondary must represent opposite hands. Hand conflicts are runtime
application validation failures, not something the CLI can detect across processes.

## Generic HTTP requests

```bash
./build/glove http get --host 127.0.0.1 --port 9000 --path /health
./build/glove http post --host 127.0.0.1 --port 9000 --path /gloves --content-type application/json --body '{"role":"primary","hand":"right","index":0,"left":[1,0,0,0,0],"right":[0,0,0,0,1]}'
```

These require an already listening server. The POST additionally requires a
combined-mode session with unused index 0 and compatible mapping. Full schema
and acceptance behavior belong in [api.md](api.md).

Quote query paths and JSON so the shell passes one argument. `--body ''` and
`--body '--example'` are accepted as body values; that does not make them valid
glove JSON. GET rejects body/content-type options. The generic tool does not
automatically JSON-encode bodies or choose their media type.

On a complete HTTP response, stderr receives `HTTP <status>` plus newline and
stdout receives body bytes unchanged, without an added newline. **The process
exits 0 for complete 4xx/5xx responses too.** Inspect HTTP status separately.
Argument, connection, timeout, and other request failures exit 1. Node senders
have a different policy: any status other than 204 fails the node.

## Argument validation and exits

With no command, execution defaults to mock; with bare `http`, it fails.
Options require values and exact supported names. Duplicate flags, unexpected
positional arguments, invalid enums, missing network flags, and invalid
port/timeout numbers fail. Decimal values must parse completely: suffixes,
leading `+`, whitespace, overflow, zero, and negatives are not valid ports or
timeouts. `--stay-open` accepts only literal `true` or `false`.

Except for generic `--body`, a value beginning with `--` is treated as missing.
An unknown standalone command prints its name and help to stderr with exit 1;
invalid options/arguments report `Error: ...` and exit 1. Supplying arguments
to mock or top-level help is rejected. There is no implicit port/address or
automatic recovery from unsupported combinations.

| Execution result | stdout | stderr | Exit |
|---|---|---|---|
| Local mock/Primary success | Five snapshots | Empty | 0 |
| Bare Secondary/Client success | Startup message and newline | Empty | 0 |
| Network sender success | Empty | Empty | 0 |
| TCP receiver success | Valid snapshots as received | Empty | 0 at clean EOF |
| UDP receiver success | Five ordered snapshots | Empty | 0 |
| HTTP Client success | Complete five-state batch once | Listening message | 0 after completed shutdown |
| Invalid arguments, connection/bind failure, timeout | May contain earlier output where applicable | Error diagnostic | 1 |
| Node HTTP sender receives non-204 | Empty | Sample/destination error | 1 |
| Generic HTTP complete response | Raw body, possibly empty | HTTP status | 0, including 4xx/5xx |

The graceful signal semantics above belong to HTTP Client. TCP/UDP do not have
the same custom shutdown path; do not infer an exit-0 contract for interrupting
them. TCP may already have printed partial output before a later failure.

## Concise workflow examples

Each block is a separate run using fresh processes. Execute each numbered
terminal line in its own terminal, in order; wait for receivers to bind before
starting senders. Do not paste a whole block into one terminal: listeners block.
Port 9000 belongs to Client, 9001 to relay Primary. HTTP readiness is announced
on stderr; TCP/UDP do not print a readiness message. The explicit 30000 ms UDP
budgets below allow time to start terminals and are not the pure UDP defaults.

These are command reference examples. Detailed procedures, readiness checks,
expected output, and negative cases are in [testing](testing.md).

### Local

```bash
./build/glove mock
```

### Two-node TCP

```bash
# Terminal 1
./build/glove client --transport tcp --bind 127.0.0.1 --port 9000
# Terminal 2
./build/glove primary --transport tcp --peer 127.0.0.1 --port 9000
```

### Three-node TCP

```bash
# Terminal 1
./build/glove client --transport tcp --bind 127.0.0.1 --port 9000
# Terminal 2
./build/glove primary --transport tcp --bind 127.0.0.1 --listen-port 9001 --peer 127.0.0.1 --port 9000
# Terminal 3
./build/glove secondary --transport tcp --hand left --peer 127.0.0.1 --port 9001
```

### Three-node UDP

```bash
# Terminal 1
./build/glove client --transport udp --bind 127.0.0.1 --port 9000 --timeout-ms 30000
# Terminal 2
./build/glove primary --transport udp --bind 127.0.0.1 --listen-port 9001 --peer 127.0.0.1 --port 9000 --timeout-ms 30000
# Terminal 3
./build/glove secondary --transport udp --hand left --peer 127.0.0.1 --port 9001
```

### Two-node HTTP

```bash
# Terminal 1
./build/glove client --transport http --bind 127.0.0.1 --port 9000
# Terminal 2
./build/glove primary --transport http --peer 127.0.0.1 --port 9000
```

### TCP-to-HTTP relay

```bash
# Terminal 1
./build/glove client --transport http --bind 127.0.0.1 --port 9000
# Terminal 2
./build/glove primary --transport http --upstream-transport tcp --bind 127.0.0.1 --listen-port 9001 --peer 127.0.0.1 --port 9000
# Terminal 3
./build/glove secondary --transport tcp --hand left --peer 127.0.0.1 --port 9001
```

### UDP-to-HTTP relay

```bash
# Terminal 1
./build/glove client --transport http --bind 127.0.0.1 --port 9000
# Terminal 2
./build/glove primary --transport http --upstream-transport udp --bind 127.0.0.1 --listen-port 9001 --peer 127.0.0.1 --port 9000
# Terminal 3
./build/glove secondary --transport udp --hand left --peer 127.0.0.1 --port 9001
```

### Direct HTTP

```bash
# Terminal 1
./build/glove client --transport http --bind 127.0.0.1 --port 9000 --submission individual
# Terminal 2
./build/glove primary --transport http --peer 127.0.0.1 --port 9000 --submission individual
# Terminal 3 (may run before Terminal 2 once Client is ready)
./build/glove secondary --transport http --hand left --peer 127.0.0.1 --port 9000
```

For reversed assignment, add `--hand left` to Primary and change Secondary to
`--hand right`; keep modes, transports, and destinations unchanged. Combined
output remains physically left then right. In two-node modes only Primary's
hand option changes because no separate Secondary process exists.

After successful HTTP batch completion, stop Client with Ctrl-C/SIGTERM, or
select `--stay-open false` before launch for automatic completion exit. Restart
Client for each new run. The generic GET/POST examples above inspect or submit
to the running server; avoid injecting duplicate samples into a workflow run.

## Sources and related references

Command authority: [parser](../src/cli/parser.cpp), [CLI declarations](../include/cli/parser.hpp),
[main](../src/main.cpp), [Makefile](../Makefile), and [node defaults](../include/nodes/network.hpp).
Execution behavior: [Primary](../src/nodes/primary.cpp), [Secondary](../src/nodes/secondary.cpp),
[Client](../src/nodes/client.cpp), [mock fixtures](../src/mock.cpp), and
[generic HTTP client](../src/transport/http/client.cpp).

Existing references: [architecture](architecture.md), [protocols](protocols.md),
[HTTP API](api.md), [testing](testing.md), and [integration](integration.md).
