"""Checks on how webserv starts. Run `make` before running the tests."""

from helpers import WEBSERV, run_webserv


def test_binary_exists():
    assert WEBSERV.exists(), "webserv not found: run make first"


def test_too_many_arguments_prints_usage():
    result = run_webserv("first.conf", "second.conf")
    assert result.returncode == 1, f"expected exit code 1, got {result.returncode}"
    assert "Usage" in result.stderr, "expected the usage message on stderr"
