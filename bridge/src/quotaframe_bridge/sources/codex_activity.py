"""Read-only, incremental desktop turn tracking from local Codex records.

The on-disk schema is internal. Missing or unreadable records are unknown,
never evidence that a turn finished. No message text leaves this module.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
import os
from pathlib import Path
import secrets
import sqlite3
import time


@dataclass(frozen=True, slots=True)
class ActivitySnapshot:
    stream: int
    available: bool = False
    running: int = 0
    unknown: int = 0
    completed: int = 0
    interrupted: int = 0
    event_age_ms: int = 0xFFFFFFFF


@dataclass(slots=True)
class _Record:
    path: Path
    offset: int = 0
    turn: str = ""
    state: str = "idle"
    changed_at: float = 0
    identity: tuple[int, int] | None = None


class CodexActivitySource:
    """Poll all indexed, unarchived desktop main chats without modifying Codex.

    The first scan is a silent baseline. Previously unclosed turns stay unknown
    until a new start or terminal event is observed. Silent running turns become
    unknown after two minutes, since a crashed app may not write a terminal event.
    """

    def __init__(self, home: Path | None = None, *, monotonic=time.monotonic,
                 wall_clock=time.time) -> None:
        self.home = home or Path(os.environ.get("CODEX_HOME", Path.home() / ".codex"))
        self._now = monotonic
        self._started_at = wall_clock()
        self._records: dict[str, _Record] = {}
        self._finished: set[tuple[str, str]] = set()
        self._baseline = True
        self._stream = secrets.randbits(32)
        self._completed = 0
        self._interrupted = 0
        self._event_at: float | None = None

    def collect(self) -> ActivitySnapshot:
        now = self._now()
        database = self.home / "state_5.sqlite"
        try:
            with sqlite3.connect(database.resolve().as_uri() + "?mode=ro", uri=True,
                                 timeout=0.2) as connection:
                rows = connection.execute(
                    "SELECT id, rollout_path, created_at FROM threads WHERE archived=0 "
                    "AND originator='Codex Desktop' AND source='vscode'"
                ).fetchall()
        except (OSError, sqlite3.Error):
            return ActivitySnapshot(self._stream)
        present = set()
        for thread, path, created_at in rows:
            present.add(thread)
            record = self._records.get(thread)
            historical = self._baseline
            if record is None or record.path != Path(path):
                record = self._records[thread] = _Record(Path(path), changed_at=now)
                historical = historical or created_at < self._started_at
            try:
                stat = record.path.stat()
                identity = (stat.st_dev, stat.st_ino)
                if record.offset > stat.st_size or (
                        record.identity is not None and record.identity != identity):
                    record.offset = 0
                    record.state = "unknown"
                    historical = True
                record.identity = identity
                if stat.st_size > record.offset:
                    with record.path.open("rb") as stream:
                        stream.seek(record.offset)
                        while line := stream.readline():
                            # Retry a writer's incomplete last line next poll.
                            if not line.endswith(b"\n"):
                                break
                            record.offset = stream.tell()
                            if not any(marker in line for marker in (
                                    b'"task_started"', b'"task_complete"', b'"turn_aborted"')):
                                continue
                            try:
                                item = json.loads(line)
                            except (ValueError, UnicodeError):
                                record.state = "unknown"
                                continue
                            payload = item.get("payload", {})
                            if item.get("type") == "event_msg" and isinstance(payload, dict):
                                self._accept(thread, record, payload, historical, now)
                    record.changed_at = now
                if record.state == "running" and now - record.changed_at > 120:
                    record.state = "unknown"
            except OSError:
                record.state = "unknown"
        self._records = {key: value for key, value in self._records.items() if key in present}
        self._baseline = False
        age = (0xFFFFFFFF if self._event_at is None else
               min(0xFFFFFFFF, max(0, int((now - self._event_at) * 1000))))
        return ActivitySnapshot(
            self._stream, True,
            sum(r.state == "running" for r in self._records.values()),
            sum(r.state == "unknown" for r in self._records.values()),
            self._completed, self._interrupted, age,
        )

    def _accept(self, thread: str, record: _Record, payload: dict,
                historical: bool, now: float) -> None:
        kind, turn = payload.get("type"), payload.get("turn_id")
        if kind not in {"task_started", "task_complete", "turn_aborted"}:
            return
        if not isinstance(turn, str) or not turn:
            record.state = "unknown"
            return
        key = (thread, turn)
        if key in self._finished:
            return
        if kind == "task_started":
            record.turn = turn
            record.state = "unknown" if historical else "running"
        else:
            self._finished.add(key)
            if record.turn != turn:
                return
            record.state = "idle"
            if not historical:
                if kind == "task_complete":
                    self._completed += 1
                else:
                    self._interrupted += 1
                self._event_at = now


def main() -> None:
    """Print the exact sanitized activity wire message, without using BLE."""
    from quotaframe_bridge.protocol.messages import encode_activity

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--watch", action="store_true")
    args = parser.parse_args()
    source = CodexActivitySource()
    sequence = 0
    while True:
        print(encode_activity(source.collect(), sequence).decode(), end="", flush=True)
        if not args.watch:
            return
        sequence = (sequence + 1) & 0xFFFFFFFF
        time.sleep(2)


if __name__ == "__main__":
    main()
