# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Webserv (Codam/42): a non-blocking HTTP server in C++, built as a team project.

## Commands

```bash
make                               # builds ./webserv
make && python3 tests/run_all.py   # run all tests
```

## Rules that always apply

- C++17 (`-std=c++17` in the Makefile). C++11 and C++17 are confirmed allowed, despite the subject text saying C++98.
- Any socket or pipe I/O must go through the single `poll()` loop: no `read`/`recv`/`write`/`send` before poll reports the fd ready, and no `errno` checks after I/O. Breaking this is a grade 0.

## Where to look

- Before using a system call or library, or for any question about required behavior: `webserv.md` (the subject summary and the authorized functions).
- Before touching config parsing, listeners, route matching, uploads, redirects, or CGI selection: `docs/config.md`. It is the agreed contract between modules A, B and C (directive syntax and defaults, error handling, route and upload rules). Its "Decisions" section is final; don't silently deviate from it, and check "Still open" before choosing something yourself.
- `conf/default.conf` is the example config and the default when `./webserv` gets no argument. Relative paths in a config resolve against the config file's directory, not the working directory. The `www/` and `cgi-bin/` files it references don't exist yet.
- Before writing or changing a test: `tests/README.md`.
- Before changing a module interface: the comments in its header in `include/`. They define the contract between modules (A = event loop, B = HTTP, C = config/CGI, X = infrastructure). Keep them in sync with the code.
