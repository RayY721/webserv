#!/usr/bin/env python3
"""Sends the same raw requests to a running NGINX and to our parser (request_dump),
and shows what each answers, so every "like NGINX" rule is checked against the real thing.

    make request_dump
    python3 tests/compare_nginx.py            # NGINX on 127.0.0.1:80
    python3 tests/compare_nginx.py 8088       # or another port

Only the parser's job is compared: whether the request is refused, and with which code.
When the parser accepts a request ("done"), NGINX can answer 200, 404, 403... that is fine:
those codes come from later steps (B6 and after), not from the parser.
"""

import socket
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DUMP = ROOT / "request_dump"
PARSER_CODES = {400, 405, 414, 431, 501, 505}

CASES = [
    ("good request", b"GET / HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("lines ending with \\n alone", b"GET / HTTP/1.1\nHost: a\n\n"),
    ("empty lines before the request", b"\r\n\r\nGET / HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("HTTP/1.0 without Host", b"GET / HTTP/1.0\r\n\r\n"),
    ("HTTP/1.5", b"GET / HTTP/1.5\r\nHost: a\r\n\r\n"),
    ("HTTP/2.0", b"GET / HTTP/2.0\r\nHost: a\r\n\r\n"),
    ("HTTP/0.9", b"GET / HTTP/0.9\r\nHost: a\r\n\r\n"),
    ("HTTP/2.0 + broken header", b"GET / HTTP/2.0\r\nbroken\r\n\r\n"),
    ("version FOO", b"GET / FOO\r\nHost: a\r\n\r\n"),
    ("version http/1.1", b"GET / http/1.1\r\nHost: a\r\n\r\n"),
    ("GARBAGE", b"GARBAGE\r\n\r\n"),
    ("two spaces", b"GET  / HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("path without /", b"GET index.html HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("absolute URL", b"GET http://a/ HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("FOO method", b"FOO / HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("PUT method", b"PUT / HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("get (lowercase)", b"get / HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("G3T", b"G3T / HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("FOO without Host", b"FOO / HTTP/1.1\r\n\r\n"),
    ("tab in the path", b"GET /a\tb HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("\\0 in the path", b"GET /a\0.py HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("\\x01 in the path", b"GET /a\x01 HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("HTTP/1.1 without Host", b"GET / HTTP/1.1\r\n\r\n"),
    ("empty Host", b"GET / HTTP/1.1\r\nHost:\r\n\r\n"),
    ("Host twice", b"GET / HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n"),
    ("Content-Length twice", b"POST / HTTP/1.1\r\nHost: a\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n"),
    ("header without ':'", b"GET / HTTP/1.1\r\nHost: a\r\nBroken\r\n\r\n"),
    ("space before ':'", b"GET / HTTP/1.1\r\nHost : a\r\n\r\n"),
    ("'(' in a header name", b"GET / HTTP/1.1\r\nHost: a\r\nX(y: 1\r\n\r\n"),
    ("\\r inside a header value", b"GET / HTTP/1.1\r\nHost: a\r\nX: a\rb\r\n\r\n"),
    ("tab inside a header value", b"GET / HTTP/1.1\r\nHost: a\r\nX: a\tb\r\n\r\n"),
    ("first line of 9000 bytes", b"GET /" + b"a" * 9000 + b" HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("leading space", b" GET / HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("trailing space", b"GET / HTTP/1.1 \r\nHost: a\r\n\r\n"),
    ("4 parts", b"GET / HTTP/1.1 x\r\nHost: a\r\n\r\n"),
    ("HTTP/1.10", b"GET / HTTP/1.10\r\nHost: a\r\n\r\n"),
    ("HTTP/1.1000", b"GET / HTTP/1.1000\r\nHost: a\r\n\r\n"),
    ("HTTP/12.1", b"GET / HTTP/12.1\r\nHost: a\r\n\r\n"),
    ("HTTP/1", b"GET / HTTP/1\r\nHost: a\r\n\r\n"),
    ("absolute URL with path", b"GET http://a/b?x=1 HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("http:// without host", b"GET http:// HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("http:///x", b"GET http:///x HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("1x://a/", b"GET 1x://a/ HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("target *", b"GET * HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("GET_X method", b"GET_X / HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("FOO with HTTP/1.0", b"FOO / HTTP/1.0\r\n\r\n"),
    ("\\r in the first line", b"GET /a\rb HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("':v' (empty name)", b"GET / HTTP/1.1\r\nHost: a\r\n: v\r\n\r\n"),
    ("'_' in a header name", b"GET / HTTP/1.1\r\nHost: a\r\nX_Y: 1\r\n\r\n"),
    ("'.' in a header name", b"GET / HTTP/1.1\r\nHost: a\r\nX.Y: 1\r\n\r\n"),
    ("tab before ':'", b"GET / HTTP/1.1\r\nHost: a\r\nX\t: 1\r\n\r\n"),
    ("line starting with a space", b"GET / HTTP/1.1\r\nHost: a\r\n X: 1\r\n\r\n"),
    ("\\x01 in a header name", b"GET / HTTP/1.1\r\nHost: a\r\nX\x01Y: 1\r\n\r\n"),
    ("\\x01 in a header value", b"GET / HTTP/1.1\r\nHost: a\r\nX: a\x01b\r\n\r\n"),
    ("\\x7f in a header value", b"GET / HTTP/1.1\r\nHost: a\r\nX: a\x7fb\r\n\r\n"),
    ("\\0 in a header value", b"GET / HTTP/1.1\r\nHost: a\r\nX: a\0b\r\n\r\n"),
    ("Host with only spaces", b"GET / HTTP/1.1\r\nHost:   \r\n\r\n"),
    ("empty Host in HTTP/1.0", b"GET / HTTP/1.0\r\nHost:\r\n\r\n"),
    ("Host a/b", b"GET / HTTP/1.1\r\nHost: a/b\r\n\r\n"),
    ("Host a..b", b"GET / HTTP/1.1\r\nHost: a..b\r\n\r\n"),
    ("Transfer-Encoding twice", b"POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n"),
    ("Accept twice", b"GET / HTTP/1.1\r\nHost: a\r\nAccept: x\r\nAccept: y\r\n\r\n"),
    ("header line of 9000 bytes", b"GET / HTTP/1.1\r\nHost: a\r\nX: " + b"a" * 9000 + b"\r\n\r\n"),
    ("head of 40 KB", b"GET / HTTP/1.1\r\nHost: a\r\n" + b"".join(b"X%d: %s\r\n" % (i, b"0" * 100) for i in range(400)) + b"\r\n"),
	    # Added after the B4/B5 review: full URLs, versions, Host and lines without ':'.
    ("full URL, '?' before any '/'", b"GET http://a?x=/b HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("http://?x (empty host)", b"GET http://?x HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("http://:80/x (empty host)", b"GET http://:80/x HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("http://a:80/x", b"GET http://a:80/x HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("http://a:8x/x (bad port)", b"GET http://a:8x/x HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("http://a:80:80/x", b"GET http://a:80:80/x HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("http://[::1]:80/x", b"GET http://[::1]:80/x HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("http://[::1/x (no ']')", b"GET http://[::1/x HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("http://[a/b]/x", b"GET http://[a/b]/x HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("http://a..b/x", b"GET http://a..b/x HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("scheme h2", b"GET h2://a/x HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("scheme ht-tp", b"GET ht-tp://a/x HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("scheme h_t", b"GET h_t://a/x HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("http:/a/x", b"GET http:/a/x HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("'#' in the path", b"GET /a#b?c HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("'#' in the host of a full URL", b"GET http://a#b HTTP/1.1\r\nHost: a\r\n\r\n"),
    ("HTTP/01.1", b"GET / HTTP/01.1\r\nHost: a\r\n\r\n"),
    ("HTTP/02.0", b"GET / HTTP/02.0\r\nHost: a\r\n\r\n"),
    ("HTTP/10.0", b"GET / HTTP/10.0\r\nHost: a\r\n\r\n"),
    ("HTTP/1.0999", b"GET / HTTP/1.0999\r\nHost: a\r\n\r\n"),
    ("HTTP/1.00 without Host", b"GET / HTTP/1.00\r\n\r\n"),
    ("HTTP/1.01 without Host", b"GET / HTTP/1.01\r\n\r\n"),
    ("Host .", b"GET / HTTP/1.1\r\nHost: .\r\n\r\n"),
    ("Host a b", b"GET / HTTP/1.1\r\nHost: a b\r\n\r\n"),
    ("Host with a tab inside", b"GET / HTTP/1.1\r\nHost: a\tb\r\n\r\n"),
    ("Host with \\x01", b"GET / HTTP/1.1\r\nHost: a\x01b\r\n\r\n"),
    ("Host with \\x80", b"GET / HTTP/1.1\r\nHost: a\x80b\r\n\r\n"),
    ("'Host' without ':' (HTTP/1.0)", b"GET / HTTP/1.0\r\nHost\r\n\r\n"),
    ("'host' without ':' + Host: a", b"GET / HTTP/1.1\r\nhost\r\nHost: a\r\n\r\n"),
    ("'foo bar' without ':'", b"GET / HTTP/1.1\r\nHost: a\r\nfoo bar\r\n\r\n"),
    ("'f@o' without ':'", b"GET / HTTP/1.1\r\nHost: a\r\nf@o\r\n\r\n"),
]

# Every printable character inside the host of a full URL and inside the Host header,
# so no character is left untested.
for code in range(33, 127):
    char = bytes([code])
    CASES.append((f"URL host a{chr(code)}b", b"GET http://a" + char + b"b/x HTTP/1.1\r\nHost: z\r\n\r\n"))
    CASES.append((f"Host a{chr(code)}b", b"GET / HTTP/1.1\r\nHost: a" + char + b"b\r\n\r\n"))

def nginx_status(raw, port):
    with socket.create_connection(("127.0.0.1", port), timeout=3) as connection:
        connection.sendall(raw)
        data = b""
        while b"\r\n" not in data:
            chunk = connection.recv(4096)
            if not chunk:
                break
            data += chunk
    first = data.split(b"\r\n", 1)[0].split()
    return int(first[1]) if len(first) > 1 and first[1].isdigit() else None


def parser_answer(raw):
    with tempfile.NamedTemporaryFile("wb") as request:
        request.write(raw)
        request.flush()
        out = subprocess.run([str(DUMP), request.name], capture_output=True, text=True, errors="replace").stdout
    first = out.splitlines()[0]
    return "done" if first == "done" else int(first.split()[1]) if first.startswith("error") else first


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 80
    differences = 0
    print(f"{'case':34} {'NGINX':>6}  {'ours':>6}")
    for name, raw in CASES:
        theirs = nginx_status(raw, port)
        ours = parser_answer(raw)
        same = (ours == "done" and theirs not in PARSER_CODES) or ours == theirs
        differences += not same
        print(f"{name:34} {str(theirs):>6}  {str(ours):>6}  {'' if same else '<-- different'}")
    print(f"\n{differences} different")
    return 1 if differences else 0


if __name__ == "__main__":
    sys.exit(main())