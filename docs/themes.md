# CSS themes

Put this at the top of your `.tappy` file:

```tappy
set theme to "./themes/midnight.css"
```

The path is relative to that `.tappy` file. To use someone else's theme, use its HTTPS link instead:

```tappy
set theme to "https://raw.githubusercontent.com/TappyWeb/TappyWeb/main/examples/themes/midnight.css"
```

GitHub file links containing `/blob/` are converted to raw links too. Other hosts should return the CSS itself, not a download page or a sign-in screen. No account or token is sent.

Tappy downloads the stylesheet when the server starts, then serves its own copy. Page loads and button clicks do not download it again. A later start tries the host again; if that fails, it uses the last successful copy from your user cache. Restart the server to fetch changes to a remote theme. Local CSS changes show up when you save, without restarting.

## Make a theme in Studio

1. Run the `.tappy` page you want to style.
2. Choose **File → New Theme…** and save it as `theme.css` in your project.
3. Add `set theme to "./theme.css"` to the page and save it.
4. Open the CSS file from the sidebar and change it. Saving refreshes the running preview.
5. Upload the CSS file to your own GitHub repository or web host and share its raw URL.

Studio edits and saves the file locally; it does not upload it or run a theme hosting service.

## Start with the variables

```css
:root {
    --tappy-background: #141419;
    --tappy-text: #eeeeef;
    --tappy-accent: #a78bfa;
    --tappy-surface: #202027;
    --tappy-border: #34343c;
    --tappy-muted: #a6a6b5;
    --tappy-radius: 12px;
}
```

The default stylesheet loads first and your CSS loads after it. You can change just these variables or write selectors for `main`, `h1`, `h2`, `p`, `button`, `input`, `textarea`, `nav`, `.card`, `.grid`, `.row` and `.stack`. Existing `--accent` overrides still work. Inline style commands in your page take precedence over ordinary stylesheet rules.

Themes apply to every page, including HTMX navigation and server updates. `tappy build` includes the CSS in the exported website. `tappy check` checks the theme declaration without downloading it; serve/build report missing files or failed downloads. Stylesheets are limited to 2 MB, and HTML/binary responses are rejected. CSS syntax is interpreted by the browser.

## Fonts and images

Relative asset links in remote stylesheets are resolved against the stylesheet URL. CSS imports and external fonts/images may still be requested by the browser; only the main theme file is cached by Tappy. Use absolute HTTPS URLs (or embedded data URLs) for assets in a local theme. Studio's local preview keeps its existing network restrictions, so external assets may be blocked there even when they work in a normal browser. Theme CSS can make external requests; use a creator you trust.

This description has been written with AI.
