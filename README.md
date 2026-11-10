<p align="center">
  <img src="assets/logo.png" width="112" alt="Tappyweb logo">
</p>

<h1 align="center">Tappyweb</h1>
<p align="center"><strong>A whole web app. One readable file.</strong></p>
<p align="center">Pages, forms, reusable components and server-side state — powered by C++ and HTMX.</p>

```tappy
save tasks as []

page "My tasks" /
    heading "Make something happen."

    form:
        input "Your next task" as draft required
        button "Add task"
        on submit:
            add draft to tasks
            clear draft

    for each task in tasks:
        row:
            text task
            button "Done":
                on click:
                    remove task from tasks
```

**That is a working task app.** Tappyweb renders the HTML, handles submissions on the server, updates the page through HTMX, and saves the tasks between restarts.

## From file to localhost

```sh
git clone https://github.com/TappyWeb/tappy-web.git
cd tappy-web
make
./bin/tappy examples/tasks.tappy
```

Open **http://127.0.0.1:3000**. Edit the file and save. The running app reloads automatically, keeping matching state values. Invalid edits leave the last working version running and show a filename/line diagnostic.

Building requires a **C++20 compiler, Make and Python 3**. On macOS, install Apple's Command Line Tools with `xcode-select --install`. Python is used for build tooling and tests; the compiled app runs as a single executable. macOS is tested; Linux can be built with a suitable toolchain (`make CXX=g++`), and Windows has a native CMake build.

## Small syntax. Real interactions.

- **Pages and navigation.** Declare routes and link them together.
- **Forms and actions.** Read named inputs, validate submissions and update server state.
- **Reusable components.** Pass values into components and compose a UI.
- **Memory or saved state.** Use `set` for a running app and `save` for values that survive a restart.
- **HTMX included.** Forms, buttons and internal navigation generate HTMX attributes. Use native core attributes for polling and fragments.
- **Live reload.** Save a valid change to update the app without restarting its server.
- **Tabs welcome.** Indent with a tab or four spaces per level.
- **One native runtime.** C++20, embedded HTMX and embedded CSS. Build and run offline with the vendored dependencies.

```tappy
set completed to 12
set goal to 21

component Metric(label, value):
    card:
        subheading label
        heading value

page "Overview" /
    grid:
        use Metric("Completed", completed)
        use Metric("Goal", goal)

    button "Complete one":
        on click:
            set completed to completed + 1
```

Arithmetic supports normal operator precedence, parentheses and unary signs. Text supports `{variable}` interpolation. Inline comments use `#` outside quoted text.

## Three commands to keep close

```sh
./bin/tappy check app.tappy                  # Check source and show diagnostics
./bin/tappy app.tappy --port 3001            # Run on localhost
./bin/tappy build app.tappy --out dist       # Export a static site
```

`serve app.tappy` also works. Static export is for pages without server actions; interactive apps run through the native server. Serve exported files over HTTP from the origin root.

## Explore the examples

| Example | What you can build |
| --- | --- |
| [Task app](examples/tasks.tappy) | A saved task list with forms and deletion |
| [Dashboard](examples/dashboard.tappy) | Metrics, conditions and server-side updates |
| [Components](examples/metrics.tappy) | Parameterized components with tab indentation |
| [Feedback form](examples/feedback.tappy) | Required fields, email validation and saved feedback |
| [Landing page](examples/landing.tappy) | A static site you can export |
| [Live fragments](examples/fragments.tappy) | HTMX polling and partial HTML responses |

[Read the language reference](docs/language.md) · [Understand the runtime](docs/architecture.md)

## Package it for macOS

```sh
make pkg
```

This builds `releases/Tappyweb-0.3.0-macOS-universal.pkg` for Apple Silicon and Intel, targeting macOS 13+. It installs `tappy` in `/usr/local/bin` and examples in `/usr/local/share/tappyweb`.

After installation:

```sh
tappy app.tappy
```

The recipient runs the compiled executable without a compiler, Python or a separate HTMX installation. Packages built by this repository are unsigned and not notarized; public distribution through Apple's normal trust flow requires Developer ID signing and notarization. Intel builds are compiled here; execution has been tested on Apple Silicon.

## Current scope

Tappyweb **0.3 is an early language MVP**, well suited to local prototypes and small interactive tools. Syntax may evolve. Legacy `state`, `remember` and `nav` declarations remain supported.

State is shared across browser sessions. Saved values live beside the source in `.tappy-data/`; they are local app storage, not user accounts or a concurrent database. The server binds to `127.0.0.1`. Keep one server process per source/data file.

Lists currently contain strings. Authentication, objects, imports, custom functions, route parameters, database connectors and outbound API clients are future work. The bundled HTMX core is 2.0.8; JavaScript evaluation, inline scripts and extensions are disabled. The HTTP server uses vendored cpp-httplib 0.28.0. Third-party licenses are included with their sources.

## Develop

```sh
make test
```

The integration suite exercises the real executable and HTTP server: forms, saved state, escaping, CSRF checks, stale actions, atomic rollback, live reload, component parameters, arithmetic and indentation.

Tappyweb Studio is developed separately. This repository contains the language, runtime, examples and CLI packaging.


## Windows

Use the native Windows x64 package to run `tappy file.tappy` from Windows Terminal. The NSIS Setup.exe installs the command and its user PATH entry. A portable ZIP is also available. See the [Windows guide](docs/windows.md) for installation, native builds and cross-compilation. Windows builds are cross-compiled on macOS; native CI and Windows UI testing must run separately.


The macOS PKG installs `/usr/local/bin/tappy` and `/etc/paths.d/tappyweb`. After installation, open a new Terminal window and run `tappy file.tappy`.

## Named pages, fewer requests

```tappy
set home to new page with name "Home":
    suffix: home
    heading "Welcome"
    link "More details" details

set details to new page with name "Details":
    suffix: details
    link "Back home" home
```

Page names are reusable route references: `home` resolves to `/home` on the current site. The Library in Mac Studio uses this syntax throughout. Legacy pages remain valid.

Live reload now uses server-sent notifications instead of polling the server twice per second. Installed runtimes can stage SHA256-verified updates from public GitHub releases, at most daily, and apply them on the next command. `tappy update` checks immediately; `TAPPY_NO_UPDATE=1` disables this behavior. Double-click `.tappy` files after installing to launch them in your terminal. See [the language reference](docs/language.md) for details.
