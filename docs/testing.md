# Manual verification

This is the reproducible desktop verification guide. It covers local simulation,
TCP/UDP pipelines, HTTP combined and individual delivery, mixed relays, endpoint
validation, and lifecycle checks. These procedures do not prove ESP32 acquisition,
live streaming, recognition, or keyboard output. Contracts remain in
[architecture](architecture.md), [protocols](protocols.md), [API](api.md), and
[CLI](cli.md). Integration boundaries are in [integration](integration.md).

## Prerequisites and build

Use a C++20-capable `c++`, Make, a Unix-like shell, POSIX IPv4 sockets, and pthread
support. macOS requires installed/selected Apple developer tools and `xcrun`;
the Makefile selects the SDK. HTTP and JSON dependencies are vendored; no package
installation is needed. macOS is the recorded verification platform. Linux is
unverified; Windows and ESP32 support are not implemented.

From the repository root:

```bash
make clean
make
./build/glove --help
```

Expect successful compilation and exit 0. The executable is `build/glove`.
Optional diagnostic tests below use `curl` and macOS `lsof`; packet injection
requires an independently available tool, not a Glover command. No new test
scripts or dependencies are supplied by this guide.

## Terminals, readiness, and exit codes

| Terminal | Responsibility | Endpoint |
|---|---|---|
| 1 | Client | 127.0.0.1:9000 |
| 2 | Primary | Receives upstream at 127.0.0.1:9001 when relaying |
| 3 | Secondary | Sends to Primary, or directly to HTTP Client |

Use the repository root in every terminal. Each scenario below is a fresh run.
Start Client, then Primary, then Secondary; direct HTTP allows either sender to
start first after Client is ready. Two-node cases need only terminals 1 and 2.
Do not paste a whole multi-terminal block into one shell: listeners block.

HTTP Client announces `HTTP Client listening on 127.0.0.1:9000` on stderr.
TCP/UDP receivers wait silently. In the next unused terminal, inspect sockets:

```bash
lsof -nP -i4TCP:9000 -sTCP:LISTEN
lsof -nP -i4TCP:9001 -sTCP:LISTEN
lsof -nP -i4UDP:9000
lsof -nP -i4UDP:9001
```

Use only checks matching the current transport/hop. Confirm the owner is the
expected Glover process. Do not use a TCP connection probe: the single-client
TCP receiver could accept the probe as its session. Primary TCP-to-TCP connects
to Client before opening its upstream listener. Wait for that listener before
starting Secondary. No automatic retry exists.

After each process returns to its prompt, immediately run:

```bash
echo $?
```

Expect 0 on successful runs. Check before another command overwrites the status.
Successful network senders print nothing to stdout/stderr. Successful TCP/UDP
Client prints the expected snapshots and no diagnostics. HTTP Client additionally
prints its listening diagnostic and stays open by default; check its exit after
completed Ctrl-C shutdown. For every pipeline, verify every process's exit as
well as Client output. Sender success alone is insufficient.

## Canonical output

The five samples at indices 0–4 are defined once in `mockSamples()` in
[src/mock.cpp](../src/mock.cpp). Expected combined stdout is:

```text
[1,0,0,0,0,0,0,0,0,1]
[0,1,0,0,0,0,0,0,1,0]
[0,0,1,0,0,0,0,1,0,0]
[1,0,1,0,0,0,0,0,1,0]
[1,1,1,1,1,1,1,1,1,1]
```

Each line ends with a newline. Left storage is pinky/ring/middle/index/thumb;
right storage is thumb/index/middle/ring/pinky. Combined output concatenates
left then right without reversal. Role assignment changes which process owns
which physical hand, not these values or their order. Receivers validate state
shape; they do not compare submitted values to this oracle.

## Local simulation

Run each separately, checking exit immediately afterward:

```bash
./build/glove
./build/glove mock
./build/glove primary
```

Each prints exactly the canonical output, stderr empty, exit 0. Bare local CLI
has no hand-selection option: `primary --hand left` is not a local variant.
Left-Primary local construction is available through the C++ model/node API,
not a supported local CLI test. Network scenarios below exercise both hands.

### Help and invalid command

```bash
./build/glove --help
echo $?
./build/glove invalid
echo $?
```

Help prints usage on stdout and exits 0. The unknown command prints
`Unknown command: invalid` and help on stderr and exits 1. These checks need
no network processes.

## Two-node TCP

```bash
# Terminal 1 — Client
./build/glove client --transport tcp --bind 127.0.0.1 --port 9000
# Terminal 2 — Primary Right
./build/glove primary --transport tcp --hand right --peer 127.0.0.1 --port 9000
```

Primary loads both fixture hands, sends five frames, and closes. Client prints
as frames arrive and returns at clean EOF. Client does not independently enforce
five samples: compare all five lines and both exit codes.

Compare Client stdout with [canonical output](#canonical-output), using the
terminal/exit checks above. Restart all processes before a new run.

Repeat with fresh processes and the same Terminal 1 command. Replace the
sender commands with these reversed-hand variants:

```bash
# Terminal 2 — Primary Left
./build/glove primary --transport tcp --hand left --peer 127.0.0.1 --port 9000
```

Expected combined output is unchanged. Check every process exit code again.

## Three-node TCP

```bash
# Terminal 1 — Client
./build/glove client --transport tcp --bind 127.0.0.1 --port 9000
# Terminal 2 — Primary Right
./build/glove primary --transport tcp --hand right --bind 127.0.0.1 --listen-port 9001 --peer 127.0.0.1 --port 9000
# Terminal 3 — Secondary Left
./build/glove secondary --transport tcp --hand left --peer 127.0.0.1 --port 9001
```

Primary pairs incoming hand frames by accepted message order; no application
index travels over TCP. It requires five samples followed by clean upstream EOF.
All three should return 0. Client can exit 0 after an upstream relay failure,
so inspect Primary too.

Compare Client stdout with [canonical output](#canonical-output), using the
terminal/exit checks above. Restart all processes before a new run.

Repeat with fresh processes and the same Terminal 1 command. Replace the
sender commands with these reversed-hand variants:

```bash
# Terminal 2 — Primary Left
./build/glove primary --transport tcp --hand left --bind 127.0.0.1 --listen-port 9001 --peer 127.0.0.1 --port 9000
# Terminal 3 — Secondary Right
./build/glove secondary --transport tcp --hand right --peer 127.0.0.1 --port 9001
```

Expected combined output is unchanged. Check every process exit code again.

## Three-node UDP

```bash
# Terminal 1 — Client
./build/glove client --transport udp --bind 127.0.0.1 --port 9000 --timeout-ms 30000
# Terminal 2 — Primary Right
./build/glove primary --transport udp --hand right --bind 127.0.0.1 --listen-port 9001 --peer 127.0.0.1 --port 9000 --timeout-ms 30000
# Terminal 3 — Secondary Left
./build/glove secondary --transport udp --hand left --peer 127.0.0.1 --port 9001
```

Start the complete sequence within 30 seconds. Indexed envelopes preserve
fixture pairing despite reordered arrival. Client prints only after five valid
unique indices, ordered 0–4. All processes should return 0. UDP guarantees
neither delivery nor ordering. Missing samples cause deadline failure and no
partial Client batch. Pure UDP defaults to 5000 ms; these examples override it.

Compare Client stdout with [canonical output](#canonical-output), using the
terminal/exit checks above. Restart all processes before a new run.

Repeat with fresh processes and the same Terminal 1 command. Replace the
sender commands with these reversed-hand variants:

```bash
# Terminal 2 — Primary Left
./build/glove primary --transport udp --hand left --bind 127.0.0.1 --listen-port 9001 --peer 127.0.0.1 --port 9000 --timeout-ms 30000
# Terminal 3 — Secondary Right
./build/glove secondary --transport udp --hand right --peer 127.0.0.1 --port 9001
```

Expected combined output is unchanged. Check every process exit code again.

## Two-node HTTP

```bash
# Terminal 1 — Client
./build/glove client --transport http --bind 127.0.0.1 --port 9000 --submission combined --timeout-ms 30000 --stay-open true
# Terminal 2 — Primary Right
./build/glove primary --transport http --hand right --peer 127.0.0.1 --port 9000 --submission combined
```

Primary submits five combined samples and requires 204 for each. Client prints
the complete canonical batch once. Primary returns 0; Client stays open for
endpoint inspection, then exits 0 on Ctrl-C after completion.

Compare Client stdout with [canonical output](#canonical-output), using the
terminal/exit checks above. For HTTP, perform [endpoint inspection](#http-endpoint-inspection)
after completion, then stop Client before a new run.

Repeat with fresh processes and the same Terminal 1 command. Replace the
sender commands with these reversed-hand variants:

```bash
# Terminal 2 — Primary Left
./build/glove primary --transport http --hand left --peer 127.0.0.1 --port 9000 --submission combined
```

Expected combined output is unchanged. HTTP role GETs must identify Primary left and Secondary right.

## TCP-to-HTTP relay

```bash
# Terminal 1 — Client
./build/glove client --transport http --bind 127.0.0.1 --port 9000 --submission combined --timeout-ms 30000 --stay-open true
# Terminal 2 — Primary Right
./build/glove primary --transport http --upstream-transport tcp --hand right --bind 127.0.0.1 --listen-port 9001 --peer 127.0.0.1 --port 9000 --submission combined
# Terminal 3 — Secondary Left
./build/glove secondary --transport tcp --hand left --peer 127.0.0.1 --port 9001
```

Primary combines TCP hand frames with local fixtures and forwards HTTP indices
by accepted position. Secondary closes after five sends; Primary still requires
clean upstream EOF. Both senders should return 0. Client can complete before
Primary validates EOF, so its completed batch is not proof of relay success.
TCP has no session timeout; the HTTP Client deadline cannot cancel its wait.

Compare Client stdout with [canonical output](#canonical-output), using the
terminal/exit checks above. For HTTP, perform [endpoint inspection](#http-endpoint-inspection)
after completion, then stop Client before a new run.

Repeat with fresh processes and the same Terminal 1 command. Replace the
sender commands with these reversed-hand variants:

```bash
# Terminal 2 — Primary Left
./build/glove primary --transport http --upstream-transport tcp --hand left --bind 127.0.0.1 --listen-port 9001 --peer 127.0.0.1 --port 9000 --submission combined
# Terminal 3 — Secondary Right
./build/glove secondary --transport tcp --hand right --peer 127.0.0.1 --port 9001
```

Expected combined output is unchanged. HTTP role GETs must identify Primary left and Secondary right.

## UDP-to-HTTP relay

```bash
# Terminal 1 — Client
./build/glove client --transport http --bind 127.0.0.1 --port 9000 --submission combined --timeout-ms 30000 --stay-open true
# Terminal 2 — Primary Right
./build/glove primary --transport http --upstream-transport udp --hand right --bind 127.0.0.1 --listen-port 9001 --peer 127.0.0.1 --port 9000 --submission combined --timeout-ms 30000
# Terminal 3 — Secondary Left
./build/glove secondary --transport udp --hand left --peer 127.0.0.1 --port 9001
```

Primary uses each UDP index to select its local fixture and forwards that
index over HTTP. Its total UDP receive budget includes synchronous forwarding;
a slow callback can delay timeout reporting. Both sender processes should exit
0 and Client should print the complete batch. CLI relay timeout defaults to
30000 ms even when omitted; it is distinct from the C++ constructor default.

Compare Client stdout with [canonical output](#canonical-output), using the
terminal/exit checks above. For HTTP, perform [endpoint inspection](#http-endpoint-inspection)
after completion, then stop Client before a new run.

Repeat with fresh processes and the same Terminal 1 command. Replace the
sender commands with these reversed-hand variants:

```bash
# Terminal 2 — Primary Left
./build/glove primary --transport http --upstream-transport udp --hand left --bind 127.0.0.1 --listen-port 9001 --peer 127.0.0.1 --port 9000 --submission combined --timeout-ms 30000
# Terminal 3 — Secondary Right
./build/glove secondary --transport udp --hand right --peer 127.0.0.1 --port 9001
```

Expected combined output is unchanged. HTTP role GETs must identify Primary left and Secondary right.

## Direct HTTP

```bash
# Terminal 1 — Client
./build/glove client --transport http --bind 127.0.0.1 --port 9000 --submission individual --timeout-ms 30000 --stay-open true
# Terminal 2 — Primary Right
./build/glove primary --transport http --hand right --peer 127.0.0.1 --port 9000 --submission individual
# Terminal 3 — Secondary Left
./build/glove secondary --transport http --hand left --peer 127.0.0.1 --port 9000
```

Each sender submits only its physical hand. Run Primary first to inspect the
intermediate state: Primary role GET returns index 4, combined and Secondary
GETs return 204, and Client has printed no batch. Start Secondary before the
deadline. Client pairs equal indices and prints the canonical output once.
Either sender may instead run first or both concurrently. Both must finish
with exit 0; one sender’s 204s do not establish paired completion.

Compare Client stdout with [canonical output](#canonical-output), using the
terminal/exit checks above. For HTTP, perform [endpoint inspection](#http-endpoint-inspection)
after completion, then stop Client before a new run.

Repeat with fresh processes and the same Terminal 1 command. Replace the
sender commands with these reversed-hand variants:

```bash
# Terminal 2 — Primary Left
./build/glove primary --transport http --hand left --peer 127.0.0.1 --port 9000 --submission individual
# Terminal 3 — Secondary Right
./build/glove secondary --transport http --hand right --peer 127.0.0.1 --port 9000
```

Expected combined output is unchanged. HTTP role GETs must identify Primary left and Secondary right.

## HTTP endpoint inspection

Use a sender terminal after it returns, while Client remains open:

```bash
./build/glove http get --host 127.0.0.1 --port 9000 --path /health
./build/glove http get --host 127.0.0.1 --port 9000 --path /gloves
./build/glove http get --host 127.0.0.1 --port 9000 --path '/gloves?role=primary'
./build/glove http get --host 127.0.0.1 --port 9000 --path '/gloves?role=secondary'
```

After full completion all four return 200. Health is `{"status":"ok"}`. State
responses identify index 4; combined GET has index/left/right, role GET has
role/hand/index/fingers. Check the physical hands against the selected run.
Before any POST, health returns 200 and all state GETs return empty 204.
GET selects highest available index, not an event queue. See [API](api.md).

The generic CLI prints `HTTP <status>` to stderr and body bytes to stdout
without appending a newline. Complete responses, including errors, exit 0.
Run `echo $?` immediately after an individual command to inspect its process
status; it does not replace checking the HTTP status.

## Isolated POST checks

These intentionally incomplete checks use fresh sessions, separate from the
five-sample workflows. Terminal 1:

```bash
./build/glove client --transport http --bind 127.0.0.1 --port 9000 --submission combined --timeout-ms 120000 --stay-open true
```

Terminal 2, after readiness:

```bash
./build/glove http post --host 127.0.0.1 --port 9000 --path /gloves --content-type application/json --body '{"role":"primary","hand":"right","index":0,"left":[1,0,0,0,0],"right":[0,0,0,0,1]}'
./build/glove http get --host 127.0.0.1 --port 9000 --path /gloves
```

Expect POST 204, GET 200 with index 0 and the submitted arrays. Client prints
no batch yet. Stop it with Ctrl-C (incomplete session exit 1), then restart
Terminal 1 with individual mode:

```bash
./build/glove client --transport http --bind 127.0.0.1 --port 9000 --submission individual --timeout-ms 120000 --stay-open true
```

Terminal 2:

```bash
./build/glove http post --host 127.0.0.1 --port 9000 --path /gloves --content-type application/json --body '{"role":"secondary","hand":"left","index":0,"fingers":[1,0,0,0,0]}'
./build/glove http get --host 127.0.0.1 --port 9000 --path '/gloves?role=secondary'
./build/glove http get --host 127.0.0.1 --port 9000 --path /gloves
```

Expect 204, 200 with Secondary/left/index 0, then empty 204 because Primary has
not arrived. No batch is printed. Stop Client: exit 1 is expected here. A POST
acknowledges in-memory acceptance, not full session, recognition, or persistence.

## Negative HTTP checks

Require `curl`. Start a fresh combined Client using the 120000 ms command above;
run the checks below in Terminal 2 before its deadline. No glove sender should
run concurrently. `curl -i` displays HTTP status, headers, and body. Without
`--fail`, curl can also exit 0 on an HTTP error. Check the displayed status.
Application errors are plain text; schema detail belongs in [API](api.md).

### Unknown route

```bash
curl -i http://127.0.0.1:9000/missing
```

Expect **404**. Unregistered path rejection.

### Removed route (negative compatibility check only)

```bash
curl -i http://127.0.0.1:9000/test
```

Expect **404**. The removed route must not work.

### Unsupported method

```bash
curl -i -X PUT http://127.0.0.1:9000/gloves
```

Expect **405**. Method restriction; Allow identifies GET and POST.

### Invalid role query

```bash
curl -i 'http://127.0.0.1:9000/gloves?role=unknown'
```

Expect **400**. Invalid role filter rejection.

### Malformed JSON

```bash
curl -i -H 'Content-Type: application/json' --data-binary '{' http://127.0.0.1:9000/gloves
```

Expect **400**. Schema/parsing rejection without accepting a sample.

### Missing required fields

```bash
curl -i -H 'Content-Type: application/json' --data-binary '{}' http://127.0.0.1:9000/gloves
```

Expect **400**. Schema/parsing rejection without accepting a sample.

### Unknown field

```bash
curl -i -H 'Content-Type: application/json' --data-binary '{"role":"primary","hand":"right","index":0,"left":[1,0,0,0,0],"right":[0,0,0,0,1],"extra":1}' http://127.0.0.1:9000/gloves
```

Expect **400**. Schema/parsing rejection without accepting a sample.

### Duplicate JSON key

```bash
curl -i -H 'Content-Type: application/json' --data-binary '{"role":"primary","role":"primary","hand":"right","index":0,"left":[1,0,0,0,0],"right":[0,0,0,0,1]}' http://127.0.0.1:9000/gloves
```

Expect **400**. Schema/parsing rejection without accepting a sample.

### Invalid finger value

```bash
curl -i -H 'Content-Type: application/json' --data-binary '{"role":"primary","hand":"right","index":0,"left":[2,0,0,0,0],"right":[0,0,0,0,1]}' http://127.0.0.1:9000/gloves
```

Expect **400**. Schema/parsing rejection without accepting a sample.

### Invalid finger count

```bash
curl -i -H 'Content-Type: application/json' --data-binary '{"role":"primary","hand":"right","index":0,"left":[1,0],"right":[0,0,0,0,1]}' http://127.0.0.1:9000/gloves
```

Expect **400**. Schema/parsing rejection without accepting a sample.

### Invalid sample index

```bash
curl -i -H 'Content-Type: application/json' --data-binary '{"role":"primary","hand":"right","index":5,"left":[1,0,0,0,0],"right":[0,0,0,0,1]}' http://127.0.0.1:9000/gloves
```

Expect **400**. Schema/parsing rejection without accepting a sample.

### Wrong submission mode

```bash
curl -i -H 'Content-Type: application/json' --data-binary '{"role":"secondary","hand":"left","index":0,"fingers":[1,0,0,0,0]}' http://127.0.0.1:9000/gloves
```

Expect **409**. Valid individual shape conflicts with combined mode.

### Unsupported content type

```bash
curl -i -H 'Content-Type: text/plain' --data-binary '{}' http://127.0.0.1:9000/gloves
```

Expect **415**. Media-type rejection precedes JSON validation.

### Duplicate and mapping conflict

Still in the fresh combined session, first accept index 0, then repeat it:

```bash
curl -i -H 'Content-Type: application/json' --data-binary '{"role":"primary","hand":"right","index":0,"left":[1,0,0,0,0],"right":[0,0,0,0,1]}' http://127.0.0.1:9000/gloves
curl -i -H 'Content-Type: application/json' --data-binary '{"role":"primary","hand":"right","index":0,"left":[1,0,0,0,0],"right":[0,0,0,0,1]}' http://127.0.0.1:9000/gloves
curl -i -H 'Content-Type: application/json' --data-binary '{"role":"primary","hand":"left","index":1,"left":[0,1,0,0,0],"right":[0,0,0,1,0]}' http://127.0.0.1:9000/gloves
./build/glove http get --host 127.0.0.1 --port 9000 --path /gloves
```

Expect 204, 409 (`Duplicate sample`), 409 (`Role-to-hand mapping conflict`),
and GET 200 still at index 0. Rejections must not replace accepted state.
Stop Client; this incomplete session should exit 1. Restart before reuse.

## TCP/UDP negative cases

For timeout without packet injection, run with no sender:

```bash
./build/glove client --transport udp --bind 127.0.0.1 --port 9000 --timeout-ms 1000
echo $?
```

Expect exit 1, no state stdout, and a timeout diagnostic listing missing indices
0,1,2,3,4. The deadline is total, not renewed per packet.

The following cases require a custom TCP/UDP packet injector already available
to the tester. They are protocol-level procedures, not supported Glover sender
commands; no new injector is included. Use fresh receivers for every row.
For TCP relay cases, start the three-node TCP Client and Primary above and
replace Secondary with the injector at port 9001. For receiver-only cases,
start the corresponding Client on port 9000. Use a sufficiently long UDP timeout.

| Case | Exact input/action | Expected behavior |
|---|---|---|
| Malformed TCP state | Send `bad` followed by LF to TCP Client, then close | Client exit 1, invalid-state diagnostic |
| Incomplete TCP frame | Send a valid combined text payload without LF, then close | Framing error, exit 1 |
| Early relay EOF | Connect to Primary and close without frames | Primary exit 1 for fewer than five; Client may exit 0 with no output |
| Extra relay sample | Send six LF-terminated `S5 L 10000` frames to right-Primary, then close | Primary rejects sixth, exit 1; Client may already have five valid states and exit 0 |
| Invalid UDP envelope | Send one datagram `U2 0 invalid` to UDP Client | Exit 1, envelope error, no state batch |
| Duplicate UDP index | Twice send `U1 0 [1,0,0,0,0,0,0,0,0,1]` before completion | Duplicate-index failure, exit 1, no batch |
| Missing UDP index | Send only that index-0 datagram, then no more | Timeout, exit 1, missing indices 1–4, no batch |

TCP transport preserves LF boundaries across arbitrary socket reads. Injector
send success is not receiver acceptance. UDP loss can turn a planned duplicate
case into a timeout; inspect the actual receiver diagnostic. See
[protocols](protocols.md) for authoritative formats and limits.

## Shutdown and port reuse

TCP/UDP successful workflows return naturally. HTTP stay-open Client remains
running after its batch: inspect GETs, then Ctrl-C in Terminal 1 and immediately
check exit 0. SIGTERM has the same completed/incomplete distinction. Stop only
the specific process you own; do not use broad process-kill commands.

Repeat a successful HTTP workflow with `--stay-open false` on Client to verify
automatic completion shutdown and exit 0. For incomplete HTTP shutdown, start
fresh Client, submit nothing, then Ctrl-C: expect exit 1 and no batch. To check
HTTP timeout without intervention:

```bash
./build/glove client --transport http --bind 127.0.0.1 --port 9000 --timeout-ms 1000 --stay-open false
echo $?
```

Expect exit 1 and missing indices 0–4. No sender or GET resets the deadline.
After shutdown, inspect the appropriate endpoints:

```bash
lsof -nP -i4TCP:9000 -sTCP:LISTEN
lsof -nP -i4TCP:9001 -sTCP:LISTEN
lsof -nP -i4UDP:9000
lsof -nP -i4UDP:9001
```

No matching sockets means no output and typically lsof exit 1. An unrelated
system/Docker listener is not a Glover leak; inspect PID, command, address, and
protocol. Do not claim release without observing it. Restart the same workflow
on the same ports and verify fresh empty HTTP state before sending. Each run
requires a fresh Client session; a completed stay-open server rejects repeats.
TCP/UDP interruption does not share HTTP's graceful exit-0 contract.

## Verification matrix and evidence

A procedure is not a PASS record. Historical manual evidence and the final
synchronization checks below have different provenance.

### Historical evidence

The project README at implementation commit `96e4e0d` recorded Wesley's
October 15, 2026 macOS manual verification: clean warning-free compilation,
local output, two-node TCP, three-node TCP/UDP with both hands, UDP timeout,
invalid commands, and help. Exit codes were explicitly checked for reversed
three-node TCP and both UDP assignments. Its November 13 desktop HTTP account
reported both hands for combined HTTP, mixed relays, and direct HTTP.

The HTTP reports included health/state/role inspection, signal and automatic
shutdown, port reuse, and rejected responses without retries. Mixed-relay
reports covered reordered UDP, malformed/wrong-hand input, early/incomplete TCP
EOF, a sixth sample, duplicate/invalid/missing UDP indices, downstream failures,
and Client stopping during forwarding. Direct reports covered Primary-first,
Secondary-first, and concurrent senders. These are preserved historical reports,
not newly observed manual checks. The documentation request additionally
reported manual reversed-hand direct HTTP success with five snapshots and
correct role GETs; that is user-reported evidence.

### Final synchronization verification — November 13, 2026

Automated local process checks were executed on macOS against source commit
`96e4e0d` with documentation changes only. No permanent test harness was added.
The executable was rebuilt with `make clean` and `make`; compilation completed
without warnings. Help ran successfully and mock output matched the canonical
five lines. Earlier documentation smoke checks also checked default, `mock`,
and bare `primary`, with exit 0 and empty stderr.

| Workflow | Primary Right | Primary Left | Evidence |
|---|---|---|---|
| Local | PASS default fixture assignment | NOT RUN; C++ only, no local CLI flag | Local smoke, not hardware |
| Two-node TCP | PASS | PASS | Automated processes; exact output; all exits 0 |
| Three-node TCP | PASS | PASS | Automated processes; exact output; all exits 0 |
| Three-node UDP | PASS | PASS | Automated processes; exact output; all exits 0 |
| Two-node HTTP | PASS | PASS | Automated processes plus GET inspection and completed SIGTERM |
| TCP-to-HTTP | PASS | PASS | Automated processes plus GET inspection and completed SIGTERM |
| UDP-to-HTTP | PASS | PASS | Automated processes plus GET inspection and completed SIGTERM |
| Direct HTTP | PASS | PASS | Automated independent senders plus GET inspection and completed SIGTERM |

The 14 network runs used available loopback ports rather than assuming 9000/9001
were free. Receive budgets were 10000 ms, process waits bounded, and socket
readiness checked without connecting to single-client TCP listeners. Each HTTP
run checked health, combined index-4 arrays, both role/hand views, and graceful
completed SIGTERM exit 0. All test-owned processes exited; lsof confirmed no
remaining listeners or UDP sockets at their endpoints.

All 16 curl requests in the negative HTTP procedure were executed in one fresh
combined session on another available port and matched expected statuses,
including the accepted setup POST. A generic CLI POST returning HTTP 400 was
separately checked for process exit 0 and its plain-text body. Stopping that
incomplete server returned exit 1, no partial output, and missing indices 1–4;
its listener was confirmed closed.

NOT RUN during final synchronization: packet-injection TCP/UDP negatives,
automatic HTTP shutdown, Ctrl-C-specific behavior, timeout waits, and repeated
binding to the same port. These remain source-checked procedures with historical
evidence where noted; checking listener closure is not itself a port-rebind test.
No new personally observed multi-terminal manual PASS or hardware PASS is claimed.

For future runs record commit, platform, date, hands, actual stdout/stderr,
all exit codes, endpoint/port checks, and whether execution was manual or
scripted. Rerun relevant checks after behavioral changes; this repository still
has no committed automated test suite.

## Troubleshooting

| Symptom | Check/action |
|---|---|
| Address already in use | Inspect owner with lsof; stop your old instance or choose unused ports consistently |
| Connection refused | Start Client first; confirm the correct TCP/HTTP listener before sending |
| Secondary cannot reach Primary | Confirm Primary upstream port 9001 is bound, not just Client port 9000 |
| Wrong-hand rejection | Use opposite physical hands; role is not physical identity |
| Submission conflict | Combined Client for combined/relay Primary; individual Client for direct senders |
| Duplicate sample | Restart Client for a new run; do not replay accepted indices |
| Incomplete session | Check all five indices, both roles in individual mode, and every sender exit |
| UDP timeout | Restart all processes and launch before the total deadline; inspect packet delivery |
| HTTP 415 | Set `--content-type application/json` for generic POST |
| Stale GET state | A stay-open Client retains its completed session; restart it |
| POST 204 but no batch | Acceptance may be only one sample or one hand; all five complete pairs are required |
| TCP relay hangs after five | Upstream must close cleanly; TCP has no session timeout |

Source checks use [parser](../src/cli/parser.cpp), [Primary](../src/nodes/primary.cpp),
[Secondary](../src/nodes/secondary.cpp), [Client](../src/nodes/client.cpp),
[JSON codec](../src/codec/glove_json.cpp), [Makefile](../Makefile), and the linked
protocol/model references. Runtime results must be recorded separately from
these source-backed expectations.
