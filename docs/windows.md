# Tappyweb for Windows

The Windows x64 package contains the native C++ `tappy.exe`, examples, documentation and dependency licenses. It targets Windows 10/11 x64 and requires no separately installed compiler, Python, .NET or Node.js to run.

## NSIS installer

Run `Tappyweb-0.2.0-Windows-x64-Setup.exe`. It installs the CLI for the current user and adds its directory to your user PATH by default. Open a new Terminal window, then run `tappy file.tappy`. Installation and uninstallation appear under Windows Installed apps; the uninstaller removes the PATH entry only if this installer added it. Custom logo sideart appears on welcome and finish pages.

## Run without installing

Extract the ZIP first. Open PowerShell or Windows Terminal in the extracted folder:

```powershell
.\tappy.exe .\examples\tasks.tappy
```

Visit http://127.0.0.1:3000. Save edits to reload. Ctrl+C stops the server.

## Install the command

Run the included `install.ps1` from PowerShell:

```powershell
.\install.ps1
```

This copies the package to `%LOCALAPPDATA%\Programs\Tappyweb` and adds that folder to your user PATH. Open a **new terminal** afterward:

```powershell
tappy file.tappy
tappy check file.tappy
tappy file.tappy --port 3001
```

Installation needs no administrator account. If your organization's script policy blocks installation, the portable executable above still works. Copy examples to a project folder before editing them; reinstalling replaces the packaged examples.

## Build on Windows

Install Visual Studio 2022 Build Tools with Desktop development with C++, CMake, Python 3 and NSIS 3. From the repository root:

```powershell
.\tools\package_windows.ps1
$env:PYTHONUTF8 = '1'
$env:TAPPY_TEST_BINARY = "$PWD/build/windows-x64/Release/tappy.exe"
python tests/test_cli.py
```

The ZIP and NSIS Setup.exe are written under `releases/`. The GitHub Actions Windows workflow builds this package and runs the integration tests against the actual Windows executable.

## Cross-build from macOS

Use the official [LLVM-MinGW toolchain](https://github.com/mstorsjo/llvm-mingw/releases), including its macOS host package:

```sh
python3 tools/embed.py
/path/to/llvm-mingw/bin/x86_64-w64-mingw32-clang++ \
  -std=c++20 -O2 -static -municode -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 \
  -Ivendor src/main.cpp -lws2_32 -o /tmp/tappy.exe
python3 tools/package_windows.py --binary /tmp/tappy.exe \
  --toolchain-license /path/to/llvm-mingw/LICENSE.TXT
```

Cross-compilation verifies that a Windows binary can be built. Native execution and Windows integration tests still need Windows. The generated executables and packages are unsigned.
