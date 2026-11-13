# nlohmann/json

Pinned upstream **v3.12.0**, header-only C++ JSON parser (MIT).
Only the application JSON adapter includes this dependency.

- Header: https://raw.githubusercontent.com/nlohmann/json/v3.12.0/single_include/nlohmann/json.hpp
- License: https://raw.githubusercontent.com/nlohmann/json/v3.12.0/LICENSE.MIT

Unmodified upstream files, SHA-256:

```text
aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63  json.hpp
46a65cffd1ea955132d95a8dd921640714a8d6b537d2e4e482d31145ae95b603  LICENSE.MIT
```

No existing dependency supplied JSON parsing. The adapter uses the parser's
callback to reject duplicate keys before object construction loses them, and
rejects nesting beyond this API's shallow schema. Updates must preserve both
license and provenance and recheck strict validation.
