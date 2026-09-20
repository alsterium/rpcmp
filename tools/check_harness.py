"""Check current-task navigation and the host verification preset, offline."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


def check(root: Path) -> list[str]:
    errors = []
    documents = (
        "docs/CURRENT.md", "README.md", "prompts/work-current.md",
        "docs/development/harness.md", "tests/README.md",
    )
    for name in documents:
        path = root / name
        if not path.is_file():
            errors.append(f"missing entry document: {name}")
            continue
        for target in re.findall(r"\[[^\]]+\]\(([^)]+)\)", path.read_text(encoding="utf-8")):
            if "://" in target or target.startswith("#"):
                continue
            if not (path.parent / target.split("#", 1)[0]).is_file():
                errors.append(f"broken local link: {name} -> {target}")

    for name, target in (("README.md", "docs/CURRENT.md"),
                         ("prompts/work-current.md", "../docs/CURRENT.md")):
        path = root / name
        if path.is_file() and f"]({target})" not in path.read_text(encoding="utf-8"):
            errors.append(f"entry must link CURRENT: {name}")

    current = root / "docs/CURRENT.md"
    if current.is_file():
        targets = re.findall(r"^Active milestone: \[[^\]]+\]\((milestones/[^)]+\.md)\)$",
                             current.read_text(encoding="utf-8"), re.MULTILINE)
        if len(targets) != 1:
            errors.append("CURRENT must identify exactly one active milestone")
        elif (current.parent / targets[0]).is_file():
            milestone = (current.parent / targets[0]).read_text(encoding="utf-8")
            if not re.search(r"^Status: active\b", milestone, re.MULTILINE):
                errors.append("CURRENT points to a milestone not marked active")

    try:
        presets = json.loads((root / "CMakePresets.json").read_text(encoding="utf-8"))
        host = next(p for p in presets["configurePresets"] if p["name"] == "host-msvc")
        if set(host) - {"name", "displayName", "description", "generator", "binaryDir", "cacheVariables"}:
            errors.append("host-msvc configure must not add hidden overrides")
        cache = host["cacheVariables"]
        for key in ("BUILD_TESTING", "RPCMP_ENABLE_CLANG_TOOLS"):
            if cache.get(key) != "ON":
                errors.append(f"host-msvc must enable {key}")
        if not re.fullmatch(r"\d+\.\d+\.\d+", cache.get("RPCMP_LLVM_VERSION", "")):
            errors.append("host-msvc must pin LLVM")
        test = next(p for p in presets["testPresets"] if p["name"] == "host-msvc")
        # These small execution presets deliberately have no hidden inheritance,
        # conditions, environment filters, targets or other selection overrides.
        if set(test) - {"name", "displayName", "description", "configurePreset", "output", "execution"}:
            errors.append("Full test preset must not add filters or overrides")
        if test.get("configurePreset") != "host-msvc":
            errors.append("Full tests must use host-msvc")
        if test.get("execution", {}).get("noTestsAction") != "error":
            errors.append("empty test runs must fail")
        if test.get("execution") != {"noTestsAction": "error"}:
            errors.append("Full execution must not restrict the test run")
        build = next(p for p in presets["buildPresets"] if p["name"] == "host-msvc")
        if build != {"name": "host-msvc", "configurePreset": "host-msvc"}:
            errors.append("host-msvc must build all default targets without overrides")
        fast = next(p for p in presets["testPresets"] if p["name"] == "host-msvc-fast")
        if fast != {"name": "host-msvc-fast", "inherits": "host-msvc",
                    "filter": {"exclude": {"name": "^tidy$"}}}:
            errors.append("Fast must inherit Full and exclude only ^tidy$")
        workflow = next(p for p in presets["workflowPresets"] if p["name"] == "host-verify")
        if workflow["steps"] != [dict(type=t, name="host-msvc") for t in ("configure", "build", "test")]:
            errors.append("host-verify must configure, build, and test host-msvc")
        fast_workflow = next(p for p in presets["workflowPresets"] if p["name"] == "host-verify-fast")
        if fast_workflow["steps"] != [
                dict(type="configure", name="host-msvc"),
                dict(type="build", name="host-msvc"),
                dict(type="test", name="host-msvc-fast")]:
            errors.append("host-verify-fast must configure/build host-msvc and test host-msvc-fast")
    except (OSError, ValueError, KeyError, StopIteration, TypeError) as error:
        errors.append(f"invalid host presets: {error}")
    return errors


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    failures = check(parser.parse_args().root)
    print("\n".join(failures) if failures else "harness navigation and presets: PASS")
    raise SystemExit(bool(failures))
