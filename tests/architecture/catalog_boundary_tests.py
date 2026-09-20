"""Exercise the new catalog boundary independently for every forbidden edge."""

from pathlib import Path
import tempfile

from check_dependencies import find_violations


def main():
    for owner, dependency in [
        ("player", "ui"), ("ui", "player"), ("ui", "library"),
        ("contracts", "player"), ("contracts", "library"),
        ("contracts", "runtime"), ("contracts", "ui"),
        ("pocket_canvas", "player"), ("pocket_canvas", "runtime"),
        ("pocket_canvas", "library"), ("pocket_canvas", "pocket_sound"),
    ]:
        with tempfile.TemporaryDirectory(prefix="rpcmp-catalog-boundary-") as directory:
            root = Path(directory)
            source = root / "core" / owner / "consumer.cpp"
            source.parent.mkdir(parents=True)
            source.write_text(f'#include "rpcmp/{dependency}/forbidden.hpp"\n', encoding="utf-8")
            assert len(find_violations(root)) == 1, (owner, dependency, "source")
            source.write_text('#include "rpcmp/contracts/catalog.hpp"\n', encoding="utf-8")
            assert not find_violations(root), (owner, "contract-only")
            cmake = root / "CMakeLists.txt"
            cmake.write_text(
                f"target_link_libraries(rpcmp_{owner} PUBLIC rpcmp_contracts)\n"
                f"target_link_libraries(rpcmp_{owner} PRIVATE rpcmp_{dependency})\n",
                encoding="utf-8",
            )
            assert len(find_violations(root)) == 1, (owner, dependency, "second link call")
    with tempfile.TemporaryDirectory(prefix="rpcmp-rtl-boundary-") as directory:
        root = Path(directory)
        rtl = root / "core/rtl/pocket"
        rtl.mkdir(parents=True)
        for name in ("mixer", "audio", "mmio"):
            (rtl / f"rpcmp_hybrid_{name}.sv").write_text("// approved M6 transport\n")
        assert not find_violations(root), "approved hybrid slice rejected"
        (rtl / "future_device.sv").write_text("// outside the active slice\n")
        assert len(find_violations(root)) == 1, "RTL allowlist became unrestricted"
    print("catalog and RTL architecture boundaries: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
