from pathlib import Path
root = Path(__file__).resolve().parent.parent
out = '#pragma once\n#include <string_view>\n'
for name, path in [('htmx_js', 'assets/htmx.min.js'), ('default_css', 'assets/default.css')]:
    content = (root / path).read_text()
    out += f'inline constexpr std::string_view {name} = R"TAPPY_ASSET({content})TAPPY_ASSET";\n'
(root / 'src/assets.hpp').write_text(out)
