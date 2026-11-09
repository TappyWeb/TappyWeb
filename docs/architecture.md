# Implementation notes

## Pipeline

`source → indentation parser → AST with source lines → semantic validation → HTML renderer → HTMX browser → C++ action executor → HTML fragment`

The native CLI parses and validates once at startup. AST nodes retain source line numbers for errors. This MVP interprets action ASTs in C++; it does not translate a Tappyweb program into native machine code. The runtime itself is compiled with C++20 and `-O2`. No JVM, Node server or Python process is required at runtime.

`src/main.cpp` contains the lexer/parser, typed values and expression evaluator, renderer, semantic checker, action executor, persistence layer and CLI/HTTP server. `assets/default.css` supplies default presentation. `tools/embed.py` generates `src/assets.hpp` from that stylesheet and the pinned HTMX distribution. Make rebuilds the binary when source, dependencies or assets change.

## Requests

A normal GET returns a full document. A GET with `HX-Request: true` returns a replacement `<main id="app">` for page navigation. History-restore requests receive a full document. Read-only fragment routes always return just their rendered HTML.

Each rendered action receives an unpredictable token stored server-side with its AST event, route, input rules, loop locals and shared-state revision. A POST to `/_tappy/action/<token>` checks the browser session and CSRF token, validates inputs, executes the event, renders the updated page, then saves persistent variables. Successful actions replace `#app` through HTMX. `open` also sets `HX-Push-Url`. Errors restore state and render a friendly error on the original page.

The browser executes the actual HTMX core; there is no custom frontend router or homegrown imitation of its request/swap behavior. `allowEval: false` and `allowScriptTags: false` disable evaluation. Requesting the generated GET/POST URLs with a raw HTTP client exercises the same server path as the browser.

## State and storage

Types are string, number, boolean and list-of-strings. Top-level declarations initialize shared state. Loop values and form fields are local to an action and cannot replace global declarations.

`remember` values are stored in a versioned, length-limited text format using quoted string encoding. Saves write a temporary file and atomically rename it in the same directory. Save failures roll back in-memory mutations. This provides atomic updates, not power-loss durability guarantees or database transactions across multiple processes. Existing saved types must match the source declaration.

One mutex serializes state-dependent GET and POST handling. This intentionally simple design provides consistent local-app semantics; it is not tuned for high concurrency. Browser action tokens capture the current state revision; actions from an old revision refresh without mutating, preventing stale list indices deleting other items. Already-rendered pages in other browsers do not automatically update unless refreshed or given HTMX polling.

Server sessions live in memory. Cookies are HttpOnly and SameSite=Strict. The server is HTTP on 127.0.0.1, so it does not mark cookies Secure. Session count and pending action count are bounded; reaching a limit can expire an older session/page. Restarting the server expires sessions and action tokens.

## Scope and constraints

No authentication, private user state, production deployment, database connectors, external API requests, user-defined functions, native code generation, editor, MCP or hosting management. The single-process runtime is a local MVP. Performance is not benchmarked against other languages or frameworks.

Static export renders initial values and rejects actions before writing any output. It emits directory-index pages and local HTMX/CSS assets. Use an HTTP server at the site root. Exporting to an existing directory overwrites matching generated files but does not remove stale files from previous exports; use a fresh output directory for a clean export.

## Dependencies

- [cpp-httplib 0.28.0](https://github.com/yhirose/cpp-httplib/tree/v0.28.0), MIT: HTTP server and form decoding. Header and license under `vendor/`.
- [HTMX 2.0.8](https://github.com/bigskysoftware/htmx/tree/v2.0.8), Zero-Clause BSD: browser requests, navigation and HTML swaps. Distribution and license under `assets/`.

Both are vendored from upstream tagged releases. Updating them is a deliberate source change; no package manager runs during compilation. Keep their notices when redistributing the bundled runtime.

## Verification

`make test` runs Python-standard-library integration tests against the compiled CLI and real temporary localhost servers. It checks parsing diagnostics (including inactive branches), all example files, static builds, escaping, CSRF rejection, forms, HTMX navigation, redirects, fragments, persistence across restart, replay prevention, stale shared-state actions and rollback on runtime failure.

A real-browser interaction check is separate from HTTP integration tests. In this environment, browser automation was unavailable and Chrome access was not approved, so browser click/visual behavior has not been verified.

## Source reloading

Serving installs a pre-routing hook which compares the source contents on each request, including a lightweight browser poll every 500 ms. This follows editor atomic-save renames without platform-specific filesystem dependencies or a background watcher thread. With no browser or HTTP requests, recompilation waits until the next request.

A changed source is parsed and validated as a candidate engine. Same-name/same-type live values are copied, every route is rendered with those values, and persistent state is saved before swapping engines under the state mutex. Errors keep the old engine and print diagnostics once per changed source contents. Missing/unreadable files keep the running app available. A successful swap clears all action tokens before destroying the old AST, and changes the source version token. Poll requests for older versions receive `HX-Refresh: true`, handled by the bundled HTMX library. Static exports do not include this hook.
