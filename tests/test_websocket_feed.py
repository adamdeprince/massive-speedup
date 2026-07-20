"""Synthetic wire tests for the native WebSocket client.

Every protocol record in this file is invented. The tests never contact a
market-data service and never use captured market data.
"""

from __future__ import annotations

import base64
import hashlib
import json
import socket
import struct
import threading
from collections.abc import Iterator
from types import TracebackType

import pytest

from massive_speedup import WebSocket


class SyntheticWebSocketServer:
    def __init__(self, frames: list[str]) -> None:
        self.frames = frames
        self.requests: list[dict[str, str]] = []
        self._listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._listener.bind(("127.0.0.1", 0))
        self._listener.listen(1)
        self._listener.settimeout(5)
        self._thread = threading.Thread(target=self._serve, daemon=True)
        self._connection: socket.socket | None = None
        self._error: BaseException | None = None

    @property
    def url(self) -> str:
        host, port = self._listener.getsockname()
        return f"ws://{host}:{port}"

    def __enter__(self) -> SyntheticWebSocketServer:
        self._thread.start()
        return self

    def __exit__(
        self,
        exception_type: type[BaseException] | None,
        exception: BaseException | None,
        traceback: TracebackType | None,
    ) -> None:
        del exception_type, exception, traceback
        if self._connection is not None:
            self._connection.close()
        self._listener.close()
        self._thread.join(timeout=5)
        if self._thread.is_alive():
            raise AssertionError("synthetic WebSocket server did not stop")
        if self._error is not None:
            raise self._error

    def _serve(self) -> None:
        try:
            connection, _ = self._listener.accept()
            self._connection = connection
            connection.settimeout(5)
            request = self._receive_until(connection, b"\r\n\r\n")
            headers = self._request_headers(request)
            accept = base64.b64encode(
                hashlib.sha1(
                    (headers["sec-websocket-key"] + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode(
                        "ascii"
                    )
                ).digest()
            ).decode("ascii")
            connection.sendall(
                (
                    "HTTP/1.1 101 Switching Protocols\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: Upgrade\r\n"
                    f"Sec-WebSocket-Accept: {accept}\r\n"
                    "\r\n"
                ).encode("ascii")
            )

            self._send_text(
                connection,
                '[{"ev":"status","status":"connected","message":"synthetic connection"}]',
            )
            self.requests.append(json.loads(self._receive_text(connection)))
            self._send_text(
                connection,
                '[{"ev":"status","status":"auth_success","message":"synthetic authentication"}]',
            )
            self.requests.append(json.loads(self._receive_text(connection)))

            for frame in self.frames:
                self._send_text(connection, frame)
            connection.sendall(b"\x88\x02\x03\xe8")
        except BaseException as error:
            self._error = error

    @staticmethod
    def _receive_until(connection: socket.socket, marker: bytes) -> bytes:
        data = bytearray()
        while marker not in data:
            chunk = connection.recv(4096)
            if not chunk:
                raise ConnectionError("client closed during WebSocket handshake")
            data.extend(chunk)
        return bytes(data)

    @staticmethod
    def _request_headers(request: bytes) -> dict[str, str]:
        lines = request.decode("ascii").split("\r\n")
        return {
            name.strip().lower(): value.strip()
            for line in lines[1:]
            if ":" in line
            for name, value in [line.split(":", 1)]
        }

    @staticmethod
    def _receive_exact(connection: socket.socket, length: int) -> bytes:
        data = bytearray()
        while len(data) < length:
            chunk = connection.recv(length - len(data))
            if not chunk:
                raise ConnectionError("client closed during WebSocket frame")
            data.extend(chunk)
        return bytes(data)

    @classmethod
    def _receive_frame(cls, connection: socket.socket) -> tuple[int, bytes]:
        first, second = cls._receive_exact(connection, 2)
        length = second & 0x7F
        if length == 126:
            length = struct.unpack("!H", cls._receive_exact(connection, 2))[0]
        elif length == 127:
            length = struct.unpack("!Q", cls._receive_exact(connection, 8))[0]
        if not second & 0x80:
            raise AssertionError("client WebSocket frame was not masked")
        mask = cls._receive_exact(connection, 4)
        payload = cls._receive_exact(connection, length)
        decoded = bytes(value ^ mask[index % 4] for index, value in enumerate(payload))
        return first & 0x0F, decoded

    @classmethod
    def _receive_text(cls, connection: socket.socket) -> str:
        while True:
            opcode, payload = cls._receive_frame(connection)
            if opcode == 1:
                return payload.decode()
            if opcode == 9:
                connection.sendall(bytes((0x8A, len(payload))) + payload)
                continue
            raise AssertionError(f"expected a text frame, received opcode {opcode}")

    @staticmethod
    def _send_text(connection: socket.socket, text: str) -> None:
        payload = text.encode()
        if len(payload) < 126:
            header = bytes((0x81, len(payload)))
        elif len(payload) <= 0xFFFF:
            header = b"\x81\x7e" + struct.pack("!H", len(payload))
        else:
            header = b"\x81\x7f" + struct.pack("!Q", len(payload))
        connection.sendall(header + payload)


def synthetic_stock_feed(*frames: str) -> Iterator[object]:
    if not hasattr(WebSocket.Stocks, "connect"):
        pytest.skip("native WebSocket feed is not available")
    with SyntheticWebSocketServer(list(frames)) as server:
        with WebSocket.Stocks.connect(
            "T.ZZTEST,Q.ZZTEST",
            api_key="synthetic-key",
            url=server.url,
            timeout=2,
            reconnect=False,
        ) as feed:
            assert feed.asset_class == "stocks"
            assert feed.subscriptions == "T.ZZTEST,Q.ZZTEST"
            assert feed.url == server.url
            yield from feed
        assert server.requests == [
            {"action": "auth", "params": "synthetic-key"},
            {"action": "subscribe", "params": "T.ZZTEST,Q.ZZTEST"},
        ]


def test_native_websocket_feed_authenticates_subscribes_and_parses() -> None:
    messages = list(synthetic_stock_feed('[{"ev":"T","sym":"ZZTEST","p":10.25,"s":2,"t":1100}]'))

    assert len(messages) == 3
    assert [message[0].event_type for message in messages] == [
        "status",
        "status",
        "T",
    ]
    assert isinstance(messages[-1][0], WebSocket.Stocks.Trade)
    assert messages[-1][0].ticker == "ZZTEST"
    assert messages[-1][0].price == pytest.approx(10.25)


def test_native_websocket_feed_drives_market_without_python_frame_iteration() -> None:
    class Endpoint:
        def buy(self, quantity: int, symbol: str | None = None) -> None:
            del quantity, symbol

        def sell(self, quantity: int, symbol: str | None = None) -> None:
            del quantity, symbol

    frames = [
        '[{"ev":"Q","sym":"ZZTEST","bp":10.0,"ap":11.0,"t":1000},'
        '{"ev":"T","sym":"ZZTEST","p":10.0,"s":1,"t":1100},'
        '{"ev":"T","sym":"ZZTEST","p":11.0,"s":1,"t":1200}]'
    ]

    with SyntheticWebSocketServer(frames) as server:
        with WebSocket.Stocks.connect(
            "T.ZZTEST,Q.ZZTEST",
            api_key="synthetic-key",
            url=server.url,
            timeout=2,
            reconnect=False,
        ) as feed:
            endpoint = Endpoint()
            rows = list(WebSocket.Stocks.market(feed, endpoint, fast=True))

        assert server.requests[-1] == {
            "action": "subscribe",
            "params": "T.ZZTEST,Q.ZZTEST",
        }

    assert len(rows) == 2
    assert all(row[0] == "ZZTEST" for row in rows)
    assert [row[2].price for row in rows] == [10.0, 11.0]
    assert rows[0][5]["ZZTEST"].bid_price == pytest.approx(10.0)
    assert rows[0][5] is rows[1][5]
    assert all(row[6] is endpoint for row in rows)
