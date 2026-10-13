# uv

We're using [uv](https://github.com/astral-sh/uv) to manage python dependencies and virtual environments. This document should serve as a uv cheatsheet, though I highly recommend reading uv's actual [documentation](https://docs.astral.sh/uv/), specifically the projects section.

## Python Version

Uv pins a python version. The specific version can be found in the [`.python-version`](../.python-version) file. See [here](https://docs.astral.sh/uv/concepts/python-versions/) for more details.

## Python Project

The [`pyproject.toml`](../pyproject.toml) file serves as a homepage for our project. Here, you can see some general info about the project, the dependencies we use, and a hint that tells pytest where to look for imports. See [here](https://docs.astral.sh/uv/concepts/configuration-files/) for more details.

## Lockfile

The [uv.lock](../uv.lock) file pins our dependencies to a commit hash. DO NOT MODIFY THIS FILE YOURSELF. It will be edited automatically as dependencies change/update. See [here](https://docs.astral.sh/uv/concepts/projects/layout/#the-lockfile) for more details.

## Adding a New Dependency

To add a new dependency, use the `uv add` command.
```bash
uv add <my_dep>
```

If your dependeny can only be run on a Raspberry Pi, add it to the `pi` group.
```bash
uv add -g pi <my_pi_dep>
```

## Running Tools and Virtual Envs

To run a tool (like pytest), use the `uv run` command.
```bash
uv run pytest --help
```

This will run the tool in a automatically managed virtual environment.