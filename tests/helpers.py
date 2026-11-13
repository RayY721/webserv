"""Tools for writing tests. You do not need to change this file.

A test only needs two things:

    with webserv("conf/default.conf", port=8080):
        response = send("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n", port=8080)
        assert response.status == 200

webserv(...) starts the server, waits until it listens on the port,
and stops it at the end of the "with" block.

send(...) opens a connection, sends the text exactly as written
and returns the response, with:
    response.status   the status code, for example 404
    response.headers  a dict with the header names in lowercase
    response.body     the body, as bytes

To send a request in pieces with a pause between them:
    send("GET / HT", "TP/1.1\r\nHost: localhost\r\n\r\n", port=8080)

For tests where webserv starts and stops by itself (bad arguments, bad config):
    result = run_webserv("conf/bad.conf")
    result.returncode is the exit code, result.stdout and result.stderr what it printed.
"""

import contextlib
import pathlib
import socket
import subprocess
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent
WEBSERV = ROOT / "webserv"
HOST = "127.0.0.1"
TIMEOUT = 5


def run_webserv(*args):
    return subprocess.run(
        [str(WEBSERV), *args],
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=TIMEOUT,
    )


class Response:
    def __init__(self, status, headers, body):
        self.status = status
        self.headers = headers
        self.body = body


@contextlib.contextmanager
def webserv(config, port):
    process = subprocess.Popen(
        [str(WEBSERV), config],
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    try:
        wait_until_listening(process, port)
        yield process
    finally:
        process.terminate()
        try:
            process.wait(timeout=TIMEOUT)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def wait_until_listening(process, port):
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise AssertionError(f"webserv stopped with exit code {process.returncode}")
        try:
            socket.create_connection((HOST, port), timeout=0.2).close()
            return
        except OSError:
            time.sleep(0.1)
    raise AssertionError(f"webserv is not listening on port {port}")


def send(*pieces, port, pause=0.2):
    with socket.create_connection((HOST, port), timeout=TIMEOUT) as connection:
        for number, piece in enumerate(pieces):
            if number > 0:
                time.sleep(pause)
            connection.sendall(piece.encode())
        return read_response(connection)


def read_response(connection):
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = connection.recv(4096)
        if not chunk:
            raise AssertionError(f"connection closed before the end of the headers: {data!r}")
        data += chunk

    head, body = data.split(b"\r\n\r\n", 1)
    lines = head.decode(errors="replace").split("\r\n")
    status = int(lines[0].split(" ")[1])

    headers = {}
    for line in lines[1:]:
        name, value = line.split(":", 1)
        headers[name.strip().lower()] = value.strip()

    if "content-length" in headers:
        length = int(headers["content-length"])
        while len(body) < length:
            chunk = connection.recv(4096)
            if not chunk:
                break
            body += chunk
    else:
        while True:
            chunk = connection.recv(4096)
            if not chunk:
                break
            body += chunk

    return Response(status, headers, body)
