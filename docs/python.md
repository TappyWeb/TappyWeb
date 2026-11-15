# Python in Tappyweb

The runtime and process integration are C++. Install Python 3.10+ only for apps that use Python events; the interpreter is not bundled.

Use `python:` inside `on click` or `on submit` and write normal Python underneath. No JSON plumbing, decorators or extra server are needed. The process bridge is part of the C++ runtime.

```tappy
set total to 0
set message to "Enter a price"
save history as []

page "Calculator" /
    text message
    text total
    form:
        input "Price" as price type number required
        button "Calculate"
        on submit:
            python:
                from decimal import Decimal

                amount = Decimal(price) * Decimal("1.20")
                total = float(amount)
                message = f"Including tax: {amount:.2f}"
                history.append(message)
```

Form inputs are strings, even numeric inputs. Use `int(price)`, `float(price)` or a library to convert them. Inside Python, use `name` for an input rather than Tappy's `name.value`. Loop items and component parameters captured by an event are also available by name. Tappy dotted metadata such as `item.index` is available through `globals()["item.index"]`.

Declared state maps to Python `str`, `float`, `bool` and `list[str]`. You can assign a new value or mutate a list. Declare each state variable using `set` or `save` in Tappy first. Its type must stay the same; numeric state accepts Python integers and floats. Lists hold at most 10,000 strings. State is copied into Python and returned together after successful execution. New Python variables are local helpers; changing form inputs or component parameters in Python does not change them in later Tappy actions. Avoid names beginning with `__`, which Python uses for module metadata.

Subsequent Tappy actions see updated state:

```tappy
            python:
                total = sum([10, 20, 30])
            set total to total + 1
```

## Existing Python files and packages

Instead of an inline block, run a script:

```tappy
        on submit:
            python "./calculate.py"
```

The script has the same state and input variables as an inline block. Paths, the working directory, and imports resolve from the `.tappy` file's directory. Import modules from a script's subdirectory using their package path from the app directory. Scripts are read again for each event, so editing a script takes effect on the next click. Inline blocks keep normal Python quotes, comments, f-strings and indentation after removing one Tappy indentation level.

Create a virtual environment beside the app to use third-party packages:

```sh
python -m venv .venv
# Windows:
.venv\Scripts\python -m pip install requests
# macOS / Linux:
.venv/bin/python -m pip install requests
```

Interpreter selection uses this order:

1. `TAPPY_PYTHON`: a path to a Python 3 executable, without extra arguments.
2. The app directory's `.venv`.
3. An active environment from `VIRTUAL_ENV`.
4. `python.exe` on Windows or `python3` on macOS/Linux, found through PATH.

For example, in PowerShell: `$env:TAPPY_PYTHON = 'C:\Python313\python.exe'`.

## Checking and errors

`tappy check app.tappy` compiles Python blocks and referenced scripts to catch syntax errors, including inactive event branches. It does not execute them or import their dependencies. Python is needed for checking and serving apps that contain Python events. Apps without these events retain their standalone runtime behavior. Static export continues to reject event handlers.

Exceptions appear on the page with a Python traceback. Inline tracebacks use the `.tappy` file's original line numbers; scripts use their own filename and lines. Tappy rolls back state changes from the entire event if Python, a later action, rendering or persistence fails. Python runs in a fresh process for each block, so imported modules and helper variables do not persist across clicks. Use `save` state or your own files for persistent data.

Each block has a 10 second process deadline and a 1 MiB serialized state result limit. Long-running blocks hold up other state requests until they finish or time out. These blocks execute trusted server-side Python with the current user's permissions. Files, network requests and other external effects cannot be rolled back; spawned child processes are not managed by the bridge. Never execute submitted code or use `eval` on form input.
