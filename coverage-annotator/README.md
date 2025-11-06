# Utility tool to annotate output report from `llvm-cov`

Will annotate output report from `llvm-cov` (`index.html`), using comments included in a JSON file.

## Usage

```bash
uv run add-annotations coverage/html/index.html annotations.json coverage_with_annotations.html
```

Format of `annotations.json`, file paths must match exactly those in the report table:

```json
{
  "include/evmmax/evmmax.hpp": "something something"
}
```