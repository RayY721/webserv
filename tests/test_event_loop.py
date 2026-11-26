"""Tests for A4: the single poll() event loop.

Until A5, webserv never answers, so send() cannot be used yet.
Instead these tests look at the connections webserv holds open, with lsof.
"""

import socket
import struct
import subprocess
import time

from helpers import HOST, webserv

CONFIG = "conf/default.conf"
PORT = 8080        # must be the same ports as "listen" in CONFIG
OTHER_PORT = 8081


def client_connections(process):
    """The client sockets webserv holds open (its listening sockets are left out)."""
    result = subprocess.run(
        ["lsof", "-a", "-p", str(process.pid), "-i", "TCP", "-n", "-P"],
        capture_output=True,
        text=True,
    )
    lines = result.stdout.splitlines()[1:]
    return [line for line in lines if "LISTEN" not in line]


def wait_for_connections(process, expected):
    """Wait until webserv holds exactly `expected` client sockets. Returns how many it holds."""
    deadline = time.monotonic() + 2
    while True:
        count = len(client_connections(process))
        if count == expected or time.monotonic() > deadline:
            return count
        time.sleep(0.05)


def connect(port):
    return socket.create_connection((HOST, port), timeout=2)


def reset(connection):
    """Close with a TCP reset (RST) instead of a normal close."""
    connection.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
    connection.close()


def test_accepts_clients_on_every_listener():
    with webserv(CONFIG, port=PORT) as process:
        first = connect(PORT)
        second = connect(OTHER_PORT)
        assert wait_for_connections(process, 2) == 2
        first.close()
        second.close()


def test_accepts_many_clients_at_once():
    with webserv(CONFIG, port=PORT) as process:
        clients = [connect(PORT) for _ in range(20)]
        assert wait_for_connections(process, 20) == 20
        for client in clients:
            client.close()


def test_client_that_closes_is_removed():
    with webserv(CONFIG, port=PORT) as process:
        client = connect(PORT)
        client.sendall(b"some bytes for the read buffer")
        assert wait_for_connections(process, 1) == 1
        client.close()
        assert wait_for_connections(process, 0) == 0
        assert process.poll() is None, "webserv stopped"


def test_client_that_resets_is_removed():
    with webserv(CONFIG, port=PORT) as process:
        client = connect(PORT)
        assert wait_for_connections(process, 1) == 1
        reset(client)
        assert wait_for_connections(process, 0) == 0
        assert process.poll() is None, "webserv stopped"


def test_other_clients_keep_working_when_one_leaves():
    with webserv(CONFIG, port=PORT) as process:
        stays = connect(PORT)
        leaves = connect(OTHER_PORT)
        assert wait_for_connections(process, 2) == 2
        leaves.close()
        assert wait_for_connections(process, 1) == 1
        newcomer = connect(OTHER_PORT)
        assert wait_for_connections(process, 2) == 2
        stays.close()
        newcomer.close()
