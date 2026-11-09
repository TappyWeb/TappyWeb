# Tappyweb

A working language MVP with a native C++20 CLI and runtime. Write readable `.tappy` files; Tappyweb parses and checks them, generates HTML/CSS and HTMX attributes, and runs server actions without application JavaScript.

```tappy
remember tasks as []

page "My tasks" /
    heading "My tasks"
    form:
        input "New task" as draft required
        button "Add task"
        on submit:
            add draft.value to tasks

    for each task in tasks:
        row:
            text task
            button "Done":
                on click:
                    remove task from tasks
```

## Run it

The binary is already built at `bin/tappy`. From this folder:

```sh
./bin/tappy serve examples/tasks.tappy
```

Open **http://127.0.0.1:3000**. Stop with Ctrl+C. To rebuild or run the integration tests:

```sh
make
make test
```

Build prerequisites: a C++20 compiler, Make and Python 3 (only for embedding assets and running tests). These are already available on this machine. On macOS, Apple Command Line Tools provide Clang and Make. On Linux, use a C++20 compiler such as Clang or GCC (`make CXX=g++`). Windows can use WSL. Only macOS has been verified in this workspace.

HTMX 2.0.8 and cpp-httplib 0.28.0 are vendored with their licenses. No npm packages, Python packages, Homebrew installation, CDN, or network connection are needed to build and run. The binary embeds both HTMX and the default stylesheet, so it can run from another directory without finding external assets.

## CLI

```sh
./bin/tappy check examples/tasks.tappy
./bin/tappy serve examples/tasks.tappy --port 3001
./bin/tappy build examples/landing.tappy --out dist
./bin/tappy --help
```

`check` validates the grammar, variable references, components, UI and actions, including false branches and empty loops. Diagnostics include the source filename and line number. Checks do not load or modify saved data.

`serve` parses once at startup and serves routes, fragments and action requests. Source edits reload automatically; connected browsers refresh after a valid change. It binds to localhost only.

`build` exports static HTML and bundled assets for an HTTP server with directory-index routing. Server actions cause an explicit build error; a static folder cannot execute C++ state mutations. Static exports use the declared initial state. Serve the exported folder at the origin root, rather than opening it with `file://`.

## Examples

| File | Shows |
| --- | --- |
| [tasks.tappy](examples/tasks.tappy) | Multiple routes, components, forms, loops, saved tasks and deletion |
| [feedback.tappy](examples/feedback.tappy) | Required fields, email validation and persistent feedback |
| [dashboard.tappy](examples/dashboard.tappy) | Numeric state, conditions and arithmetic |
| [landing.tappy](examples/landing.tappy) | A fully static, exportable site |
| [fragments.tappy](examples/fragments.tappy) | Native HTMX attributes, read-only fragments and polling |

Read the [language reference](docs/language.md) and [implementation notes](docs/architecture.md).

## MVP boundaries

This is a provisional grammar implementing the brief's strongest preferences: `page "Name" /route`, four-space indentation, quoted text, short commands, no braces, semicolons or arrows. It is a language/runtime MVP, not a complete general-purpose programming language.

State is **shared by the whole running app**, not private per user. `remember` writes to `examples/.tappy-data/tasks.tappy.state` for the task example. Move or back up this file before starting the server if you want a fresh notebook. `state` resets on restart. Run one server process per source/data file; file storage is not a concurrent multi-process database.

MVP lists contain strings. There are no objects, parameterized components, custom functions, route parameters, imports, SQL/database connectors, authentication, outbound API clients, arbitrary JavaScript, or asset directory server. Image URLs may use HTTP(S); bundled local images are not yet supported. Numeric inputs currently provide text values, matching HTML form behavior.

The full HTMX core library is bundled; generated interactions use it directly. The `htmx` command exposes native core attributes for advanced behavior. JavaScript evaluation, inline event scripts and HTMX extensions are excluded. This does not promise implementations of arbitrary backend endpoints: native requests must target routes/fragments the runtime provides.

No studio, AI/MCP integration, publishing, VPS management, accounts, or hosted services are included. This local MVP has input limits, escaping, CSRF checks and atomic state updates, but is not a production hosting or authentication platform. C++ gives a native runtime; no claim of being the fastest implementation is made without comparative benchmarks.

## macOS installer

Build a universal installer for Apple Silicon and Intel Macs running macOS 13 or newer:

```sh
make pkg
```

The result is `releases/Tappyweb-0.1.1-macOS-universal.pkg`. Send that file to users. It installs the self-contained executable at `/usr/local/bin/tappy` and examples/documentation at `/usr/local/share/tappyweb`. Recipients need no compiler, Python, npm or separate HTMX installation. The package does not start a server or install a background service.

After installing, open Terminal in a folder containing your source:

```sh
tappy website.tappy
tappy website.tappy --port 3001
```

The first command hosts at **http://127.0.0.1:3000** until Ctrl+C. Existing `tappy serve`, `tappy check` and `tappy build` commands still work. The command and source extension remain `tappy` and `.tappy`.

To try the installed example without writing into the system examples folder:

```sh
mkdir -p ~/TappyProjects
cp /usr/local/share/tappyweb/examples/tasks.tappy ~/TappyProjects/tasks.tappy
cd ~/TappyProjects
tappy tasks.tappy
```

On a customized shell whose PATH omits `/usr/local/bin`, run `/usr/local/bin/tappy` directly or add that directory to your PATH.

The generated installer is **unsigned and not notarized**. The universal binary is ad-hoc signed, which is not a Developer ID signature. For public distribution with the normal macOS trust flow, sign the executable with a Developer ID Application identity, sign the package with a Developer ID Installer identity, then notarize and staple it using your Apple Developer account. The package builder does not use credentials or perform those distribution steps. Intel compatibility is compiled into the package but has not been executed on an Intel Mac here.

## Automatic reload

`tappy file.tappy` and `tappy serve file.tappy` pick up saved edits automatically. Open browser pages poll every 500 ms and refresh when a valid source change is accepted. No server restart is needed. Manual refresh and normal requests also pick up changes. Static exports contain no reload polling.

Invalid edits print a filename/line diagnostic in the terminal and leave the last working app online. Save a correction to recover. Existing state values survive when their variable names and types still match. New variables use their declared defaults; a changed type resets to its new default. Editing an existing variable's initial value does not reset its live value—restart for memory-only state, or manage the saved data file for persistent state.

Reloading invalidates old form/action tokens so they cannot reference replaced code. Open forms may lose unsent input when the browser refreshes. Changes to the C++ runtime or embedded assets still require rebuilding and restarting. Reinstall a freshly built package to update an older installed CLI.
