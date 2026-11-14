# Glover HTTP API

This is the application contract for the HTTP server hosted by the Glover
**Client node**. Primary and Secondary send states; neither hosts this API.
The generic HTTP transport supplies requests, responses, and routing without
owning glove models. See [architecture](architecture.md) for ownership,
[protocols](protocols.md) for transport behavior, and [CLI](cli.md) for launch
options.

The current API handles one fixed five-sample desktop session per process run.
It submits and inspects glove states, not live hardware, recognized chords, or
keyboard output. JSON object key order is not contractual.

## Endpoints

| Method | Target | Purpose |
|---|---|---|
| GET | `/health` | Service health |
| GET | `/gloves` | Highest-index complete combined state |
| GET | `/gloves?role=primary` | Highest-index available Primary state |
| GET | `/gloves?role=secondary` | Highest-index available Secondary state |
| POST | `/gloves` | Submit an individual or combined sample |

The role variants use the same `/gloves` GET route. There are no per-role paths,
index lookup paths, or additional registration, calibration, history, or CRUD
operations.

## GET /health

Returns **200**, `Content-Type: application/json`, with this body:

```json
{"status":"ok"}
```

Availability does not depend on samples. This indicates the service can answer,
not that gloves are connected or a session is complete. Any query component,
including a trailing `?`, is rejected with 400.

## GET /gloves

Returns **204 with an empty body** if no complete pair is available. Otherwise
returns **200**, `application/json`, with exactly these fields:

```json
{
  "index": 4,
  "left": [1, 1, 1, 1, 1],
  "right": [1, 1, 1, 1, 1]
}
```

`index` is an integer 0–4. `left` and `right` each contain five integer binary
values in [physical-hand storage order](architecture.md#finger-ordering).
Combined state is always left then right. This GET representation omits the
`role` and `hand` fields required by a combined POST.

“Latest” means the highest available complete index, not the last request
received. Index 4 may be inspectable before lower indices arrive. One available
pair does not mean the five-sample session has completed. GET does not consume
data and is not a queue, history API, or guaranteed delivery mechanism.

## Role-filtered GET

`GET /gloves?role=primary` and `GET /gloves?role=secondary` return **204** with no
body if the requested role has no accepted state. Otherwise they return **200**,
`application/json`, with exactly:

```json
{
  "role": "primary",
  "hand": "right",
  "index": 4,
  "fingers": [1, 1, 1, 1, 1]
}
```

The role selects communication identity; `hand` reports its physical hand.
Primary may be left or right. Each role selects its own highest available
index, so role GETs can report different indices. Individual state becomes
available without waiting for its opposite-hand partner. Combined submissions
populate both role views as well as the combined view.

Query parsing requires exactly one `role=primary` or `role=secondary` filter
when `?` is present. Names and values are case-sensitive. Unknown keys, unknown
or empty roles, an empty query, repeated/additional parameters, and malformed
percent escapes produce 400. Valid percent-encoded keys and values are decoded;
`+` becomes a space and does not match either allowed role value. There is no
`index` filter. POST `/gloves` rejects any query component with 400.

## POST /gloves

Send JSON with an accepted content type described below. The Client startup
mode determines which valid payload shape can be accepted. **204 with an empty
body** means the submission was stored in the current process's session.

### Individual request

```json
{
  "role": "secondary",
  "hand": "left",
  "index": 0,
  "fingers": [1, 0, 0, 0, 0]
}
```

| Required field | JSON type and allowed values |
|---|---|
| `role` | String: `primary` or `secondary` |
| `hand` | String: `left` or `right` |
| `index` | Integer from 0 through 4 |
| `fingers` | Array of exactly five integers, each 0 or 1 |

There are no optional fields. Primary and Secondary each send their own hand.
In individual mode, the two roles may arrive in either order or concurrently,
and sample indices may arrive out of order. Pairing requires equal indices.

### Combined request

```json
{
  "role": "primary",
  "hand": "right",
  "index": 0,
  "left": [1, 0, 0, 0, 0],
  "right": [0, 0, 0, 0, 1]
}
```

| Required field | JSON type and allowed values |
|---|---|
| `role` | String: `primary` only |
| `hand` | String: `left` or `right`, identifying Primary's hand |
| `index` | Integer from 0 through 4 |
| `left` | Exactly five integer binary values for the physical left hand |
| `right` | Exactly five integer binary values for the physical right hand |

There are no optional fields. Primary supplies both hand arrays, either from
the fixtures or after receiving Secondary over TCP/UDP. Setting `hand` to
`left` does not exchange the arrays. The other physical hand is attributed to
Secondary. Role is validated metadata, not authenticated sender identity.

### JSON validation

The JSON codec rejects malformed JSON, non-object roots, missing/unknown fields,
duplicate keys, and mixed individual/combined shapes with **400**. Excessive
nesting is rejected. Field names and enum strings are case-sensitive.

`index` and array elements must be JSON integers. Boolean values, floats such
as `1.0`, exponent-form floating numbers, strings, and null are rejected even
when they resemble valid numbers. Finger integers must be 0 or 1; each array
must have exactly five elements. Index must be 0–4. A combined request with
Secondary role is a schema error (400), not a mode conflict.

The codec validates syntax, fields, types, and ranges. Client separately
validates session availability, configured mode, role/hand consistency, and
duplicate slots. Submitted finger values need not equal the mock fixtures.

## Modes, mapping, and state effects

| Client mode | Senders and accepted shape | Completion |
|---|---|---|
| `combined` (default) | Primary submits both hands, directly or through a TCP/UDP relay | One accepted combined request at every index 0–4 |
| `individual` | Primary and Secondary independently submit their own hands | Both roles accepted at every index 0–4 |

A structurally valid request of the other shape returns **409**. Mode is chosen
at process startup and cannot be changed through an endpoint.

The first accepted request establishes Primary's physical hand. For a Secondary
request, Primary is inferred to be the opposite hand. All subsequent requests
must preserve that complementary mapping. Both Primary-right/Secondary-left
and Primary-left/Secondary-right are supported. A conflicting mapping returns
409 without replacing accepted sample data.

In individual mode, a repeated role/index is a duplicate. In combined mode, a
repeated index is a duplicate. Identical repeats also return 409; this is not
an idempotent update API. Validation conflicts do not overwrite existing data.
The second valid hand at an index creates a complete pair, left then right.
Each accepted request advances applicable highest-index trackers and checks
whether all five slots are complete.

## Content types

POST accepts `application/json`, optionally with exactly one UTF-8 charset:

```text
application/json
application/json; charset=utf-8
application/json; charset="utf-8"
```

Matching is ASCII case-insensitive for media type, parameter name, and charset.
Spaces/tabs around the media type, parameter, and `=` are tolerated. Unknown
parameters, repeated parameters, non-UTF-8 charsets, a bare trailing semicolon,
missing content type, and other media types fail with **415**. Multipart
requests are rejected by the HTTP wrapper.

Successful nonempty API GETs use `application/json`. Application/wrapper errors
use `text/plain`, not a JSON error envelope. A 204 has no response body; do not
attempt JSON parsing. The handler's empty-response media-type default is
`text/plain`, but the useful contract for 204 is status and absence of content.

## Status and error reference

| Status | Source and meaning |
|---|---|
| 200 | Health or available state GET |
| 204 | Accepted POST, or unavailable requested GET state; empty body |
| 400 | Invalid JSON/schema, invalid role query, or a query on health/POST |
| 404 | Unknown route; wrapper body `Not Found` |
| 405 | Unsupported method at a known route; body `Method Not Allowed`; `Allow: GET` for health or `GET, POST` for gloves |
| 409 | Session closed, mode conflict, role/hand conflict, or duplicate |
| 413 | HTTP library rejects a body exceeding its configured size limit |
| 415 | Unsupported POST content type or multipart body |
| 500 | Unhandled handler exception; wrapper body `Internal Server Error` |

HEAD is not a supported application method. Transport parsing and resource
limits can reject a request before an application handler runs; those library
responses are not glove-schema error contracts. A failed connection or read
can also produce a client exception without a complete HTTP response.

Session conflict bodies are `Session is closed`, `Submission mode conflict`,
`Role-to-hand mapping conflict`, and `Duplicate sample`. Validation errors use
the codec's plain-text diagnostic, for example `Missing, unknown or mixed fields`.
No structured error object is implemented.

For a routed POST, query rejection precedes content-type checking; JSON parsing
precedes session conflict checks. A request violating multiple rules therefore
need not return the status of a later check. Ordinary rejected requests do not
complete or reset the session. An unexpected POST handler exception marks the
session failed before the server returns 500.

## Session lifecycle and concurrency

1. Client constructs an empty five-slot session, starts its total receive
   deadline, registers handlers, and runs the server on a worker thread.
2. Once listening, it reports `HTTP Client listening on ADDRESS:PORT` to stderr.
   Bind/listener failure exits 1.
3. Accepted submissions populate in-memory slots. GET inspection never consumes
   data or resets the deadline.
4. When all five indices have complete pairs, Client prints the complete batch
   once, in index order, using the canonical text serializer. It prints no
   incomplete batch.
5. Default `--stay-open true` disables the receive deadline after completion and
   permits inspection until Ctrl-C/SIGTERM. Successful completed shutdown exits 0.
6. `--stay-open false` stops after completion, drains active responses, joins
   workers, and exits 0. Start a fresh process for another batch.

The default deadline is **30000 ms from HTTP execution startup**, not from the
first POST and not per request. Missing indices, timeout, or stopping before
completion fail with exit 1 and missing-index diagnostics. GETs, rejected
requests, and individual acceptance without a complete partner do not extend
the budget. CLI details are in [cli.md](cli.md).

Handlers can run concurrently. A session mutex protects slots, mapping,
duplicate checks, trackers, and completion flags. GET copies a consistent
snapshot under that lock, then serializes it. Separate GET calls are not a
transaction across role views. No throughput guarantee or benchmark is implied.

A POST 204 confirms in-memory acceptance only. One individual acknowledgment
does not confirm the opposite hand, all five pairs, durable storage, recognition,
or keyboard delivery. A lost acknowledgment can follow successful acceptance;
blind replay can conflict. Downstream completion does not establish upstream
relay success: a TCP relay still requires clean EOF after its fifth sample.

## Request examples

These are short contract examples, not complete five-sample verification runs.
Run commands from the repository root. In one terminal, start a fresh server:

```bash
./build/glove client --transport http --bind 127.0.0.1 --port 9000 --submission combined --timeout-ms 120000 --stay-open true
```

After its listening message, use another terminal:

```bash
# 200: health JSON
./build/glove http get --host 127.0.0.1 --port 9000 --path /health
# 204: initially empty
./build/glove http get --host 127.0.0.1 --port 9000 --path /gloves
# 204: accept combined index 0
./build/glove http post --host 127.0.0.1 --port 9000 --path /gloves --content-type application/json --body '{"role":"primary","hand":"right","index":0,"left":[1,0,0,0,0],"right":[0,0,0,0,1]}'
# 200: index 0, left/right arrays; no role/hand fields
./build/glove http get --host 127.0.0.1 --port 9000 --path /gloves
# 200: primary/right and secondary/left, both index 0
./build/glove http get --host 127.0.0.1 --port 9000 --path '/gloves?role=primary'
./build/glove http get --host 127.0.0.1 --port 9000 --path '/gloves?role=secondary'
# 400: invalid schema, plain-text diagnostic
./build/glove http post --host 127.0.0.1 --port 9000 --path /gloves --content-type application/json --body '{}'
```

For the individual example, stop the first server and start a fresh one in
individual mode. Stopping these deliberately incomplete sessions exits 1.

```bash
./build/glove client --transport http --bind 127.0.0.1 --port 9000 --submission individual --timeout-ms 120000 --stay-open true
```

In the request terminal after readiness:

```bash
# 204: Secondary accepted without Primary
./build/glove http post --host 127.0.0.1 --port 9000 --path /gloves --content-type application/json --body '{"role":"secondary","hand":"left","index":0,"fingers":[1,0,0,0,0]}'
# 200: Secondary index 0
./build/glove http get --host 127.0.0.1 --port 9000 --path '/gloves?role=secondary'
# 204: no complete pair yet
./build/glove http get --host 127.0.0.1 --port 9000 --path /gloves
```

The generic CLI writes `HTTP <status>` plus newline to stderr and body bytes
unchanged to stdout, without adding a newline. All complete responses above,
including 400, make that request process exit 0. Its exit code is not the HTTP
application status. These examples submit only index 0 and print no server
snapshot batch; full workflows are maintained in [testing](testing.md).

## Limits and sources

The API has no database, historical retrieval, authentication, registration,
calibration, recognition, keyboard delivery, or guaranteed event stream. Fixed
indices do not define production ESP32 streaming. Plain HTTP, body limits,
I/O timeouts, and retry behavior are documented in
[protocols](protocols.md#generic-http-transport-boundary).

Source authority: [Client handlers/session](../src/nodes/client.cpp),
[JSON codec](../src/codec/glove_json.cpp), [codec declarations](../include/codec/glove_json.hpp),
[HTTP server](../src/transport/http/server.cpp), and
[node defaults](../include/nodes/network.hpp). [Testing](testing.md) covers procedures and
[integration](integration.md) records unfinished team interfaces.
