# cpp-httplib

Vendored upstream release: **v0.66.0** (MIT license; see `LICENSE`).

- Release: https://github.com/yhirose/cpp-httplib/releases/tag/v0.66.0
- Header: https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.66.0/httplib.h
- License: https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.66.0/LICENSE

Both upstream files are unmodified. SHA-256:

```text
51ddd1482c67acb31f3064e48329aade14acc7dd0fab31ee5397193697ea1c35  httplib.h
4b45cbe16d7b71b89ae6127e26e0d90a029198ca5e958ad8e3d0b8bbed364d8b  LICENSE
```

The Makefile supplies the include path and `-pthread`. Both generic HTTP client
and server use this pinned dependency. Optional TLS and compression are disabled.
Glove JSON parsing is separate application code using the dependency documented
in [nlohmann/json](../nlohmann/README.md). Dependency updates should replace the
upstream header and license together and refresh the version and hashes here.
