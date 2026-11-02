import asyncio
import json
from pathlib import Path
import unittest
from unittest.mock import AsyncMock, Mock, patch

from quotaframe_bridge.domain.models import Provider, SourceState
from quotaframe_bridge.sources.codex_cli import CodexCliSource, parse_rate_limits
from quotaframe_bridge.sources.selection import create_usage_source


class CodexCliTests(unittest.IsolatedAsyncioTestCase):
    def test_projection(self):
        usage = parse_rate_limits({"rateLimitsByLimitId": {"codex": {
            "primary": {"usedPercent": 65.4, "resetsAt": 1800000000},
            "secondary": {"usedPercent": 73, "resetsAt": None},
        }}}, 123)[Provider.CODEX]
        self.assertEqual(usage.short.used_percent, 65)
        self.assertEqual(usage.week.used_percent, 73)
        self.assertEqual(usage.sampled_at, 123)
        self.assertEqual(usage.state, SourceState.OK)

    def test_partial_and_invalid(self):
        self.assertEqual(parse_rate_limits({"rateLimits": {
            "primary": {"usedPercent": 0},
        }}, 123)[Provider.CODEX].state, SourceState.PARTIAL)
        for percent in (-1, 101, True, "bad"):
            with self.assertRaises(ValueError):
                parse_rate_limits({"rateLimits": {"primary": {"usedPercent": percent}}}, 123)

    def process(self, messages):
        process = Mock(returncode=None)
        process.stdin.drain = AsyncMock()
        process.stdout.readline = AsyncMock(side_effect=[
            (json.dumps(message) + "\n").encode() for message in messages
        ])
        process.wait = AsyncMock()
        return process

    async def test_read_only_handshake_and_cleanup(self):
        process = self.process([
            {"id": 1, "result": {}},
            {"method": "notification"},
            {"id": 2, "result": {"rateLimits": {"primary": {"usedPercent": 44}}}},
        ])
        with patch("asyncio.create_subprocess_exec", AsyncMock(return_value=process)):
            result = await CodexCliSource(Path("codex.exe")).collect(123)
        self.assertEqual(result[Provider.CODEX].short.used_percent, 44)
        sent = [json.loads(call.args[0]) for call in process.stdin.write.call_args_list]
        self.assertEqual([message["method"] for message in sent], [
            "initialize", "initialized", "account/rateLimits/read",
        ])
        process.kill.assert_called_once()
        process.wait.assert_awaited_once()

    async def test_errors_are_sanitized_and_process_cleaned(self):
        process = self.process([{"id": 1, "result": {}},
                                {"id": 2, "error": {"message": "private-account-data"}}])
        with patch("asyncio.create_subprocess_exec", AsyncMock(return_value=process)):
            with self.assertRaisesRegex(RuntimeError, "^Codex quota request failed$"):
                await CodexCliSource(Path("codex.exe")).collect()
        process.kill.assert_called_once()

    async def test_timeout_cleans_process(self):
        process = self.process([])
        process.stdout.readline = lambda: asyncio.sleep(10)
        with patch("asyncio.create_subprocess_exec", AsyncMock(return_value=process)):
            with self.assertRaises(TimeoutError):
                await CodexCliSource(Path("codex.exe"), timeout=0.01).collect()
        process.kill.assert_called_once()
        process.wait.assert_awaited_once()

    def test_opt_in_selection(self):
        with patch("quotaframe_bridge.sources.selection.load_config", return_value={
            "usage_source": "codex_cli", "codex_cli": "official.exe",
        }):
            source = create_usage_source(platform="win32")
            self.assertIsInstance(source, CodexCliSource)
            self.assertEqual(source.executable, Path("official.exe"))
            self.assertNotIsInstance(create_usage_source(Path("custom.exe"), platform="win32"), CodexCliSource)
