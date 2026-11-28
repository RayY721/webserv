"""Checks on the request parser (B2-B5), through the request_dump test helper.
Every expected answer here is what NGINX 1.24 gives (checked with tests/compare_nginx.py)."""

import subprocess
import tempfile

from helpers import ROOT, TIMEOUT

DUMP = ROOT / "request_dump"


def parse(raw):
    """Gives the request bytes to request_dump and returns what it printed, as lines."""
    subprocess.run(["make", "-s", "request_dump"], cwd=ROOT, check=True, timeout=60)
    with tempfile.NamedTemporaryFile("wb") as request:
        request.write(raw)
        request.flush()
        result = subprocess.run([str(DUMP), request.name], cwd=ROOT,
                                capture_output=True, text=True, timeout=TIMEOUT)
    assert result.returncode == 0, result.stderr
    return result.stdout.splitlines()


def test_good_request_is_split_into_its_parts():
    lines = parse(b"GET /a?x=1 HTTP/1.1\r\nHost: localhost\r\nUser-Agent: curl\r\n\r\n")
    assert lines == [
        "done",
        "method Get",
        "path /a",
        "query x=1",
        "version HTTP/1.1",
        "header host: localhost",
        "header user-agent: curl",
        "left 0",
    ]


def test_body_stays_in_the_buffer():
    lines = parse(b"POST / HTTP/1.1\r\nHost: a\r\nContent-Length: 9\r\n\r\nname=Luis")
    assert lines[0] == "done"
    assert lines[-1] == "left 9"


def test_head_without_empty_line_needs_more():
    assert parse(b"GET / HTTP/1.1\r\nHost: a\r\n") == ["need_more"]


def test_accepted_like_nginx():
    for raw in [
        b"GET / HTTP/1.1\nHost: a\n\n",                     # lines ending with \n alone
        b"\r\n\r\nGET / HTTP/1.1\r\nHost: a\r\n\r\n",       # empty lines before
        b"GET   /   HTTP/1.1 \r\nHost: a\r\n\r\n",          # several spaces, one at the end
        b"GET / HTTP/1.0\r\n\r\n",                          # HTTP/1.0 needs no Host
        b"GET / HTTP/1.5\r\nHost: a\r\n\r\n",               # any HTTP/1.x
        b"GET http://a/b HTTP/1.1\r\nHost: a\r\n\r\n",      # full URL
        b"GET / HTTP/1.1\r\nHost: a\r\nBroken\r\n\r\n",     # line without ':': header with empty value
        b"GET / HTTP/1.1\r\nHost: a\r\nX(y: 1\r\n\r\n",     # odd name is ignored
        b"GET / HTTP/1.1\r\nHost: a\r\nX: a\x01b\r\n\r\n",  # control char in a value
		b"GET http://a:80/x HTTP/1.1\r\nHost: a\r\n\r\n",   # full URL with a port
        b"GET http://[::1]/x HTTP/1.1\r\nHost: a\r\n\r\n",  # full URL with an IPv6 address
        b"GET h2://a/x HTTP/1.1\r\nHost: a\r\n\r\n",        # scheme with a digit
        b"GET / HTTP/1.0999\r\nHost: a\r\n\r\n",            # minor 999 (leading zeros do not count)
        b"GET / HTTP/1.00\r\n\r\n",                         # HTTP/1.00 is HTTP/1.0: no Host needed
        b"GET / HTTP/1.1\r\nHost: a@b\r\n\r\n",             # the Host header accepts '@'
    ]:
        assert parse(raw)[0] == "done", raw


def test_full_url_keeps_only_the_path():
    lines = parse(b"GET http://a/b?x=1 HTTP/1.1\r\nHost: a\r\n\r\n")
    assert "path /b" in lines and "query x=1" in lines

def test_path_and_query_like_nginx():
    for target, path, query in [
        (b"http://a?x=/b", "/", "x=/b"),    # the host ends at '?' too
        (b"http://a?x=1", "/", "x=1"),
        (b"http://a/p?x=1", "/p", "x=1"),
        (b"/a#b", "/a", ""),                # everything from '#' on is dropped
        (b"/a?x#y", "/a", "x"),
        (b"/a#b?c", "/a", ""),
    ]:
        lines = parse(b"GET " + target + b" HTTP/1.1\r\nHost: a\r\n\r\n")
        assert f"path {path}" in lines and f"query {query}" in lines, target


def test_version_is_kept_as_1_0_or_1_1():
    assert "version HTTP/1.0" in parse(b"GET / HTTP/1.00\r\n\r\n")
    assert "version HTTP/1.1" in parse(b"GET / HTTP/1.5\r\nHost: a\r\n\r\n")

def test_400():
    for raw in [
        b"GARBAGE\r\n\r\n",
        b" GET / HTTP/1.1\r\nHost: a\r\n\r\n",              # space at the start
        b"GET / HTTP/1.1 x\r\nHost: a\r\n\r\n",             # 4 parts
        b"get / HTTP/1.1\r\nHost: a\r\n\r\n",               # lowercase method
        b"G3T / HTTP/1.1\r\nHost: a\r\n\r\n",               # digit in the method
        b"GET index.html HTTP/1.1\r\nHost: a\r\n\r\n",      # path without /
        b"GET * HTTP/1.1\r\nHost: a\r\n\r\n",
        b"GET / HTTP/0.9\r\nHost: a\r\n\r\n",
        b"GET / FOO\r\nHost: a\r\n\r\n",
        b"GET /a\tb HTTP/1.1\r\nHost: a\r\n\r\n",           # tab in the first line
        b"GET /a\0.py HTTP/1.1\r\nHost: a\r\n\r\n",         # \0 in the first line
        b"GET / HTTP/1.1\r\n\r\n",                           # HTTP/1.1 without Host
        b"GET / HTTP/1.1\r\nHost:\r\n\r\n",                  # empty Host
        b"GET / HTTP/1.1\r\nHost: a/b\r\n\r\n",              # '/' in Host
        b"GET / HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n",     # Host twice
        b"GET / HTTP/1.1\r\nHost : a\r\n\r\n",               # space before ':'
        b"GET / HTTP/1.1\r\nHost: a\r\nX: a\0b\r\n\r\n",     # \0 in a value
        b"GET / HTTP/1.1\r\nHost: a\r\nX: " + b"a" * 9000 + b"\r\n\r\n",  # header over 8 KB
		b"GET http://?x HTTP/1.1\r\nHost: a\r\n\r\n",         # full URL with an empty host
        b"GET http://a@b/ HTTP/1.1\r\nHost: a\r\n\r\n",       # '@' in the host of a full URL
        b"GET http://a#b HTTP/1.1\r\nHost: a\r\n\r\n",        # '#' in the host of a full URL
        b"GET http://a:8x/x HTTP/1.1\r\nHost: a\r\n\r\n",     # port that is not a number
        b"GET http://[::1/x HTTP/1.1\r\nHost: a\r\n\r\n",     # IPv6 address without ']'
        b"GET h_t://a/x HTTP/1.1\r\nHost: a\r\n\r\n",         # '_' in the scheme
        b"GET / HTTP/01.1\r\nHost: a\r\n\r\n",                # major starting with 0
        b"GET / HTTP/1.1000\r\nHost: a\r\n\r\n",              # minor over 999
        b"GET / HTTP/1.01\r\n\r\n",                         # HTTP/1.01 is HTTP/1.1: needs Host
        b"GET / HTTP/1.1\r\nHost: .\r\n\r\n",
        b"GET / HTTP/1.1\r\nHost: a b\r\n\r\n",              # space inside Host
        b"GET / HTTP/1.1\r\nHost: a\tb\r\n\r\n",             # tab inside Host
        b"GET / HTTP/1.0\r\nHost\r\n\r\n",                   # 'Host' without ':' is an empty Host
        b"GET / HTTP/1.1\r\nhost\r\nHost: a\r\n\r\n",        # ... and counts as a second Host
        b"GET / HTTP/1.1\r\nHost: a\r\nfoo bar\r\n\r\n",     # line without ':' with a space
    ]:
        assert parse(raw) == ["error 400"], raw


def test_414():
    assert parse(b"GET /" + b"a" * 9000 + b" HTTP/1.1\r\nHost: a\r\n\r\n") == ["error 414"]


def test_505():
    assert parse(b"GET / HTTP/2.0\r\nHost: a\r\n\r\n") == ["error 505"]
    assert parse(b"GET / HTTP/2.0\r\nbroken\r\n\r\n") == ["error 505"]   # before the headers


def test_405():
    assert parse(b"FOO / HTTP/1.1\r\nHost: a\r\n\r\n") == ["error 405"]
    assert parse(b"FOO / HTTP/1.1\r\n\r\n") == ["error 400"]             # Host is checked first
