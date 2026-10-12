"""Create independent review copies, snapshotting committed SQLite data.

Never overwrites an existing review. Live text buffers and an IME preedit are
not captured. Auxiliary recovery databases retain their own identities/leases.
"""
from pathlib import Path
import argparse
import json
import shutil
import sqlite3
import uuid


def snapshot(origin: Path, destination: Path):
    if destination.exists():
        raise RuntimeError("Review already exists; do not overwrite: " + str(destination))
    if origin.is_symlink() or any(p.is_symlink() for p in origin.rglob("*")):
        raise RuntimeError("Review sources must not contain symbolic links")
    project_id = json.loads((origin / "manifest.json").read_text())["project_id"]
    assert str(uuid.UUID(project_id)) == project_id
    databases = sorted(origin.rglob("*.sqlite3"))
    excluded = {str(p.relative_to(origin)) for db in databases
                for p in (db, Path(str(db) + "-wal"), Path(str(db) + "-shm"), Path(str(db) + "-journal"))}

    def ignore(folder, names):
        return [name for name in names if str((Path(folder) / name).relative_to(origin)) in excluded]

    shutil.copytree(origin, destination, ignore=ignore)
    for path in databases:
        target = destination / path.relative_to(origin)
        with sqlite3.connect(path.resolve().as_uri() + "?mode=ro", uri=True) as src:
            with sqlite3.connect(target) as dst:
                src.backup(dst)
                assert dst.execute("PRAGMA integrity_check").fetchall() == [("ok",)]
                assert not dst.execute("PRAGMA foreign_key_check").fetchall()
                tables = {r[0] for r in dst.execute("SELECT name FROM sqlite_master WHERE type='table'")}
                if "recovery_records" in tables:
                    for session, owner in dst.execute("SELECT session_id,project_id FROM recovery_records"):
                        assert owner == project_id and str(uuid.UUID(session)) == session
                        lock = target.parent / (owner + "-" + session + ".lock")
                        if lock.exists():
                            lock.unlink()
                        lock.touch(mode=0o600)
    return {"path": str(destination), "origin": str(origin), "project_id": project_id,
            "committed_sqlite_snapshots": len(databases), "mutable_review": True}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("origin", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    print(json.dumps(snapshot(args.origin, args.destination), ensure_ascii=False, indent=2))
