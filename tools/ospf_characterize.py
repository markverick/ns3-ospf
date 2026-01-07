#!/usr/bin/env python3

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path


def _repo_root() -> Path:
    # contrib/ospf/tools/ospf_characterize.py -> contrib/ospf
    return Path(__file__).resolve().parents[1]


def _ns3_root() -> Path:
    # contrib/ospf -> ns-3-dev
    return _repo_root().parents[1]


def _sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def _find_results_dir(example_results_rel: Path) -> Path:
    # Depending on how waf runs the program, outputs may land under
    # ns-3 root or build/.
    candidates = [
        _ns3_root() / example_results_rel,
        _ns3_root() / "build" / example_results_rel,
    ]
    for cand in candidates:
        if cand.exists() and cand.is_dir():
            return cand

    # Fall back: search a little (bounded) for a matching directory.
    for base in (_ns3_root(), _ns3_root() / "build"):
        probe = base / example_results_rel
        if probe.exists() and probe.is_dir():
            return probe

    raise FileNotFoundError(
        f"Could not find results dir '{example_results_rel}' under '{_ns3_root()}' or '{_ns3_root() / 'build'}'"
    )


def _run_waf(program: str, program_args: list[str]) -> None:
    ns3 = _ns3_root()
    waf = ns3 / "./waf"
    cmd = [str(waf), "--run", " ".join([program] + program_args)]
    print("[run]", " ".join(cmd))
    subprocess.run(cmd, cwd=str(ns3), check=True)


def _collect_files(results_dir: Path, include_globs: list[str]) -> list[Path]:
    files: list[Path] = []
    for pattern in include_globs:
        files.extend(sorted(results_dir.glob(pattern)))

    files = [p for p in files if p.is_file()]
    # Ensure stable ordering and stable relative paths
    files.sort(key=lambda p: str(p.relative_to(results_dir)))
    return files


def _write_manifest(results_dir: Path, files: list[Path], manifest_path: Path) -> dict:
    data = {
        "results_dir": str(results_dir),
        "files": {
            str(p.relative_to(results_dir)): _sha256_file(p) for p in files
        },
    }
    manifest_path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")
    return data


def _load_manifest(manifest_path: Path) -> dict:
    return json.loads(manifest_path.read_text())


def _copy_files(results_dir: Path, files: list[Path], dest_dir: Path) -> None:
    if dest_dir.exists():
        shutil.rmtree(dest_dir)
    dest_dir.mkdir(parents=True, exist_ok=True)

    for src in files:
        rel = src.relative_to(results_dir)
        out = dest_dir / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, out)


def _compare(results_dir: Path, manifest: dict) -> tuple[bool, list[str]]:
    errors: list[str] = []

    expected: dict[str, str] = manifest.get("files", {})
    for rel_str, expected_hash in expected.items():
        p = results_dir / rel_str
        if not p.exists():
            errors.append(f"missing: {rel_str}")
            continue
        actual_hash = _sha256_file(p)
        if actual_hash != expected_hash:
            errors.append(f"changed: {rel_str} expected={expected_hash} actual={actual_hash}")

    # Detect unexpected new files that match our tracked set.
    # (We do not consider other files in results_dir at all.)
    return (len(errors) == 0, errors)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Characterization test runner for the contrib/ospf module"
    )
    parser.add_argument(
        "mode",
        choices=["record", "check"],
        help="record = overwrite golden snapshot; check = compare against it",
    )
    parser.add_argument(
        "--example",
        default="ospf-two-nodes",
        help="Example program name (as used by ./waf --run)",
    )
    parser.add_argument(
        "--results-rel",
        default="results/ospf-two-nodes",
        help="Relative results directory produced by the example",
    )
    parser.add_argument(
        "--rng-run",
        default="1",
        help="RngRun value to make randomness deterministic",
    )
    parser.add_argument(
        "--extra-args",
        nargs=argparse.REMAINDER,
        default=[],
        help="Extra args passed to the example after '--'",
    )

    args = parser.parse_args()

    repo = _repo_root()
    golden_root = repo / "ospf" / "test" / "golden" / args.example
    manifest_path = golden_root / "manifest.json"

    # We intentionally track only route outputs for now. Pcaps/ascii traces can be noisy.
    include_globs = ["*.routes"]

    program_args = [f"--RngRun={args.rng_run}"]
    if args.extra_args:
        program_args.extend(args.extra_args)

    _run_waf(args.example, program_args)

    results_dir = _find_results_dir(Path(args.results_rel))
    files = _collect_files(results_dir, include_globs)

    if not files:
        print(f"error: no files matched {include_globs} in {results_dir}", file=sys.stderr)
        return 2

    if args.mode == "record":
        _copy_files(results_dir, files, golden_root)
        _write_manifest(golden_root, _collect_files(golden_root, include_globs), manifest_path)
        print(f"recorded golden snapshot: {golden_root}")
        print(f"manifest: {manifest_path}")
        return 0

    if not manifest_path.exists():
        print(
            f"error: missing manifest at {manifest_path}. Run with mode=record first.",
            file=sys.stderr,
        )
        return 2

    manifest = _load_manifest(manifest_path)
    ok, diffs = _compare(results_dir, manifest)
    if ok:
        print("ok: outputs match golden snapshot")
        return 0

    print("FAILED: outputs differ from golden snapshot", file=sys.stderr)
    for d in diffs:
        print(" -", d, file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
