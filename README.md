<p align="center">
  <img src="assets/logo.png" width="112" alt="Tappyweb logo">
</p>
<h1 align="center">Tappyweb</h1>
<p align="center">Build a web app in a file you can read.</p>
<p align="center">
  <a href="https://github.com/TappyWeb/TappyWeb/releases/latest"><img src="https://img.shields.io/github/v/release/TappyWeb/TappyWeb?label=release" alt="Latest release"></a>
  <a href="https://github.com/TappyWeb/TappyWeb/actions/workflows/native.yml"><img src="https://img.shields.io/github/actions/workflow/status/TappyWeb/TappyWeb/native.yml?branch=main&label=macOS%20%2F%20Linux" alt="macOS and Linux tests"></a>
  <a href="https://github.com/TappyWeb/TappyWeb/actions/workflows/windows.yml"><img src="https://img.shields.io/github/actions/workflow/status/TappyWeb/TappyWeb/windows.yml?branch=main&label=Windows" alt="Windows build and tests"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-green" alt="MIT license"></a>
  <a href="https://github.com/TappyWeb/TappyWeb/stargazers"><img src="https://img.shields.io/github/stars/TappyWeb/TappyWeb?style=flat&label=stars" alt="GitHub stars"></a>
</p>
<p align="center">
  <a href="https://github.com/TappyWeb/TappyWeb/releases/latest">Download</a> ·
  <a href="docs/language.md">Language guide</a> ·
  <a href="docs/python.md">Python</a> ·
  <a href="docs/themes.md">Themes</a> ·
  <a href="examples">Examples</a> ·
  <a href="https://github.com/TappyWeb/TappyWeb/issues">Report a bug</a>
</p>

Tappyweb puts pages, forms, components and state in a `.tappy` file. The **C++20 runtime** renders the HTML and handles actions on the server; HTMX updates the page. You can add Python to an event when you need a Python library or calculation.

```tappy
set completed to 16

set home to new page with name "Home":
    suffix: /
    heading "One step at a time."
    text "{completed} tasks completed"
    button "Finish a task":
        on click:
            set completed to completed + 1
```

Save it as `home.tappy` and run:

```sh
tappy home.tappy
```

Open **http://127.0.0.1:3000**. Save changes to update the running app. Matching state stays in memory; an invalid edit leaves the last working app running. Use `save` instead of `set` for state that should survive a restart.

## Download

Get the [latest release](https://github.com/TappyWeb/TappyWeb/releases/latest):

| Platform | Install | Also available |
| --- | --- | --- |
| macOS 13+, Apple Silicon and Intel | Universal `.pkg` | Portable `.zip` |
| Windows 10/11, x64 | NSIS `Setup.exe` | Portable `.zip` |
| Linux x64 / ARM64 | Debian / Ubuntu `.deb` | Portable `.tar.gz` |

See the [Linux guide](docs/linux.md) for CPU and library requirements and portable installation.

Installers add the `tappy` command and a `.tappy` file launcher. Existing OS default-app choices may take precedence. Releases are currently unsigned; Mac packages are not notarized.

No compiler or Python is needed for ordinary Tappy apps. **Apps with Python events need Python 3.10+ installed separately.** Python is not bundled with the runtime.

```sh
tappy --version
tappy update
```

The installed language checks for GitHub updates in the background at most daily. Updates are SHA256-verified and staged for the next command. Running servers keep their current version until restarted. Set `TAPPY_NO_UPDATE=1` to disable update checks and cached-runtime forwarding.

## Python when you need it

The integration is written in **C++**, with the small Python execution bridge embedded in the binary. There is no Rust runtime or separate Python web server.

```tappy
set total to 0

set home to new page with name "Calculator":
    suffix: /
    heading "Total: {total}"
    form:
        input "Price" as price required type number
        button "Add tax"
        on submit:
            python:
                from decimal import Decimal
                total = float(Decimal(price) * Decimal("1.20"))
```

State and form inputs are available by name. Changes to declared state return to Tappy after the block succeeds. You can also run a file with `python "./helper.py"`, import your own modules, and use packages from the app's `.venv`.

`tappy check` catches Python syntax errors without executing the code. Event errors roll back Tappy state. Each block has a 10-second deadline and a 1 MiB result limit. Python runs with your server user's permissions; file writes and network requests cannot be rolled back. Run trusted app code only.

See the [Python guide](docs/python.md) for interpreter selection, virtual environments, scripts and examples.

## Pages, components and themes

- **Named pages:** `set home to new page with name "Home":`, then `suffix: home` creates `/home`. Use `link "Home" home` or `open home` in an action.
- **Components:** define a `component` once, then `use` it on your pages. Parameters let each copy have its own content.
- **Forms and state:** validate inputs, change state, and save lists or values between restarts.
- **CSS themes:** `set theme to "./theme.css"` or use an HTTPS link. Local CSS reloads on save; remote CSS is downloaded at startup and cached.
- **HTMX:** included in the binary, with native attributes available for fragments and custom interactions.
- **Tabs or spaces:** use tabs or four spaces for Tappy indentation. Python blocks keep Python's own indentation inside the block.

[Read the language guide](docs/language.md) · [Browse examples](examples) · [Make a theme](docs/themes.md)

## Build from source

Use a C++20 compiler and Make, or CMake 3.20+. **Building and packaging the language do not require Python.** The asset generator and release packager are C++ tools. Python is used for integration tests and for apps that contain Python events.

```sh
git clone https://github.com/TappyWeb/TappyWeb.git
cd TappyWeb
make
./bin/tappy examples/dashboard.tappy
```

On Linux, use `make CXX=g++`. On macOS, install Apple's Command Line Tools if needed. For Windows, see the [Windows build guide](docs/windows.md).

```sh
cmake -S . -B build/native
cmake --build build/native --config Release
```

### Check and export

```sh
tappy check home.tappy
tappy build home.tappy --out dist
```

`build` exports static pages. Pages with event handlers need `serve` instead. Normal requests and actions stay on the local server; live reload uses one persistent change connection rather than repeated browser polling.

### Test and package

```sh
make test             # Python 3 for HTTP integration tests
make pkg              # macOS: universal PKG and update ZIP
```

For Windows, `tools/package_windows.ps1` builds with CMake and runs the native packager. NSIS 3 creates the installer. The packager can also package cross-compiled Windows binaries; see [docs/windows.md](docs/windows.md).

## Contributing

Bug reports, small fixes, examples and themes are welcome. Include a small `.tappy` file that reproduces a problem. See [CONTRIBUTING.md](CONTRIBUTING.md) for checks and pull requests.

If you find Tappyweb useful, [give the repo a star](https://github.com/TappyWeb/TappyWeb/stargazers). It helps other people find it.

## License and credits

Tappyweb is available under the [MIT license](LICENSE). Bundled third-party code keeps its own license:

| Dependency | License | Used for |
| --- | --- | --- |
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | [MIT](vendor/httplib.LICENSE) | Local HTTP server |
| [HTMX](https://github.com/bigskysoftware/htmx) | [0BSD](assets/htmx.LICENSE) | Browser updates |
| [nlohmann/json](https://github.com/nlohmann/json) | [MIT](vendor/json.LICENSE) | Python state exchange and release metadata |

Python support was brought over from the Tappyweb language prototype while keeping the public runtime in C++. The original dependency notices are included in release packages.

This description has been written with AI.
