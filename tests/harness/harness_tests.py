"""Mutation tests: the navigation/configuration gate must reject regressions."""

import importlib.util
import json
import shutil
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("check_harness", ROOT / "tools/check_harness.py")
HARNESS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(HARNESS)


class HarnessTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for folder in ("docs", "prompts", "tests"):
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


if __name__ == "__main__":
    unittest.main()
