"""Checks on the config layer, through the config_dump test helper."""

import subprocess
import tempfile

from helpers import ROOT, TIMEOUT

DUMP = ROOT / "config_dump"
CONFIG = "conf/default.conf"


def run_dump(*args):
    subprocess.run(["make", "-s", "config_dump"], cwd=ROOT, check=True, timeout=60)
    return subprocess.run(
        [str(DUMP), *args],
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=TIMEOUT,
    )


def dump_tokens_of(text):
    with tempfile.NamedTemporaryFile("w", suffix=".conf") as config:
        config.write(text)
        config.flush()
        return run_dump("tokens", config.name)


def test_default_conf_becomes_tokens_with_line_numbers():
    result = run_dump("tokens", CONFIG)
    assert result.returncode == 0, result.stderr
    lines = result.stdout.splitlines()
    assert lines[0] == "5\tWord\tserver", "comments must be skipped"
    assert lines[1] == "5\tOpenBrace\t{"
    assert "6\tWord\t127.0.0.1:8080" in lines
    assert "6\tSemicolon\t;" in lines
    assert lines[-1].endswith("\tCloseBrace\t}")


def test_symbols_are_split_from_words():
    result = dump_tokens_of("server{listen 80;}")
    assert result.stdout.splitlines() == [
        "1\tWord\tserver",
        "1\tOpenBrace\t{",
        "1\tWord\tlisten",
        "1\tWord\t80",
        "1\tSemicolon\t;",
        "1\tCloseBrace\t}",
    ]


def test_comment_ends_at_newline():
    result = dump_tokens_of("root a; # root b;\nindex c;")
    assert result.stdout.splitlines() == [
        "1\tWord\troot",
        "1\tWord\ta",
        "1\tSemicolon\t;",
        "2\tWord\tindex",
        "2\tWord\tc",
        "2\tSemicolon\t;",
    ]


def test_quote_is_rejected_with_its_line():
    result = dump_tokens_of('server {\n    root "my site";\n}')
    assert result.returncode == 1
    assert "line 2" in result.stderr
