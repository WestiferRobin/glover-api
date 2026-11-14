# Team integration

This reference distinguishes implemented infrastructure from proposed integration
and unresolved team decisions. It does not approve a hardware protocol, sensor
algorithm, recognition hook, or keyboard API. See [architecture](architecture.md)
for ownership, [protocols](protocols.md) for wire contracts, [API](api.md) for HTTP,
[CLI](cli.md) for configuration, and [testing](testing.md) for desktop procedures.

## End-to-end boundary

The intended system conceptually moves from ESP32 sensors through finger-state
acquisition, glove models, communication, Client reception, recognition, and
keyboard output. That entire pipeline is not implemented. Recognition placement
and the production text handoff still require agreement; the conceptual order
is not an existing execution graph.

Available today: C++20 `Glove`/`GlovePair`, Primary/Secondary nodes, Client
reception, TCP/UDP/HTTP transport, text and JSON codecs, shared mock samples,
HTTP state inspection, and combined console output.

Not implemented here: sensor acquisition, firmware adapter, calibration, live
streaming, integrated recognition, or keyboard events. Desktop transport success
is not evidence that ESP32 firmware can use these classes or protocols directly.

## Current and potential integration points

| Boundary | Implemented now | Proposed or unresolved integration |
|---|---|---|
| State origin | `mockSamples()` supplies five left/right arrays | Hardware acquisition/conversion must replace the fixture source through a separately designed adapter |
| One hand | `Glove::setState()` and `setFinger()` accept logical values | Map calibrated sensor readings to physical finger slots |
| Pair | Primary owns a pair; setters update hand snapshots | Define synchronization/freshness rules for live hands |
| Communication | TCP/UDP relay; combined HTTP and direct HTTP | Select firmware transport and compatible lifecycle |
| Combined availability | `GlovePair::getState()`, parsed TCP states, complete UDP/HTTP Client slots | Choose a runtime recognition consumption hook |
| Recognition output | No execution path; dictionary is documented only | Define text/symbol events, timing, and delivery |
| Computer output | Client prints state arrays | Eduardo's adapter needs an agreed recognized-output contract |

No new public callback or hardware-input method is defined by this document.
Model setters and codecs are usable APIs, but current nodes still execute
fixture-driven paths. Client does not own or share Primary's `GlovePair`.

## Role, hand, and finger-state contract

Primary/Secondary are communication roles; Left/Right identify physical hands.
Both Primary Right + Secondary Left and Primary Left + Secondary Right remain
valid. Recognition must interpret physical positions, not assume Primary is right.

Each glove stores five Boolean values. Left order is pinky/ring/middle/index/thumb;
right order is thumb/index/middle/ring/pinky. Combined state is left five then
right five. Raw sensor readings are not this binary contract: the software does
not define sensor units or thresholds. Anatomical finger access maps to stored
positions; raw-array setters expect the already correct order. Details belong
in [model architecture](architecture.md#finger-ordering).

Five shared fixtures provide repeatable inputs. TCP/UDP text codecs and the HTTP
JSON codec are available; their exact representations remain canonical in
[protocols](protocols.md) and [API](api.md). Do not transmit raw C++ object memory
or introduce a separate HTTP fixture definition.

## Skyler — sensor acquisition

README identifies Skyler with sensors, thresholds, and calibration. It does not
specify sensor models, GPIO pins, electrical properties, or numeric thresholds.

| Requirement | Status in this repository | Decision needed |
|---|---|---|
| Logical finger representation | IMPLEMENTED | Preserve hand-specific slot mapping |
| Raw-to-logical conversion | PARTIALLY IMPLEMENTED | Destination API exists; conversion algorithm does not |
| Physical sensor acquisition | PLANNED | Hardware choice, reading interface, units |
| ESP32 acquisition firmware | PLANNED | Firmware boundary and build/deployment environment |
| Thresholds/calibration | PLANNED | Per-finger rules, storage, recalibration procedure |
| Sampling cadence | UNKNOWN | Frequency and event timing |
| Missing/bad sensor handling | UNKNOWN | Validity and recovery policy; Boolean arrays contain no validity field |

These statuses describe this repository, not private work a teammate may have
elsewhere. Confirm external progress with Skyler before integration. Calibration
and signal conversion cannot be inferred from the fact that `setState()` exists.

## Esteban — hardware communication

README assigns Secondary communication and a hardware transport adapter to
Esteban. Desktop roles and hand identities are implemented. A hardware adapter,
Wi-Fi provisioning, discovery, and production recovery are not.

The team must confirm firmware capabilities, network topology, how roles/hands
are assigned, where the receiving computer runs, and which existing payloads
can be reused. Do not assume ESP32 implements every desktop transport. Numeric
endpoint configuration is available on desktop; it is not device discovery.
Logical role/hand identity is not a unique authenticated device identity.

| Desktop path | Integration implication |
|---|---|
| TCP | Ordered stream requires LF framing and closure semantics; no application acknowledgment or timeout |
| UDP | Indexed datagrams tolerate reordering in simulation, but no delivery/retry/source-filter guarantee |
| Combined HTTP | Primary sends both hands to Client and checks acceptance |
| TCP/UDP-to-HTTP | Primary must receive, combine, and forward; failures can occur after downstream acceptance |
| Direct HTTP | Both roles send independently to Client; pairing uses equal indices |

Choose a firmware transport based on actual support, topology, latency
measurements, ordering/reliability needs, recovery, resource limits, and adapter
complexity. No production transport or performance target is selected here.

## Nikita — recognition

README identifies Nikita with recognition, chord timing, and decoded-result
delivery. Ten-value snapshots already exist through `GlovePair::getState()` and
`parseState()`. Client receives combined states and prints them; HTTP additionally
exposes latest-state inspection. There is no recognition execution hook in the
current node runtime. GET polling is not a guaranteed sample feed.

A consumer could be integrated at combined-state production or Client reception,
but placement and interface are PROPOSED, not existing public APIs. Recognition
must define event timing, chord matching, state transitions, debounce, held-state
repetition, invalid chords, output type, and error handling before integration.
The model does not decide when a chord begins or ends.

### Documented dictionary

The original project README recorded an agreed dictionary of 20 words and one space. This is a
repository documentation artifact, not executed recognition code. Preserve its
entries while confirming who owns its production representation and updates.
Compact bit strings below are dictionary notation, not TCP/UDP payload syntax.

| Physical ten-finger pattern | Documented result |
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
| 1111111111 | One literal space |

All-zero has no documented mapping. This does not establish runtime handling
for unknown chords, releases, repeated states, or separators between words.
Dictionary maintenance ownership is UNKNOWN; Nikita and the team must confirm it.
This table is the canonical documentation of those mappings.

## Eduardo — Client and keyboard output

README associates Eduardo with computer/keyboard output and the final result
interface. The shared Glover Client currently receives/validates states; it
prints arrays, not recognized words, and does not inject keyboard events.
HTTP Client buffers a complete simulation batch and permits inspection. Those
behaviors do not define a production recognition-to-keyboard stream.

The production handoff needs decisions on text versus symbols/key events,
encoding, destination, separators, special keys, repetition prevention, error
reporting, and operating-system support. No keyboard adapter or OS event API is
implemented. macOS desktop transport verification does not verify keyboard
permissions or behavior on any OS.

### Provisional decoded-results example

[examples/decoded_results.txt](../examples/decoded_results.txt) contains:

```text
"I"
"you"
"we"
"that"
" "
```

This file is not read by the running application and is not Client stdout. Its
provisional convention is five UTF-8 records: remove outer quotes and line
endings, preserve the interior, and add no separators. The last record contains
one literal space. Concatenation gives `Iyouwethat `, including the trailing
space. There are no escapes or embedded quotes. This is a standalone downstream
example for independent development, not an implemented keyboard contract.
Nikita and Eduardo must confirm its replacement or production interpretation.

## Wesley — shared infrastructure

README assigns Wesley nodes, networking, and CLI maintenance. Current shared
infrastructure includes models, Primary/Secondary/Client, TCP/UDP/HTTP, text and
JSON codecs, fixtures, API contracts, and these references. These references cover the current HTTP/JSON infrastructure as well as TCP/UDP.

Maintaining these shared boundaries does not assign Wesley every hardware,
recognition, or keyboard design decision. PROPOSED: infrastructure changes should
update their canonical reference and verification procedure in the same change,
with affected teammates reviewing interface changes. Documentation review and
new cross-component ownership require team agreement rather than inference.

## Unresolved conversion and streaming requirements

| Question / requirement | Suggested decision participants | Why it matters / affected contract | Blocks |
|---|---|---|---|
| What raw readings, units, and per-finger mapping are produced? | Skyler, Esteban | Conversion into five logical slots | Hardware adapter |
| How are thresholds and calibration selected/persisted? | Skyler | Repeatable binary state conversion | Reliable sensor input |
| What cadence and debounce rules apply? | Skyler, Nikita | Timing of state updates and chord transitions | Live recognition |
| What happens on failed or missing readings? | Skyler, Esteban, Wesley | Boolean state has no validity metadata | Robust hardware path |
| How are two hands synchronized and stale readings handled? | Esteban, Nikita, Wesley | Meaning of a combined snapshot | Live paired input |
| Which transport/topology, Wi-Fi setup, and device identities are used? | Esteban, hardware team | Endpoint and connection responsibilities | Firmware integration |
| How are continuous samples identified, sessions begun, and indices reused? | Esteban, Wesley, Nikita | Existing five-sample completion and duplicate rules | Continuous streaming |
| How are loss, disconnection, and reconnection handled? | Esteban, Wesley | Acceptance/replay/recovery policy | Production recovery |
| Where and how does recognition consume states? | Nikita, Wesley, Eduardo | Missing execution hook and processing placement | Recognition integration |
| Who owns dictionary changes and chord-to-symbol representation? | Nikita and team | Documented mappings versus executable lookup | Recognition contract |
| What are release, repetition, invalid-chord, and transition rules? | Nikita, Skyler | Recognition event timing | Repeatable recognition |
| What output type, encoding, and error contract connect recognition to output? | Nikita, Eduardo | Missing decoded-result interface | Keyboard handoff |
| Which OS, output destination, special keys, and repetition policy? | Eduardo, Nikita | Keyboard adapter and event semantics | Keyboard integration |
| Who approves protocol/version changes and acceptance records? | Wesley and affected owners | Compatibility and documentation ownership | Shared interface changes |

These are UNKNOWN requirements, not selected algorithms or approved assignments.
None blocks continued desktop fixture verification or independent component work.
Do not choose sensor values, rates, latency budgets, or OS APIs by assumption.

Current UDP/HTTP sessions have indices 0–4 and stop accepting new sample slots
once filled; reuse conflicts in a retained HTTP session. TCP Client can accept
other frame counts, but the mock relay requires five plus EOF. Thus none of the
current simulations supplies a complete continuous-streaming contract. Define
what happens after sample five, session reset, missed readings, synchronization,
recognition delivery, and reconnection before implementing production streaming.
No current protocol change is proposed as part of this documentation task.

## Integration readiness

Owners below reflect README roles; expanded cross-component decisions are
suggested participants and need confirmation.

| Component | Current status | Owner / participants | Next decision |
|---|---|---|---|
| Glove/GlovePair | IMPLEMENTED | Shared infrastructure; Skyler consumes | Hardware-to-slot mapping |
| Desktop nodes/transports/codecs/CLI/API | IMPLEMENTED | Wesley; Esteban for adapter use | Firmware compatibility and production lifecycle |
| Physical acquisition | PLANNED, not implemented here | Skyler | Sensor output interface |
| Calibration/conversion | PARTIALLY IMPLEMENTED destination only | Skyler | Conversion rules |
| Hardware communication adapter | PLANNED | Esteban | Transport and topology |
| Continuous paired streaming | UNKNOWN contract | Esteban, Wesley, Nikita | Session, identity, synchronization, recovery |
| Recognition | PLANNED, not integrated | Nikita | Execution hook and output contract |
| Dictionary runtime/maintenance | Documented mappings; UNKNOWN ownership | Nikita/team | Representation and approval process |
| Keyboard output | PLANNED, not integrated | Eduardo | OS/output interface |
| Decoded-text handoff | Provisional example only | Nikita, Eduardo | Production semantics |

## Independent development and proposed sequence

Teams can use existing fixed inputs without waiting for hardware: Skyler can
validate conversion against known finger positions; Esteban can verify codecs
and desktop paths; Nikita can develop recognition against ten-value snapshots;
Eduardo can independently consume the provisional decoded example after agreeing
its interpretation. These are development opportunities, not wired runtime paths.

PROPOSED dependency order, not an approved schedule:

1. Confirm raw sensor representation, physical mapping, calibration, and failures.
2. Confirm firmware transport/topology and design the hardware conversion adapter.
3. Approve a live session/sampling/recovery contract before extending fixed indices.
4. Demonstrate one physical glove producing valid logical states and transmitting.
5. Demonstrate both hands yielding correctly ordered, appropriately paired states.
6. Agree recognition placement, timing, dictionary ownership, and output contract;
   this design and fixture-based development can proceed alongside hardware work.
7. Connect recognition through an explicitly implemented hook and verify it.
8. Agree keyboard handoff and OS scope; downstream fixture development can proceed
   independently, then integrate the keyboard adapter.
9. Verify the end-to-end hardware path, failure recovery, and regression workflows.

Preserve current desktop behavior while interfaces are designed. Do not repurpose
GET latest-state inspection into a reliable event queue without an approved change.

## Proposed integration acceptance criteria

These are future criteria, not completed tests or numeric performance targets:

- Known physical finger activations map to the correct five logical positions.
- Both role/hand assignments produce the same physically ordered ten-value state.
- Conversion handles calibration and missing readings according to the agreed rules.
- Communication validates payloads and reports loss/disconnection/recovery as agreed.
- Continuous operation proceeds beyond five samples under the approved new contract.
- A recorded state-transition sequence yields repeatable recognition events,
  including unknown chords, release, and held/repeated states.
- Recognized output reaches the agreed destination exactly as specified, including
  literal spaces/special keys and repetition prevention.
- Each supported OS has explicit keyboard verification evidence.
- An end-to-end hardware demonstration records inputs, combined states, recognition
  results, actual output, and failure behavior; existing desktop regressions pass.

## Contributor and assistant workflow

Read the repository and your assigned component before proposing changes. Use
model, codec, and node headers plus implementations rather than a CLI transcript
alone. Preserve physical finger order, dictionary mappings, wire formats, and CLI
behavior unless the team explicitly agrees to change them. Use fixed inputs
while upstream hardware or downstream interfaces are absent.

Describe the exact component scope, existing input/output contract, and APIs to
reuse. Distinguish executed verification from inspection and untested assumptions.
For manual instructions, label terminals and guide one scenario at a time. Keep
changes reviewable; do not stage, commit, or push without authorization.

BLE is deferred, not active code. The previous macOS experiment is preserved at
commit `7dd12177081c26dcbe8b4203c8b9711db111a043`. Plover compatibility remains a
future goal outside the MVP; it is not an implemented keyboard interface.

## Available now and open work

Available now: desktop models, five mock samples, TCP/UDP/HTTP communication,
JSON submissions, Client state inspection, and combined state output. Not yet
available: real ESP32 acquisition, continuous hardware streaming, production
connection recovery, integrated chord recognition, or keyboard event generation.

Raw sensor format, conversion, calibration, cadence, firmware transport,
synchronization, continuous identities/sessions, recognition hook/output, dictionary
ownership, and keyboard delivery remain unresolved as identified above. This
reference records boundaries; it does not turn them into approved requirements.

Evidence: [README team assignments](../README.md#team-integration),
[Glove API](../include/glove/glove.hpp), [pair API](../include/glove/pair.hpp),
[Primary](../src/nodes/primary.cpp), [Secondary](../src/nodes/secondary.cpp),
[Client](../src/nodes/client.cpp), [fixtures](../src/mock.cpp), and the linked
codec/protocol references. No recognition or keyboard execution implementation
was found in `include/` or `src/` during this documentation pass.
