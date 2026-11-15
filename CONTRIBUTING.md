# Contributing to Tappyweb

Start with an issue for a larger change. For bugs, include a small `.tappy` example, your OS, `tappy --version`, and the error you saw. Leave credentials and personal data out of reports.

The runtime, Python integration, asset generator and release packager are C++. Use a C++20 compiler with Make or CMake. Python 3.10+ is needed for the integration suite and Python-event examples, not to build the runtime.

Before opening a pull request:

```sh
make
TAPPY_NO_UPDATE=1 make test
```

Keep existing syntax and saved state compatible where possible. For behavior changes, add a focused HTTP integration test. For Python changes, check syntax-only validation, state rollback, interpreter selection and the process deadline.

Keep third-party notices with bundled code. Contributions are made under this repository's MIT license.

This description has been written with AI.
