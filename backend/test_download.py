"""Unit tests for the direct-MCP /download path (mcp_client.py + splice_auth.download_asset).

No real network calls: mocks urllib.request.urlopen at the point mcp_client
uses it. Run with: python3 -m unittest test_download.py -v
"""

from __future__ import annotations

import json
import time
import unittest
import urllib.error
from unittest.mock import MagicMock, patch

import splice_auth
from mcp_client import MCPAuthError, MCPClient, MCPError, extract_tool_result


def _fake_response(body: dict, headers: dict | None = None):
    resp = MagicMock()
    resp.read.return_value = json.dumps(body).encode()
    resp.headers = headers or {"Content-Type": "application/json"}
    resp.__enter__.return_value = resp
    resp.__exit__.return_value = False
    return resp


class MCPClientTests(unittest.TestCase):
    @patch("mcp_client.urllib.request.urlopen")
    def test_initialize_and_call_tool_success(self, mock_urlopen):
        mock_urlopen.side_effect = [
            _fake_response({"jsonrpc": "2.0", "id": 1, "result": {}}, {"Mcp-Session-Id": "sess-1"}),
            _fake_response({"jsonrpc": "2.0", "result": {}}),  # notifications/initialized
            _fake_response(
                {
                    "jsonrpc": "2.0",
                    "id": 2,
                    "result": {
                        "structuredContent": {
                            "download_url": "https://cdn.splice.com/x.wav",
                            "file_name": "kick.wav",
                        }
                    },
                }
            ),
        ]

        client = MCPClient("https://mcp.splice.com/mcp", "token-123")
        client.initialize()
        result = client.call_tool("download_asset", {"asset_uuid": "abc"})

        self.assertEqual(client.session_id, "sess-1")
        self.assertEqual(extract_tool_result(result)["download_url"], "https://cdn.splice.com/x.wav")

    @patch("mcp_client.urllib.request.urlopen")
    def test_call_tool_is_error_raises(self, mock_urlopen):
        mock_urlopen.side_effect = [
            _fake_response({"jsonrpc": "2.0", "id": 1, "result": {}}),
            _fake_response({"jsonrpc": "2.0", "result": {}}),
            _fake_response(
                {"jsonrpc": "2.0", "id": 2, "result": {"isError": True, "content": [{"type": "text", "text": "nope"}]}}
            ),
        ]
        client = MCPClient("https://mcp.splice.com/mcp", "token-123")
        client.initialize()
        with self.assertRaises(MCPError):
            client.call_tool("download_asset", {"asset_uuid": "abc"})

    @patch("mcp_client.urllib.request.urlopen")
    def test_401_raises_mcp_auth_error(self, mock_urlopen):
        mock_urlopen.side_effect = urllib.error.HTTPError(
            "https://mcp.splice.com/mcp", 401, "Unauthorized", {}, None
        )
        # HTTPError needs a readable body for our error handler.
        mock_urlopen.side_effect.read = lambda: b"token expired"

        client = MCPClient("https://mcp.splice.com/mcp", "expired-token")
        with self.assertRaises(MCPAuthError):
            client.initialize()

    def test_extract_tool_result_falls_back_to_text_json(self):
        result = {"content": [{"type": "text", "text": '{"download_url": "https://x", "file_name": "a.wav"}'}]}
        self.assertEqual(extract_tool_result(result)["file_name"], "a.wav")

    def test_extract_tool_result_raises_when_unparseable(self):
        with self.assertRaises(MCPError):
            extract_tool_result({"content": [{"type": "text", "text": "not json"}]})

    def test_extract_tool_result_falls_back_to_url_in_prose(self):
        # Real download_asset responses are prose/markdown, not JSON --
        # confirmed live that describe_a_sound (also no output schema)
        # returns prose, and download_asset failed to JSON-parse in
        # production with a truncated error, so this is the realistic shape.
        text = (
            "Your download is ready. Download it here: "
            "https://cdn.splice.com/assets/abc123/kick.wav (link expires in 5 minutes)."
        )
        result = extract_tool_result({"content": [{"type": "text", "text": text}]})
        self.assertEqual(result["download_url"], "https://cdn.splice.com/assets/abc123/kick.wav")


class DownloadAssetTests(unittest.TestCase):
    """Tests splice_auth.download_asset's token-refresh-on-401 behavior."""

    def setUp(self):
        patcher = patch("splice_auth.get_access_token", return_value="token-1")
        self.mock_get_token = patcher.start()
        self.addCleanup(patcher.stop)

    @patch("splice_auth.MCPClient")
    def test_success_no_refresh_needed(self, mock_client_cls):
        mock_client = MagicMock()
        mock_client.call_tool.return_value = {
            "structuredContent": {"download_url": "https://cdn.splice.com/x.wav", "file_name": "kick.wav"}
        }
        mock_client_cls.return_value = mock_client

        result = splice_auth.download_asset("asset-uuid")

        self.assertEqual(result["file_name"], "kick.wav")
        mock_client.initialize.assert_called_once()

    @patch("splice_auth.refresh", return_value={"access_token": "token-2"})
    @patch("splice_auth.MCPClient")
    def test_401_triggers_refresh_and_retry(self, mock_client_cls, mock_refresh):
        first_client = MagicMock()
        first_client.call_tool.side_effect = MCPAuthError("expired")
        second_client = MagicMock()
        second_client.call_tool.return_value = {
            "structuredContent": {"download_url": "https://cdn.splice.com/x.wav", "file_name": "kick.wav"}
        }
        mock_client_cls.side_effect = [first_client, second_client]

        result = splice_auth.download_asset("asset-uuid")

        mock_refresh.assert_called_once()
        self.assertEqual(result["file_name"], "kick.wav")

    @patch("splice_auth.refresh", side_effect=RuntimeError("refresh endpoint down"))
    @patch("splice_auth.MCPClient")
    def test_401_then_failed_refresh_raises_splice_auth_error(self, mock_client_cls, mock_refresh):
        client = MagicMock()
        client.call_tool.side_effect = MCPAuthError("expired")
        mock_client_cls.return_value = client

        with self.assertRaises(splice_auth.SpliceAuthError):
            splice_auth.download_asset("asset-uuid")

    @patch("splice_auth.refresh", return_value={"access_token": "token-2"})
    @patch("splice_auth.MCPClient")
    def test_still_unauthorized_after_refresh_raises_splice_auth_error(self, mock_client_cls, mock_refresh):
        first_client = MagicMock()
        first_client.call_tool.side_effect = MCPAuthError("expired")
        second_client = MagicMock()
        second_client.call_tool.side_effect = MCPAuthError("still expired")
        mock_client_cls.side_effect = [first_client, second_client]

        with self.assertRaises(splice_auth.SpliceAuthError):
            splice_auth.download_asset("asset-uuid")


if __name__ == "__main__":
    unittest.main()
