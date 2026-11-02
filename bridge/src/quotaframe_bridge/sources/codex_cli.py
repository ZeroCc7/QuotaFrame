"""Read sanitized Codex quota through the official CLI's read-only app-server RPC."""

from __future__ import annotations

import asyncio
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
from urllib.request import getproxies

from quotaframe_bridge.domain.models import Provider, ProviderUsage, SourceState, UsageWindow
from quotaframe_bridge.sources.codexbar import _percent


def parse_rate_limits(result: dict, sampled_at: int) -> dict[Provider, ProviderUsage]:
    limits = result.get("rateLimitsByLimitId") or {}
    quota = limits.get("codex") or result.get("rateLimits")
    if not isinstance(quota, dict):
        raise ValueError("Codex quota is unavailable")

    def window(name: str) -> UsageWindow | None:
        raw = quota.get(name)
        if raw is None:
            return None
        if not isinstance(raw, dict):
            raise ValueError("invalid Codex quota window")
        used = _percent(raw.get("usedPercent"), "usedPercent")
        reset = raw.get("resetsAt")
        if reset is not None and (isinstance(reset, bool) or not isinstance(reset, int)):
            raise ValueError("invalid Codex reset time")
        return UsageWindow(used, reset)

    short, week = window("primary"), window("secondary")
    present = int(short is not None) + int(week is not None)
    state = SourceState.OK if present == 2 else SourceState.PARTIAL if present else SourceState.UNAVAILABLE
    return {
        Provider.CODEX: ProviderUsage(Provider.CODEX, state, sampled_at, short, week),
        Provider.CLAUDE: ProviderUsage(Provider.CLAUDE, SourceState.UNAVAILABLE, sampled_at),
    }


class CodexCliSource:
    """Start one short-lived CLI per sample; credentials remain owned by Codex."""

    def __init__(self, executable: Path | None = None, *, timeout: float | None = None) -> None:
        self.executable = executable
        self.timeout = 20.0 if timeout is None else timeout

    async def collect(self, attempted_at: int | None = None) -> dict[Provider, ProviderUsage]:
        executable = str(self.executable) if self.executable else shutil.which(
            "codex.exe" if sys.platform == "win32" else "codex"
        )
        if not executable:
            raise RuntimeError("official Codex CLI executable not found")
        environment = os.environ.copy()
        # The Windows tray process may have no proxy environment despite a
        # configured system proxy. Pass that existing preference to the CLI.
        for protocol, proxy in getproxies().items():
            if protocol in ("http", "https", "all", "no"):
                key = protocol + "_proxy"
                if not any(name.lower() == key for name in environment):
                    environment[key.upper()] = proxy
        process = await asyncio.create_subprocess_exec(
            executable, "app-server", "--listen", "stdio://", "-c", "analytics.enabled=false",
            stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.DEVNULL, env=environment,
            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0,
        )

        async def send(message: dict) -> None:
            process.stdin.write((json.dumps(message) + "\n").encode("utf-8"))
            await process.stdin.drain()

        async def response(sequence: int) -> dict:
            while True:
                line = await process.stdout.readline()
                if not line:
                    raise RuntimeError("Codex quota source disconnected")
                message = json.loads(line)
                if message.get("id") != sequence:
                    continue
                if "error" in message or not isinstance(message.get("result"), dict):
                    # RPC errors can contain account metadata; never forward them.
                    raise RuntimeError("Codex quota request failed")
                return message["result"]

        try:
            async with asyncio.timeout(self.timeout):
                await send({"id": 1, "method": "initialize", "params": {
                    "clientInfo": {"name": "quotaframe_bridge", "title": "QuotaFrame Bridge", "version": "1.2.0"},
                    "capabilities": None,
                }})
                await response(1)
                await send({"method": "initialized"})
                await send({"id": 2, "method": "account/rateLimits/read"})
                result = await response(2)
                return parse_rate_limits(result, int(time.time()) if attempted_at is None else attempted_at)
        finally:
            if process.returncode is None:
                try:
                    process.kill()
                except ProcessLookupError:
                    pass
            await process.wait()
