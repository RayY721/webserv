"""Copy this file to tests/test_<something>.py to write a new test.

Each function whose name starts with test_ is one test.
It passes if every assert is true.
Run all tests with: make && python3 tests/run_all.py

HOW TO TURN THE "DONE WHEN" OF AN ISSUE INTO TESTS

1. Split the "Done when" at each ";". Each part is usually one test.

2. Each part says "<something happens> gives <a result>".
   Find the two halves:

       "/missing gives 404"
        ^^^^^^^^       ^^^
        what to do     what to check

3. What to do -> the code that does it:

       a request (GET, POST, DELETE, a URL)  -> with webserv(CONFIG, port=PORT):
                                                    response = send("...", port=PORT)
       a request "in pieces" / "split"       -> send("first piece", "second piece", port=PORT)
       "./webserv <arguments>", a config     -> result = run_webserv("conf/...")
       that prints an error                     (see test_startup.py)

4. What to check -> one assert:

       "gives 404"                    -> assert response.status == 404
       "with Location: /new"          -> assert response.headers["location"] == "/new"
       "the page contains Index of"   -> assert b"Index of" in response.body
       "the file is gone"             -> assert not (ROOT / "www/a.txt").exists()
       "prints line 3"                -> assert "line 3" in result.stderr
       "exits with an error"          -> assert result.returncode != 0

5. If a part cannot be written like this (siege, valgrind, the browser),
   check it by hand and paste the command and its output in the PR.
   If a part is too vague to check, ask in the team chat: the "Done when" must be fixed.
"""

from helpers import ROOT, run_webserv, send, webserv

CONFIG = "conf/default.conf"
PORT = 8080   # must be the same port as "listen" in CONFIG


def test_home_page_returns_200():
    with webserv(CONFIG, port=PORT):
        response = send("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n", port=PORT)
        assert response.status == 200


def test_missing_page_returns_404():
    with webserv(CONFIG, port=PORT):
        response = send("GET /missing HTTP/1.1\r\nHost: localhost\r\n\r\n", port=PORT)
        assert response.status == 404


def test_broken_request_line_returns_400():
    with webserv(CONFIG, port=PORT):
        response = send("GET HTTP/1.1\r\nHost: localhost\r\n\r\n", port=PORT)
        assert response.status == 400


def test_request_sent_in_two_pieces():
    with webserv(CONFIG, port=PORT):
        response = send("GET / HT", "TP/1.1\r\nHost: localhost\r\n\r\n", port=PORT)
        assert response.status == 200
