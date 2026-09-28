#!/usr/bin/env python3
"""A failing member names its issue (task N2 of the 2026-09-28 ecosystem design,
mcpp-community/mcpp `.agents/docs/2026-09-28-ecosystem-design-and-optimisation-plan.md`).

WHY. The two full sweeps of 2026-09-26 on `main` failed -- one on the
`mirror-cn-reachable` job, one on the member `pangocairo` (linux) -- and nothing
recorded either: the job that opens the sweep issue failed as well, and a red
weekly sweep that no issue names is read by nobody (#482).

`tests/known-red.tsv` lists the members known to fail, per leg, each with the
open issue that owns it. After the members have run, this script reads every
shard's timing rows (`<seconds>\t<member>\t<ok|FAIL>`, in the artifacts
`timings-<platform>-<toolchain>-<shard>`), and:

- lists every failing member with its issue, in the run summary;
- fails when a failing member has no issue on its leg;
- fails when the members' job failed and no member row says FAIL (a shard that
  failed before testing: nothing can be attributed);
- lists a known-red member that passed, so its line can be removed.

    red_members.py report <timings dir> <known-red.tsv> [--members-result R]
    red_members.py selftest

A leg is `<platform>-<toolchain>` (`linux-default`, `linux-llvm`,
`macos-default`, `windows-default`); `*` in known-red.tsv matches every leg.
"""
from __future__ import annotations

import pathlib
import sys
import tempfile


def read_known(path: pathlib.Path) -> dict[tuple[str, str], str]:
    known: dict[tuple[str, str], str] = {}
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        parts = line.split("\t")
        if len(parts) != 3 or not all(p.strip() for p in parts):
            raise SystemExit(f"{path}:{number}: expected <leg>\\t<member>\\t<issue>, got {line!r}")
        leg, member, issue = (p.strip() for p in parts)
        known[(leg, member)] = issue
    return known


def read_rows(timings: pathlib.Path) -> list[tuple[str, str, str]]:
    """(leg, member, status) for every row of every shard."""
    rows = []
    for tsv in sorted(timings.glob("timings-*/timings.tsv")):
        # timings-<platform>-<toolchain>-<shard>
        name = tsv.parent.name.split("-")
        if len(name) < 4:
            continue
        leg = f"{name[1]}-{name[2]}"
        for line in tsv.read_text(encoding="utf-8").splitlines():
            parts = line.split("\t")
            if len(parts) == 3:
                rows.append((leg, parts[1], parts[2].strip()))
    return rows


def report(timings: pathlib.Path, known_path: pathlib.Path, members_result: str,
           out=sys.stdout) -> int:
    known = read_known(known_path)
    rows = read_rows(timings)
    failing = sorted({(leg, m) for leg, m, s in rows if s == "FAIL"})
    passing = {(leg, m) for leg, m, s in rows if s == "ok"}

    def issue_of(leg: str, member: str) -> str | None:
        return known.get((leg, member)) or known.get(("*", member))

    unattributed = [(leg, m) for leg, m in failing if not issue_of(leg, m)]
    print("### Red members", file=out)
    print("", file=out)
    if failing:
        print("| leg | member | issue |", file=out)
        print("|---|---|---|", file=out)
        for leg, m in failing:
            print(f"| {leg} | `{m}` | {issue_of(leg, m) or '**none**'} |", file=out)
    else:
        print("No member failed.", file=out)
    healed = sorted((leg, m) for (leg, m) in known
                    if leg != "*" and (leg, m) in passing)
    if healed:
        print("", file=out)
        print("Listed in tests/known-red.tsv and passing now (remove the line once "
              "its issue is closed):", file=out)
        for leg, m in healed:
            print(f"- {leg} `{m}` ({known[(leg, m)]})", file=out)
    status = 0
    if unattributed:
        print("", file=out)
        print("A failing member without an issue fails this job: open one and add "
              "`<leg>\\t<member>\\t<issue>` to tests/known-red.tsv, or fix the member.",
              file=out)
        status = 1
    if members_result == "failure" and not failing:
        print("", file=out)
        print("The members' job failed, and no member row says FAIL: a shard failed "
              "before testing, and nothing can be attributed to a member.", file=out)
        status = 1
    return status


def selftest() -> int:
    def run(rows_by_artifact: dict[str, str], known: str, result: str) -> tuple[int, str]:
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            for artifact, text in rows_by_artifact.items():
                (root / artifact).mkdir()
                (root / artifact / "timings.tsv").write_text(text, encoding="utf-8")
            known_path = root / "known-red.tsv"
            known_path.write_text(known, encoding="utf-8")
            import io
            buf = io.StringIO()
            code = report(root, known_path, result, out=buf)
            return code, buf.getvalue()

    header = "# <leg>\t<member>\t<issue>\n"
    cases = [
        # (name, rows, known, members result, expected status, text that must appear)
        ("all green", {"timings-linux-default-0": "10\ta\tok\n"}, header, "success", 0, "No member failed"),
        ("a red member without an issue", {"timings-linux-default-0": "10\ta\tFAIL\n"}, header,
         "failure", 1, "**none**"),
        ("a red member with its issue", {"timings-linux-default-0": "10\ta\tFAIL\n"},
         header + "linux-default\ta\t#1\n", "failure", 0, "| linux-default | `a` | #1 |"),
        ("an issue on another leg does not cover", {"timings-linux-llvm-1": "10\ta\tFAIL\n"},
         header + "linux-default\ta\t#1\n", "failure", 1, "**none**"),
        ("a wildcard leg covers", {"timings-windows-default-0": "10\ta\tFAIL\n"},
         header + "*\ta\t#2\n", "failure", 0, "#2"),
        ("a shard that failed before testing", {"timings-linux-default-0": "10\ta\tok\n"}, header,
         "failure", 1, "failed before testing"),
        ("a known-red member that passes is named", {"timings-macos-default-0": "10\ta\tok\n"},
         header + "macos-default\ta\t#3\n", "success", 0, "passing now"),
    ]
    failed = 0
    for name, rows, known, result, want_status, want_text in cases:
        status, text = run(rows, known, result)
        ok = status == want_status and want_text in text
        print(f"{'ok' if ok else 'FAIL'}: {name}")
        if not ok:
            failed += 1
            print(text)
    return 1 if failed else 0


def main(argv: list[str]) -> int:
    if argv[:1] == ["selftest"]:
        return selftest()
    if argv[:1] == ["report"] and len(argv) >= 3:
        result = ""
        if "--members-result" in argv:
            result = argv[argv.index("--members-result") + 1]
        return report(pathlib.Path(argv[1]), pathlib.Path(argv[2]), result)
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
