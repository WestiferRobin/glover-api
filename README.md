# Glover API

Glover is the C++20 software foundation for the SDSU CS 576 two-glove
stenography project. It represents physical finger states and communicates them
between glove nodes and a computer-side Client.

The current implementation is a desktop simulation using five shared mock
samples. ESP32 sensor acquisition, recognition, and keyboard output are not yet
integrated. Desktop networking success does not establish hardware compatibility.

## Current implementation

- `Glove` represents one physical hand; `GlovePair` owns left/right snapshots.
- Primary combines local and received hand states; Secondary sends its own hand.
- Client receives and validates states and prints combined ten-finger snapshots.
- TCP, UDP, and HTTP support the desktop workflows below.
- Primary can relay TCP/UDP input to HTTP; both gloves can also submit directly
  to the HTTP Client in individual mode.
- Text and JSON codecs validate their respective communication contracts.
- Every mock workflow reuses the five fixtures in `src/mock.cpp`.

See [architecture](docs/architecture.md) for components, ownership, and models.

## Requirements

- A C++20-capable compiler available as `c++`, Make, and a Unix-like shell.
- POSIX IPv4 sockets and pthread support.
- On macOS, installed and selected Apple developer tools providing `c++` and
  `xcrun`; the Makefile selects their SDK through `SDKROOT`.

macOS is the recorded desktop verification platform. Linux is unverified;
Windows and ESP32 support are not implemented. Network examples use loopback
and available ports; no external service or sensor is required.

## Build

From the repository root:

```bash
make clean
make
```

The executable is `build/glove`. `make` builds with C++20, warnings enabled, and
`-pthread`; vendored dependencies require no package installation. An unchanged
build does not recompile. To select another compiler, use `make CXX=clang++`.
`make run` builds when needed and runs the default simulation.

## Testing

Run from the repository root:

```bash
make test-unit         # Socket-free C++ model, codec, framing, mock and parser tests
make test-integration  # Loopback transport, HTTP API, CLI and node workflow tests
make test              # Both suites; run before pushing
```

The targets build their required executables. Tests need no physical gloves,
ESP32 devices, external services, or Python packages. In addition to the build
tools above, integration tests require Python 3, `lsof`, and local IPv4 networking.
macOS is locally verified; other platforms are not yet verified.

Tests live in `tests/unit/`, `tests/integration/`, and `tests/fixtures/`.
[Automated testing guidance](docs/testing.md#automated-testing) covers adding
cases, interpreting failures, prerequisites, and CI. GitHub Actions is configured
to run `make test` on branch pushes and pull requests; remote execution remains
to be verified after the workflow is pushed.

## Quickstart

```bash
./build/glove
```

Expect five bracketed, comma-separated ten-value snapshots, empty stderr, and
exit 0. Each line contains the left hand's five values followed by the right
hand's five values. Compare against the single maintained
[expected-output reference](docs/testing.md#canonical-output).

```bash
./build/glove mock
./build/glove --help
```

`mock` runs the same local demonstration. Help also accepts `-h`. Bare `primary`
runs locally; bare `secondary` and `client` are startup smoke tests, not network
listeners. See [CLI reference](docs/cli.md) for network configuration.

For complete terminal assignments, startup order, commands, expected output,
exit checks, and shutdown, use [manual verification](docs/testing.md). Run one
scenario at a time with fresh processes.

## Communication workflows

| Workflow | Data flow |
|---|---|
| Local simulation | Shared mock samples → local pair → output |
| Two-node TCP | Primary → TCP → Client |
| Three-node TCP | Secondary → TCP → Primary → TCP → Client |
| Three-node UDP | Secondary → UDP → Primary → UDP → Client |
| Two-node HTTP | Primary → HTTP combined submission → Client |
| TCP-to-HTTP relay | Secondary → TCP → Primary → HTTP → Client |
| UDP-to-HTTP relay | Secondary → UDP → Primary → HTTP → Client |
| Direct HTTP | Primary and Secondary → individual HTTP submissions → Client |

Primary and Secondary are communication roles, independent of Left and Right
physical hands. Both assignments are supported; combined output always remains
left then right. Client pairs direct HTTP submissions by sample index.

[Architecture](docs/architecture.md#supported-desktop-workflows) explains the
flows; [protocols](docs/protocols.md) explains transport and completion semantics.
Successful sender exit alone does not prove a complete downstream session.

## HTTP API

Glover Client hosts the application API:

```text
GET  /health
GET  /gloves
POST /gloves
```

GET `/gloves` supports `role=primary` and `role=secondary` filters. Primary and
Secondary are senders, not HTTP API hosts. GET inspects latest available state;
it is not an event queue. See [HTTP API reference](docs/api.md) for schemas,
validation, responses, submission modes, and session lifecycle.

## Documentation

| Document | Canonical purpose |
|---|---|
| [Architecture](docs/architecture.md) | Models, ownership, communication workflows |
| [HTTP API](docs/api.md) | Endpoints, JSON contracts, response codes, HTTP sessions |
| [CLI](docs/cli.md) | Commands, options, defaults, output and exit behavior |
| [Protocols](docs/protocols.md) | Text serialization, TCP/UDP framing and transport semantics |
| [Testing](docs/testing.md) | Automated tests, contributor guidance, manual verification and evidence |
| [Integration](docs/integration.md) | Team boundaries, dictionary, ESP32/recognition/keyboard decisions |

## Team integration

Repository team assignments identify Skyler with sensors, thresholds, and
calibration; Esteban with communication and a hardware adapter; Nikita with
recognition and chord timing; Eduardo with computer/keyboard output; and Wesley
with shared nodes, networking, and CLI infrastructure.

Start with [integration](docs/integration.md) for usable APIs, the documented
dictionary, the provisional decoded-text example, and decisions requiring team
agreement. It distinguishes established roles from proposed interface ownership.

When contributing, inspect the relevant source and canonical reference, keep
changes scoped, preserve existing contracts, and update affected documentation
and verification procedures together. Report what you actually tested. Leave
staging and commits for explicit authorization.

## Current limitations

- No integrated ESP32 acquisition, sensor calibration, or hardware adapter.
- No production continuous-streaming, synchronization, or recovery contract.
- No integrated chord recognition or keyboard event generation.
- No production device discovery, authentication, or persistent sample storage.
- Fixed five-sample simulations do not define a live hardware session.
- TCP has no application acknowledgment or session timeout; UDP has no delivery
  guarantee or retransmission; HTTP acceptance is in-memory only.

Open requirements are recorded in [integration](docs/integration.md).
Verification evidence and its limits are recorded in [testing](docs/testing.md#verification-matrix-and-evidence).

## Dependencies and licensing

HTTP uses vendored **cpp-httplib v0.66.0**; application JSON uses vendored
**nlohmann/json v3.12.0**. TLS and compression are disabled.

- [cpp-httplib provenance and update policy](third_party/cpp-httplib/README.md)
  and [MIT license](third_party/cpp-httplib/LICENSE).
- [nlohmann/json provenance and update policy](third_party/nlohmann/README.md)
  and [MIT license](third_party/nlohmann/LICENSE.MIT).

Dependency records preserve upstream attribution, versions, and hashes. Do not
confuse desktop dependency availability with ESP32 firmware support.
