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


def dump_text(mode, text):
    with tempfile.NamedTemporaryFile("w", suffix=".conf") as config:
        config.write(text)
        config.flush()
        return run_dump(mode, config.name)


def dump_tokens_of(text):
    return dump_text("tokens", text)


def dump_servers_of(text):
    return dump_text("servers", text)


def assert_config_error(text, line):
    result = dump_servers_of(text)
    assert result.returncode == 1, f"accepted:\n{text}\n{result.stdout}"
    assert f"line {line}:" in result.stderr, f"{text!r}: {result.stderr}"
    return result.stderr


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


def test_default_conf_becomes_servers():
    result = run_dump("servers", CONFIG)
    assert result.returncode == 0, result.stderr
    assert result.stdout.splitlines() == [
        "server",
        "listen 127.0.0.1 8080",
        "root ../www/site-a",
        "index index.html",
        "error_page 404 ../www/errors/404.html",
        "client_max_body_size 1048576",
        "server",
        "listen 127.0.0.1 8081",
        "root ../www/site-b",
        "index index.html",
        "client_max_body_size 1048576",
    ]


def test_server_defaults_and_listen_forms():
    result = dump_servers_of(
        "server {\n"
        "    listen 80;\n"
        "    listen localhost:8080;\n"
        "    error_page 500 a.html;\n"
        "    error_page 404 b.html;\n"
        "}"
    )
    assert result.returncode == 0, result.stderr
    assert result.stdout.splitlines() == [
        "server",
        "listen 0.0.0.0 80",
        "listen localhost 8080",
        "root .",
        "index index.html",
        "error_page 404 b.html",
        "error_page 500 a.html",
        "client_max_body_size 1048576",
    ]


def test_server_without_listen_is_rejected():
    stderr = assert_config_error("\nserver {\n    root a;\n}", 2)
    assert "no listen" in stderr


def test_bad_listen_values_are_rejected():
    for value in ["0", "65536", "99999999999999999999999", "abc", "-1",
                  "1.2.3:80", "256.0.0.1:80", "127.0.0.1:", ":80", "a:80"]:
        assert_config_error(f"server {{\n    listen {value};\n}}", 2)


def test_bad_body_sizes_are_rejected():
    for value in ["0", "-1", "+5", "1M", "99999999999999999999999"]:
        assert_config_error(
            f"server {{\n    listen 80;\n    client_max_body_size {value};\n}}", 3)


def test_bad_error_pages_are_rejected():
    assert_config_error("server {\n    listen 80;\n    error_page 404;\n}", 3)
    assert_config_error("server {\n    listen 80;\n    error_page 399 a;\n}", 3)
    assert_config_error("server {\n    listen 80;\n    error_page 600 a;\n}", 3)


def test_duplicates_are_rejected():
    assert_config_error("server {\n    listen 80;\n    root a;\n    root b;\n}", 4)
    assert_config_error(
        "server {\n    listen 80;\n    error_page 404 a;\n    error_page 404 b;\n}", 4)


def test_wrong_directives_are_rejected():
    stderr = assert_config_error("server {\n    listen 80;\n    methods GET;\n}", 3)
    assert "not allowed" in stderr
    stderr = assert_config_error("server {\n    listen 80;\n    lisen 81;\n}", 3)
    assert "unknown" in stderr
    assert_config_error("server {\n    listen 80 81;\n}", 2)


def test_broken_structure_is_rejected():
    assert_config_error("server {\n    listen 80\n}", 2)
    assert_config_error("server {\n    listen 80;\n", 2)
    assert_config_error("server {\n    listen 80;\n}\n}", 4)
    assert_config_error("server\nlisten 80;", 1)
    assert_config_error("listen 80;", 1)
    assert_config_error("# only a comment\n", 1)
    assert_config_error("server {\n    listen 80;\n    location / {\n", 3)
