"""Godot's remote debugger protocol -- the editor's half of it.

Enough of core/debugger/remote_debugger_peer.cpp and core/io/marshalls.cpp to
stand in for the editor: a uint32 little-endian payload length followed by one
encode_variant()'d Array. Game -> host messages are [name, thread_id, data];
host -> game are [command, thread_id, data].

Only the Variant types the debugger actually puts on the wire are decoded. A
value of any other type raises UnsupportedVariant rather than silently
desyncing the stream, since every value after it in that message would be
misread; extending `decode` is the fix if a fixture ever needs one.
"""

import socket
import struct
import sys

MAIN_THREAD_ID = 1  # Thread::MAIN_ID

# Variant::Type, in declaration order (core/variant/variant.h).
NIL, BOOL, INT, FLOAT, STRING = 0, 1, 2, 3, 4
STRING_NAME, NODE_PATH = 21, 22
OBJECT, DICTIONARY, ARRAY = 24, 27, 28
PACKED_BYTE_ARRAY, PACKED_INT32_ARRAY = 29, 30
PACKED_STRING_ARRAY = 34

HEADER_TYPE_MASK = 0xFF
HEADER_DATA_FLAG_64 = 1 << 16
HEADER_DATA_FLAG_OBJECT_AS_ID = 1 << 16


class UnsupportedVariant(Exception):
    def __init__(self, type_id):
        super().__init__(f"no decoder for Variant type {type_id}")
        self.type_id = type_id


# ----------------------------------------------------------------- decoding


class Reader:
    def __init__(self, data):
        self.data = data
        self.pos = 0

    def u32(self):
        (v,) = struct.unpack_from("<I", self.data, self.pos)
        self.pos += 4
        return v

    def i32(self):
        (v,) = struct.unpack_from("<i", self.data, self.pos)
        self.pos += 4
        return v

    def i64(self):
        (v,) = struct.unpack_from("<q", self.data, self.pos)
        self.pos += 8
        return v

    def f32(self):
        (v,) = struct.unpack_from("<f", self.data, self.pos)
        self.pos += 4
        return v

    def f64(self):
        (v,) = struct.unpack_from("<d", self.data, self.pos)
        self.pos += 8
        return v

    def string(self):
        n = self.u32()
        raw = self.data[self.pos : self.pos + n]
        self.pos += n
        while self.pos % 4:  # padded to a 4 byte boundary
            self.pos += 1
        return raw.decode("utf-8", "replace")


def decode(r):
    header = r.u32()
    t = header & HEADER_TYPE_MASK
    if t == NIL:
        return None
    if t == BOOL:
        return r.u32() != 0
    if t == INT:
        return r.i64() if header & HEADER_DATA_FLAG_64 else r.i32()
    if t == FLOAT:
        return r.f64() if header & HEADER_DATA_FLAG_64 else r.f32()
    if t in (STRING, STRING_NAME, NODE_PATH):
        return r.string()
    if t == ARRAY:
        count = r.u32() & 0x7FFFFFFF
        return [decode(r) for _ in range(count)]
    if t == DICTIONARY:
        count = r.u32() & 0x7FFFFFFF
        out = {}
        for _ in range(count):
            k = decode(r)
            out[k] = decode(r)
        return out
    if t == PACKED_STRING_ARRAY:
        return [r.string() for _ in range(r.u32())]
    if t == PACKED_BYTE_ARRAY:
        n = r.u32()
        raw = r.data[r.pos : r.pos + n]
        r.pos += n + (-n % 4)
        return raw
    if t == PACKED_INT32_ARRAY:
        return [r.i32() for _ in range(r.u32())]
    if t == OBJECT:
        if header & HEADER_DATA_FLAG_OBJECT_AS_ID:
            return f"<Object #{r.i64()}>"
        raise NotImplementedError("full Object encoding")
    raise UnsupportedVariant(t)


# ----------------------------------------------------------------- encoding


def enc_int(v):
    if -(2**31) <= v < 2**31:
        return struct.pack("<Ii", INT, v)
    return struct.pack("<Iq", INT | HEADER_DATA_FLAG_64, v)


def enc_string(s):
    raw = s.encode("utf-8")
    return struct.pack("<II", STRING, len(raw)) + raw + b"\0" * (-len(raw) % 4)


def enc_bool(v):
    return struct.pack("<II", BOOL, 1 if v else 0)


def enc(v):
    if isinstance(v, bool):
        return enc_bool(v)
    if isinstance(v, int):
        return enc_int(v)
    if isinstance(v, str):
        return enc_string(v)
    if isinstance(v, list):
        return struct.pack("<II", ARRAY, len(v)) + b"".join(enc(x) for x in v)
    if v is None:
        return struct.pack("<I", NIL)
    raise NotImplementedError(type(v))


# ----------------------------------------------------------------- session


class Session:
    """Framed message transport over a connected socket."""

    def __init__(self, conn):
        self.conn = conn
        self.buf = b""

    def send(self, command, data=None):
        payload = enc([command, MAIN_THREAD_ID, data or []])
        self.conn.sendall(struct.pack("<I", len(payload)) + payload)

    def recv(self):
        while True:
            if len(self.buf) >= 4:
                (size,) = struct.unpack_from("<I", self.buf, 0)
                if len(self.buf) >= 4 + size:
                    payload = self.buf[4 : 4 + size]
                    self.buf = self.buf[4 + size :]
                    return decode(Reader(payload))
            chunk = self.conn.recv(65536)
            if not chunk:
                return None
            self.buf += chunk
