# Tests

How to run the tests of webserv, and how to write new ones.
You do not need to know Python: you copy `TEMPLATE.py` and change two lines.

Why we have tests: the subject asks the server to *"remain operational at all times"* and suggests writing tests *"in a more suitable language, such as Python"* (page 11). Each check is written once and runs again before every PR (from sprint 3), so we see at once when a change breaks something that worked.

---

## 1. How to run the tests

In the root of the repo:

```bash
make && python3 tests/run_all.py
```

What you see today:

```
PASS  test_startup.py::test_binary_exists
PASS  test_startup.py::test_too_many_arguments_prints_usage

2 passed, 0 failed
```

- `PASS` = the test worked.
- `FAIL` = something is wrong. Below it you see which line failed.

**From sprint 3 (25 Nov)**, run this **before every PR**. Everything must say PASS, also the tests of the other layers. Before sprint 3 there is nothing to run yet: the server cannot answer requests (section 7).

---

## 2. What is in this folder

| File | What it is | Do you edit it? |
| --- | --- | --- |
| `tests/run_all.py` | Runs all the tests and prints PASS or FAIL | Never |
| `tests/helpers.py` | The tools a test uses (section 4) | Never |
| `tests/TEMPLATE.py` | A model test file. You copy it to write your tests | Never: you copy it |
| `tests/test_startup.py` | The first two tests (the binary exists; too many arguments prints the usage) | No |
| `tests/README.md` | This guide | Only to improve it |

`TEMPLATE.py` is never run, because its name does not start with `test_`.

---

## 3. The Python you need (only this)

A test looks like this:

```python
def test_missing_page_returns_404():
    with webserv(CONFIG, port=PORT):
        response = send("GET /missing HTTP/1.1\r\nHost: localhost\r\n\r\n", port=PORT)
        assert response.status == 404
```

Read it as: *start webserv; send this request; the answer must be 404.*

Each piece:

| You see | It means |
| --- | --- |
| `def test_missing_page_returns_404():` | The start of one test. The name must start with `test_` and say what it checks |
| 4 spaces at the start of a line | "This line belongs to the block above". Python uses spaces instead of `{ }` |
| `with webserv(CONFIG, port=PORT):` | "Start webserv, run the lines below, then stop it". It stops even if the test fails |
| `response = send(...)` | Send a request and keep the answer in `response` |
| `"GET /missing HTTP/1.1\r\nHost: localhost\r\n\r\n"` | The request, as text. `\r\n` ends each line in HTTP. `\r\n\r\n` (an empty line) ends the headers. **Without it, the server keeps waiting** |
| `assert A == B` | "A must be equal to B". If not, the test shows FAIL. `!=` means "different". `in` means "is inside" |
| `b"some text"` | Text as bytes. Use it to compare with `response.body` |
| `# some text` | A comment. Python ignores it |

Want to see a real request? Terminal 1: `nc -l 8080`. Terminal 2: `curl localhost:8080/hello`. Terminal 1 shows exactly what curl sent.

---

## 4. The tools in `helpers.py`

**To test a request** (the server must stay on and answer):

```python
with webserv(CONFIG, port=PORT):
    response = send("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n", port=PORT)
```

Then you can check:

| Write | You get | Example |
| --- | --- | --- |
| `response.status` | The status code | `assert response.status == 201` |
| `response.headers["name"]` | A header. **Name always in lowercase** | `assert response.headers["location"] == "/new"` |
| `response.body` | The body, as bytes | `assert b"Index of" in response.body` |

To send a request **in pieces**, with a short pause between them, give `send` several texts:

```python
response = send("GET / HT", "TP/1.1\r\nHost: localhost\r\n\r\n", port=PORT)
```

**To test how webserv starts** (it starts, prints something and stops by itself, for example with a bad config):

```python
result = run_webserv("conf/tests/typo.conf")
assert result.returncode == 1          # the exit code
assert "line 3" in result.stderr       # what it printed as an error
```

**Which one?**
- The server must stay on and answer → `with webserv(...)` + `send(...)`.
- The program ends by itself → `run_webserv(...)`.

`CONFIG` and `PORT` are written once, at the top of the test file:

```python
CONFIG = "conf/default.conf"
PORT = 8080   # must be the same port as "listen" in CONFIG
```

---

## 5. From "Done when" to tests: the recipe

The tests of an issue come from its **"Done when"** in the team backlog. The "Done when" says *what* must work. The test is the code that *checks* it.

Every issue is done in the same 4 steps:

**Step 1 · Split the "Done when"** at each `;` and each "and". Each part is one test.

**Step 2 · In each part, find two things:** what you do, and what you check.

```
/missing   gives   404
└ what you do      └ what you check
```

**Step 3 · "What you do" tells you the tool:**

| The part talks about… | You write |
| --- | --- |
| A URL, a request, GET, POST, DELETE | `with webserv(...)` + `response = send("...", port=PORT)` |
| A request "in pieces" or "split" | `send("piece 1", "piece 2", port=PORT)` |
| Starting webserv with arguments, or a bad config | `result = run_webserv(...)` |

**Step 4 · "What you check" becomes one `assert`:**

| The part says… | You write |
| --- | --- |
| "gives 404" | `assert response.status == 404` |
| "with `Location: /new`" | `assert response.headers["location"] == "/new"` |
| "the page contains Index of" | `assert b"Index of" in response.body` |
| "the file is gone" | `assert not (ROOT / "www/a.txt").exists()` |
| "prints line 3" | `assert "line 3" in result.stderr` |

**If a part does not fit any row:**
- If it can only be checked by hand (siege, valgrind, the browser): do it by hand and paste the command and its result in the PR.
- If it is too vague to check: ask in the team chat. The "Done when" must be fixed, not guessed.

The same recipe is written at the top of `tests/TEMPLATE.py`.

---

## 6. Full example: the tests of B6b

**The issue.** B6b · Serve static files with MIME types.

**Its "Done when":**
> `/index.html` gives 200, `Content-Type: text/html`, an exact `Content-Length` and `Connection: close`; CSS and images load in the browser; a file without read permission gives 403

### Step 1 · Copy the template

```bash
cp tests/TEMPLATE.py tests/test_static_files.py
```

### Step 2 · Clean the copy

In `tests/test_static_files.py`:

1. Replace the long text between `"""` at the top with one line: `"""Tests for B6b: serve static files with MIME types."""`
2. Delete the 4 example tests (each `def test_...` and its lines).
3. Keep the `from helpers import ...` line, `CONFIG` and `PORT`.

### Step 3 · Split the "Done when" and decide

| # | Part | What you do | What you check | How |
| --- | --- | --- | --- | --- |
| 1 | gives 200 | request `/index.html` | status is 200 | test |
| 2 | `Content-Type: text/html` | same request | header `content-type` | test |
| 3 | an exact `Content-Length` | same request | header `content-length` = size of the file | test |
| 4 | `Connection: close` | same request | header `connection` | test |
| 5 | CSS and images load in the browser | open the page in a browser | it looks right | **by hand**, screenshot in the PR |
| 6 | no read permission gives 403 | create a file nobody can read, request it | status is 403 | test |

### Step 4 · Write one test per part

**Part 1.** A request → `with webserv` + `send`. "gives 200" → `assert response.status == 200`.

```python
def test_index_html_returns_200():
    with webserv(CONFIG, port=PORT):
        response = send("GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n", port=PORT)
        assert response.status == 200
```

**Parts 2 and 4.** Same request. Only the `assert` changes: now it checks a header.

```python
def test_index_html_has_content_type_html():
    with webserv(CONFIG, port=PORT):
        response = send("GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n", port=PORT)
        assert response.headers["content-type"] == "text/html"


def test_index_html_has_connection_close():
    with webserv(CONFIG, port=PORT):
        response = send("GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n", port=PORT)
        assert response.headers["connection"] == "close"
```

**Part 3.** "Exact" means "the same as the real size of the file". So first we read that size:

```python
def test_index_html_has_exact_content_length():
    file_size = (ROOT / "www/index.html").stat().st_size
    with webserv(CONFIG, port=PORT):
        response = send("GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n", port=PORT)
        assert response.headers["content-length"] == str(file_size)
```

- `ROOT` = the root folder of the repo.
- `.stat().st_size` = the size of the file, in bytes.
- `str(...)` turns the number into text, because headers are text.

**Part 6.** We need a file nobody can read. The test creates it, removes its permissions, and deletes it at the end:

```python
def test_file_without_read_permission_returns_403():
    secret = ROOT / "www/secret.txt"
    secret.write_text("nobody can read me")
    os.chmod(secret, 0)
    try:
        with webserv(CONFIG, port=PORT):
            response = send("GET /secret.txt HTTP/1.1\r\nHost: localhost\r\n\r\n", port=PORT)
            assert response.status == 403
    finally:
        secret.unlink()
```

- `write_text(...)` creates the file with that text.
- `os.chmod(secret, 0)` is the same as `chmod 000 www/secret.txt` in the terminal. It needs `import os` at the top of the file.
- `try:` … `finally:` = "whatever happens in `try`, always do `finally`". So the file is deleted even if the test fails.
- `unlink()` deletes the file.

**Part 5** has no test. It goes in the PR description (step 7).

### Step 5 · Run the tests before writing the code

```bash
make && python3 tests/run_all.py
```

The new tests must say **FAIL**, because B6b is not written yet. That is good: it proves the tests really check something. A test that passes before the code exists checks nothing.

### Step 6 · Write the code, and run the tests again until everything says PASS

When a test fails, its name tells you which part is still wrong.

### Step 7 · Open the PR

Include the code, `tests/test_static_files.py`, and one line for part 5, for example:

> CSS and images: opened http://localhost:8080 in Firefox; the page loads with its style and image (screenshot below).

**The finished file** starts like this, followed by the 5 tests of step 4:

```python
"""Tests for B6b: serve static files with MIME types."""

import os

from helpers import ROOT, send, webserv

CONFIG = "conf/default.conf"
PORT = 8080   # must be the same port as "listen" in CONFIG
```

---

## 7. How this fits into our work

- **Each test goes in the PR of the issue it checks.** The 404 test goes in the PR of B7. The timeout test goes in the PR of A6.
- **Not every test can run from day one.** Each tool needs some code to exist first:

  | Tool | Works when | Sprint |
  | --- | --- | --- |
  | `run_webserv(...)` | Always | From now |
  | `with webserv(...)` | webserv listens on a port (A3, A4) | 2 |
  | `send(...)` | webserv answers a request (A5: A hands the request to B and sends B's Response) | 3 |

  Until a tool works, show in your PR that the "Done when" works in another way (a small program, or the output of a command).
- **The reviewer checks** that the PR has one test for each part of the "Done when".

---

## 8. Why Python, and not curl or C++

- **curl** builds the request itself and sends it all at once. It cannot send a broken request (`GET HTTP/1.1`, which must give 400) or a request in pieces. Those are exactly the cases where servers crash.
- **C++** would need a whole socket client for each test, plus a new Makefile rule. A lot of code to check one 404.
- **Python** does it in the few lines above, with nothing to install. And with the template, nobody needs to learn it. It is also the first language the subject suggests for tests (page 11).
