# Language reference · MVP 0.3

## Named pages and reusable routes

```tappy
set home to new page with name "Home":
    suffix: home
    heading "Welcome"
    link "Details" details

set details to new page with name "Details":
    suffix: details
    link "Home" home
    button "Back":
        on click:
            open home
```

`suffix: home` means `/home` on the current site (for example `app.com/home`). Use `suffix: /` for the root. Names are immutable page references usable by `link`, `open`, and `{interpolation}`. They cannot collide with state names. Links can refer to pages declared later in the file. A named page must have exactly one suffix. Existing `page "Home" /home` declarations remain supported.

## Files and blocks

Use `.tappy`, one tab or four spaces per indentation level, and double quotes for literal text. A tab advances to the next four-column tab stop; tabs and spaces may coexist when they align to complete levels. Blank lines and `# comments` (including inline comments outside quoted strings) are allowed. Blocks can have a trailing colon; page declarations retain `page "Name" /route`. Strings support `\n`, `\t`, `\"` and `\\`.

Names use letters, numbers and underscores and cannot start with a number. Routes are `/` or slash-separated segments using letters, numbers, `_` and `-`. No trailing slash, route parameters or reserved `/_tappy` paths.

```tappy
page "Home" /
    heading "A small beginning."

page "About" /about
    text "A second page."
    link "Home" /
```

## State and values

Declare state at the top level. Variables keep their original type.

```tappy
set count to 0
set ready to false
set greeting to "Hello"
save tasks as []
```

`set` declares memory-only state at the top level. `save` persists strings, numbers, booleans or lists between restarts. All state is app-wide. Lists start empty and contain strings.

Read a variable with `text greeting`, or interpolate inside text: `text "You have {tasks.count} tasks"`. `.count` returns list length. There is no general expression interpolation inside braces: use a variable or `.count`.

Arithmetic supports `+ - * /`, parentheses and unary `+`/`-`. Multiplication and division take precedence over addition and subtraction; operators at the same precedence evaluate left to right. For example, `2 + 3 * 4` is 14, while `(2 + 3) * 4` is 20. Operators can touch their operands (`count+1`). `set count to count + 1` updates a number inside an event. Events may only update already-declared globals; they cannot silently create state. Duplicate top-level declarations are errors. `+` joins strings if either operand is text. Define strings/numbers before using them in other declarations. An input named `draft` exposes text as either `draft` or `draft.value` in its event.

## Elements and layout

| Command | Result |
| --- | --- |
| `heading "Title"` | h1 |
| `subheading "Title"` | h2 |
| `text "Paragraph"` | p |
| `link "About" /about` | Link with HTMX navigation for declared pages |
| `button "Save"` | Submit button, or action button with `on click` |
| `image "https://example.com/photo.png" "Description"` | Image with required alt text |
| `divider` | Horizontal rule |
| `row`, `stack`, `grid`, `card` | Layout containers |
| `section`, `navigation`, `footer` | Semantic containers |

Put child elements inside layout containers. Heading, text, image and link children are limited to style/HTMX attributes. Buttons contain styles/HTMX attributes and optionally one `on click` block. Nesting forms is invalid. Use a form's submit event for submissions rather than a click event on its submit button.

```tappy
page "Home" /
    stack:
        heading "Hello!"
        grid:
            card:
                subheading "First idea"
                text "Something useful."
            card:
                subheading "Next idea"
                text "Something fun."
```

## Styling and identifiers

Put style commands inside the element to style. Numeric lengths become pixels; quoted values can include CSS units. Values are single CSS values, not arbitrary stylesheets.

```tappy
page "Home" /
    background "#f7f7fc"
    heading "Make something.":
        color "#7048d8"
        size 48
        center
    card:
        id "summary"
        padding 28
        rounded 20
        text "A card with a stable HTML ID."
```

Supported: `background`, `color`, `size`, `padding`, `rounded`, `gap`, `width`, `height`, `columns` (1–12), `center`, `bold`, `muted`, and `id "name"`. `center` aligns text. `id "app"` is reserved. Give HTML IDs unique names; do not repeat IDs inside loops. Multiple style rules for the same CSS property follow normal CSS ordering.

## Components

Components reuse UI and can read global state. Parameters are positional and scoped to each use. They may be text, numbers, booleans or lists. Arguments are expressions evaluated in the caller’s scope. Parameters cannot shadow global state names; `set` inside an event always updates a declared global. Event handlers capture the arguments of the particular component instance.

```tappy
component Navigation:
    navigation:
        link "Home" /
        link "About" /about

page "Home" /
    use Navigation
    heading "Welcome"
```

```tappy
component Metric(label, value):
    card:
        subheading label
        heading value

page "Metrics" /
    grid:
        use Metric("Completed", completed)
        use Metric("Goal", goal)
```

The example assumes `completed` and `goal` have been declared. Argument counts and duplicate parameter names are checked. Recursive components are rejected. Parameterized component bodies are checked at each use; unused parameterized bodies cannot be type-checked until arguments are supplied.

## Forms and actions

```tappy
save feedback as []
set notice to "Tell us what you think."

page "Feedback" /
    heading "Feedback"
    text notice
    form:
        input "Name" as name required
        input "Email" as email type email required
        textarea "Message" as message required
        button "Send"
        on submit:
            add "{name}: {message}" to feedback
            set notice to "Thank you, {name}!"
```

Inputs support `required`, `placeholder "Hint"`, and `type text|email|number|password|date`. Each input name must be unique on its page and must not collide with a state variable. Browser validation handles the HTML field types. The server checks required fields and basic email structure, and caps field length at 4096 bytes. Inputs are strings, including `type number`. Forms reset after a successful page replacement. Failed submissions also replace the page and currently clear typed values.

Actions:

| Command | Meaning |
| --- | --- |
| `set count to count + 1` | Change a declared variable, preserving its type |
| `add draft.value to tasks` | Append text to a list |
| `remove task from tasks` | Remove the current loop item by captured index |
| `remove 0 from tasks` | Remove an item by zero-based numeric index |
| `clear tasks` / `clear draft` | Empty a list or named form input |
| `open "/about"` | Navigate to a declared page and stop the action |
| `stop` | Stop the current action |
| `if ...`, `else` | Conditional actions |

Actions are atomic: all mutations are rolled back if execution, rendering or saving fails. List length is capped at 10,000. A stale action refreshes the page instead of applying to changed shared data. Completed action tokens cannot be replayed.

A button outside a form can use `on click`. It includes page inputs as optional values; use a form with `on submit` when fields must be required. Buttons with click actions inside a form see the form's inputs. A plain button without `on click` is a submit button.

## Conditions and loops

```tappy
page "Tasks" /
    if tasks.count is 0:
        text "No tasks yet."
    else:
        for each task in tasks:
            row:
                text task
                button "Done":
                    on click:
                        remove task from tasks
```

Conditions accept a truthy value, or two values separated by `is`, `==`, `!=`, `<`, `>`, `<=`, `>=`. Ordering comparisons need numbers. Equality compares both type and value. Empty text/lists, false and zero are falsey. Boolean `and`/`or` and `else if` are not implemented. `else` must immediately follow `if` at the same indentation.

Inside a loop, `task` is the item's text, and `task.index` is its zero-based index. The loop variable exists only inside its block. Duplicate list values are supported: deletion uses the index, not matching text.

## Native HTMX and fragments

The entire pinned HTMX core is served locally. Ordinary forms, buttons and navigation generate native HTMX attributes automatically. For additional behavior, use `htmx "attribute" "value"` inside an element.

```tappy
set completed to 7

page "Dashboard" /
    card:
        id "stats"
        htmx "hx-get" "/stats"
        htmx "hx-trigger" "load, every 5s"
        htmx "hx-swap" "innerHTML"
        text "Loading…"

fragment "Live stats" /stats
    subheading "{completed} tasks completed"
```

A fragment route returns HTML without a document or `#app` wrapper. Fragments are read-only views. Requests may use local paths only. GET fragments can poll state changed by other actions. The `fragments.tappy` example updates numeric state using a button and fetches the fragment.

Supported attribute passthrough includes HTTP verbs, target, swap, trigger, boost, push/replace URL, select/select-oob, swap-oob, sync, include, params, vals, headers, confirm, prompt, indicator, disabled-elt, disable, disinherit/inherit, encoding, history/history-elt, preserve, request and validate. Values can interpolate variables. Only local endpoints implemented by your app/runtime will respond; defining an attribute does not implement an API endpoint.

JavaScript evaluation, `hx-on` scripts, `js:` expressions and extensions are not enabled. For elements with automatic form/action/navigation behavior, overriding their generated request/target/swap attributes is rejected to avoid duplicate attributes. Put manual HTMX behavior on a separate element instead. Refer to the [official HTMX reference](https://htmx.org/reference/) for attribute semantics.


## Compatibility and inputs

Existing `state name as value`, `remember name as value` and `nav:` source remains supported. Prefer `set name to value`, `save name as value` and `navigation:` in new code. `navigation bar:` is also accepted. Existing saved-data files remain compatible.

`clear draft` clears a named input in the current event, so subsequent expressions read an empty string. Successful form responses render a fresh empty form. `clear tasks` still empties a declared list. Numeric updates use `set count to count + 1`; `add item to tasks` is reserved for lists.

## Live source updates

Browsers keep one server-sent event connection open. The server notifies them when valid source changes; the browser then reloads. There is no half-second HTTP polling loop. A small filesystem watcher checks metadata four times per second once per server, not once per browser, and reads/compiles the file only when it changes. Idle connections send a keepalive every 25 seconds. Up to eight live-change connections are admitted, with separate thread capacity kept for normal requests.

## Automatic language updates

Installed runtime packages check the public `TappyWeb/TappyWeb` GitHub release at most once per day, in the background when serving. Updates must have an official release archive and a matching GitHub SHA256 digest. They are unpacked in a versioned, per-user cache and take effect on the next invocation. An active server keeps its current runtime. No administrator permission is needed for a runtime update.

Run `tappy update` to check immediately. Set `TAPPY_NO_UPDATE=1` to disable both automatic checks and forwarding to a cached newer version. Check/build commands do not initiate network checks. Studio has no separate automatic updater or GitHub login.

The language installer registers `.tappy` sources to launch in Terminal on macOS or Command Prompt on Windows. OS-level default-app preferences can retain an existing association; choose Tappyweb in Open With if the OS preserves an earlier selection.
