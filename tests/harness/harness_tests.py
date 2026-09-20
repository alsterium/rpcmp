"""Mutation tests: the navigation/configuration gate must reject regressions."""

import importlib.util
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("check_harness", ROOT / "tools/check_harness.py")
HARNESS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(HARNESS)

OPTIONS = None


class HarnessTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for folder in ("docs", "prompts", "tests", "specs"):
            for source in (ROOT / folder).rglob("*.md"):
                target = self.root / source.relative_to(ROOT)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, target)
        for name in ("README.md", "CMakePresets.json"):
            shutil.copyfile(ROOT / name, self.root / name)

    def test_repository_passes(self):
        self.assertEqual(HARNESS.check(self.root), [])

    def test_missing_milestone_fails(self):
        current = self.root / "docs/CURRENT.md"
        current.write_text("Active milestone: [Missing](milestones/missing.md)\n", encoding="utf-8")
        self.assertTrue(any("broken local link" in e for e in HARNESS.check(self.root)))

    def test_completed_milestone_fails(self):
        for milestone in (self.root / "docs/milestones").glob("*.md"):
            milestone.write_text(milestone.read_text(encoding="utf-8").replace(
                "Status: active", "Status: complete"), encoding="utf-8")
        self.assertIn("CURRENT points to a milestone not marked active", HARNESS.check(self.root))

    def test_stale_entry_route_fails(self):
        path = self.root / "README.md"
        path.write_text(path.read_text(encoding="utf-8").replace(
            "docs/CURRENT.md", "docs/milestones/M2-ym2151-fixed-sequence.md"), encoding="utf-8")
        self.assertIn("entry must link CURRENT: README.md", HARNESS.check(self.root))

    def test_disabled_checks_fail(self):
        path = self.root / "CMakePresets.json"
        original = json.loads(path.read_text(encoding="utf-8"))
        for key in ("BUILD_TESTING", "RPCMP_ENABLE_CLANG_TOOLS"):
            with self.subTest(key=key):
                presets = json.loads(json.dumps(original))
                presets["configurePresets"][0]["cacheVariables"][key] = "OFF"
                path.write_text(json.dumps(presets), encoding="utf-8")
                self.assertIn(f"host-msvc must enable {key}", HARNESS.check(self.root))

    def test_empty_run_success_fails(self):
        path = self.root / "CMakePresets.json"
        presets = json.loads(path.read_text(encoding="utf-8"))
        presets["testPresets"][0]["execution"]["noTestsAction"] = "ignore"
        path.write_text(json.dumps(presets), encoding="utf-8")
        self.assertIn("empty test runs must fail", HARNESS.check(self.root))

    def test_omitted_test_step_fails(self):
        path = self.root / "CMakePresets.json"
        presets = json.loads(path.read_text(encoding="utf-8"))
        presets["workflowPresets"][0]["steps"].pop()
        path.write_text(json.dumps(presets), encoding="utf-8")
        self.assertIn("host-verify must configure, build, and test host-msvc", HARNESS.check(self.root))

    def test_selection_overrides_fail(self):
        path = self.root / "CMakePresets.json"
        original = json.loads(path.read_text(encoding="utf-8"))
        mutations = [
            ("testPresets", 0, "filter", {"exclude": {"name": "^format$"}}),
            ("testPresets", 0, "filter", {"include": {"label": "developer-tools"}}),
            ("testPresets", 0, "inherits", "filtered-parent"),
            ("testPresets", 0, "environment", {"CTEST_TEST_TIMEOUT": "1"}),
            ("testPresets", 0, "configurePreset", "unchecked"),
            ("testPresets", 0, "execution", {"noTestsAction": "error", "stopOnFailure": True}),
            ("testPresets", 1, "filter", {"exclude": {"label": "developer-tools"}}),
            ("testPresets", 1, "filter", {"exclude": {"name": "tidy"}}),
            ("testPresets", 1, "filter", {"exclude": {"name": "^(tidy|format)$"}}),
            ("testPresets", 1, "execution", {"noTestsAction": "ignore"}),
            ("testPresets", 1, "inherits", "unchecked"),
            ("testPresets", 1, "configurePreset", "unchecked"),
            ("buildPresets", 0, "targets", ["rpcmp_runtime"]),
            ("configurePresets", 0, "inherits", "unchecked"),
            ("configurePresets", 0, "condition", False),
            ("workflowPresets", 1, "steps", original["workflowPresets"][1]["steps"][:2]),
        ]
        for group, index, key, value in mutations:
            with self.subTest(group=group, index=index, key=key, value=value):
                presets = json.loads(json.dumps(original))
                presets[group][index][key] = value
                path.write_text(json.dumps(presets), encoding="utf-8")
                self.assertTrue(HARNESS.check(self.root))

    def test_actual_ctest_selection(self):
        selections = []
        for preset in ("host-msvc", "host-msvc-fast"):
            result = subprocess.run(
                [OPTIONS.ctest, "--preset", preset, "--test-dir", str(OPTIONS.build_dir),
                 "--show-only=json-v1"], cwd=ROOT, capture_output=True, text=True, check=True)
            selections.append({test["name"] for test in json.loads(result.stdout)["tests"]})
        full, fast = selections
        # Auxiliary builds may legitimately omit clang tools. The canonical
        # Full/Fast configure preset is separately required to enable them.
        cache = (OPTIONS.build_dir / "CMakeCache.txt").read_text(encoding="utf-8")
        setting = re.search(r"^RPCMP_ENABLE_CLANG_TOOLS:[^=]+=(.*)$", cache, re.MULTILINE)
        self.assertIsNotNone(setting, "configured clang-tools option is missing")
        value = setting[1].strip().upper()
        disabled = value in {"", "0", "OFF", "NO", "FALSE", "N", "IGNORE", "NOTFOUND"} or value.endswith("-NOTFOUND")
        self.assertEqual("tidy" in full, not disabled)
        self.assertTrue(fast)
        self.assertEqual(fast, full - {"tidy"})


class WorkflowAcceptanceTests(unittest.TestCase):
    """Opt-in Windows acceptance: real wrapper, compiler, CTest and tidy, isolated inputs."""

    setUp = HarnessTests.setUp

    def run_wrapper(self, *arguments):
        result = subprocess.run(
            ["pwsh", "-NoProfile", "-File", str(self.root / "tools/host-verify.ps1"), *arguments],
            cwd=self.root.parent, capture_output=True, text=True, encoding="utf-8", errors="replace")
        self.outputs.append(result.stdout + result.stderr)
        return result

    def test_workflow_failures_and_coverage(self):
        if not OPTIONS.integration:
            self.skipTest("run --integration for isolated Windows workflow acceptance")
        self.assertEqual(os.name, "nt", "the host wrapper requires Windows")
        self.outputs = []
        for relative in ("tools/host-verify.ps1", "tools/check_harness.py", ".clang-tidy"):
            target = self.root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, target)
        runtime = self.root / "core/runtime"
        runtime.mkdir(parents=True)
        source = self.root / "main.cpp"
        source.write_text("int main() { return 0; }\n", encoding="utf-8")
        tidy_source = self.root / "tidy.cpp"
        tidy_source.write_text("int f() { return 0; }\n", encoding="utf-8")
        checker = (ROOT / "tests/architecture/check_dependencies.py").as_posix()
        cmake = self.root / "CMakeLists.txt"
        base = (
            'cmake_minimum_required(VERSION 3.25)\nproject(probe LANGUAGES CXX)\n'
            'enable_testing()\nadd_executable(probe main.cpp)\n'
            'find_package(Python3 REQUIRED COMPONENTS Interpreter)\n'
            'find_program(TIDY clang-tidy REQUIRED)\n'
        )
        tests = (
            'add_test(NAME functional COMMAND probe)\n'
            f'add_test(NAME architecture COMMAND "${{Python3_EXECUTABLE}}" "{checker}" '
            '--root "${CMAKE_SOURCE_DIR}")\n'
            'add_test(NAME tidy COMMAND "${TIDY}" --config-file "${CMAKE_SOURCE_DIR}/.clang-tidy" '
            '"${CMAKE_SOURCE_DIR}/tidy.cpp" -- -std=c++17)\n'
        )
        cmake.write_text(base + tests, encoding="utf-8")
        try:
            for arguments, mode in (((), "Full"), (("-Mode", "Full"), "Full"),
                                    (("-Mode", "Fast"), "Fast")):
                result = self.run_wrapper(*arguments)
                self.assertEqual(result.returncode, 0, self.outputs[-1])
                self.assertIn(f"Host verification {mode}: PASS", result.stdout)
                if mode == "Fast":
                    self.assertIn("tidy NOT RUN; not full verification", result.stdout)

            source.write_text("int main() { return 1; }\n", encoding="utf-8")
            result = self.run_wrapper("-Mode", "Fast")
            self.assertNotEqual(result.returncode, 0, self.outputs[-1])
            self.assertIn("functional", result.stdout)
            self.assertIn("Host verification Fast: FAIL", result.stdout)
            source.write_text("int main() { return 0; }\n", encoding="utf-8")

            forbidden = runtime / "forbidden.cpp"
            forbidden.write_text('#include "rpcmp/ui/forbidden.hpp"\n', encoding="utf-8")
            result = self.run_wrapper("-Mode", "Fast")
            self.assertNotEqual(result.returncode, 0, self.outputs[-1])
            self.assertIn("runtime depends on UI", result.stdout)
            forbidden.unlink()

            tidy_source.write_text("int f() { int *p = nullptr; return *p; }\n", encoding="utf-8")
            result = self.run_wrapper("-Mode", "Fast")
            self.assertEqual(result.returncode, 0, self.outputs[-1])
            result = self.run_wrapper()
            self.assertNotEqual(result.returncode, 0, self.outputs[-1])
            self.assertIn("clang-analyzer-core.NullDereference", result.stdout)

            source.write_text("this is not C++\n", encoding="utf-8")
            result = self.run_wrapper("-Mode", "Fast")
            self.assertNotEqual(result.returncode, 0, self.outputs[-1])
            self.assertIn("Host verification Fast: FAIL", result.stdout)
            source.write_text("int main() { return 0; }\n", encoding="utf-8")

            cmake.write_text(base, encoding="utf-8")
            for mode in ("Full", "Fast"):
                result = self.run_wrapper("-Mode", mode)
                self.assertNotEqual(result.returncode, 0, self.outputs[-1])
                self.assertIn("No tests were found", self.outputs[-1])
                self.assertIn(f"Host verification {mode}: FAIL", result.stdout)
        finally:
            if OPTIONS.evidence:
                OPTIONS.evidence.parent.mkdir(parents=True, exist_ok=True)
                OPTIONS.evidence.write_text("\n".join(self.outputs), encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "out/build/host-msvc")
    parser.add_argument("--ctest", default="ctest")
    parser.add_argument("--integration", action="store_true")
    parser.add_argument("--evidence", type=Path)
    OPTIONS, remaining = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remaining])
