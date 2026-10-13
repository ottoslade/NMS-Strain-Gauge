# Ruff

We use [ruff](https://github.com/astral-sh/ruff) to automatically enforce python style guides. See the [`ruff.toml`](../ruff.toml) file for a full list of the rules we check.

Ruff has 2 modes: linting to catch potential errors and some style inconsistencies and formatting for readability. Use uv to run them manually though they will be run on every commit using pre-commit.

```bash
uv run ruff check --fix 
uv run ruff format
```

See [here](https://docs.astral.sh/ruff/) for the full documentation.

