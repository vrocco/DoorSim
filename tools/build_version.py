#!/usr/bin/env python3
import subprocess
from datetime import datetime, timezone

Import("env")


def git_value(args, fallback):
    try:
        return subprocess.check_output(args, text=True).strip()
    except Exception:
        return fallback


def define_string(name, value):
    escaped = value.replace('\\', '\\\\').replace('"', '\\"')
    return f'-D{name}=\\"{escaped}\\"'


commit = git_value(["git", "rev-parse", "--short=12", "HEAD"], "unknown")
branch = git_value(["git", "rev-parse", "--abbrev-ref", "HEAD"], "unknown")
built_at = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

env.Append(
    BUILD_FLAGS=[
        define_string("DOORSIM_BUILD_COMMIT", commit),
        define_string("DOORSIM_BUILD_BRANCH", branch),
        define_string("DOORSIM_BUILD_TIME", built_at),
    ]
)
