# Webserv — Project Brief

**Source:** *Webserv*, version 24.0 (15-page project subject).  
**Goal:** Implement an HTTP server in C++98 that can be tested with a standard web browser.

## Project at a glance

| Item | Requirement |
| --- | --- |
| Program name | `webserv` |
| Invocation | `./webserv [configuration file]` |
| Language | C++98 |
| Files to submit | `Makefile`, `*.h`, `*.hpp`, `*.cpp`, `*.tpp`, `*.ipp`, and configuration files |
| Makefile rules | `$(NAME)`, `all`, `clean`, `fclean`, `re` |
| Library policy | No external libraries, Boost, or Libft |

The configuration file may be supplied on the command line or loaded from a default path. The server must be implemented directly; launching another web server with `execve` is prohibited.

## Build and reliability rules

- Compile with `c++ -Wall -Wextra -Werror`. The code must also compile with `-std=c++98`.
- Prefer C++ facilities over corresponding C facilities where possible.
- The Makefile must compile the sources without unnecessary relinking.
- The program must not crash or terminate unexpectedly, including when memory runs out. A crash makes the project non-functional and results in a grade of 0.
- A request must never hang indefinitely. The server must stay operational under stress and handle client disconnections properly.

## HTTP and I/O requirements

- Serve a complete static website and support file uploads.
- Implement at least the `GET`, `POST`, and `DELETE` methods.
- Return accurate HTTP status codes and provide default error pages when no custom pages are configured.
- Work with a standard browser of your choice. NGINX can be used to compare response headers and behavior, accounting for differences between HTTP versions.
- Listen on multiple ports and serve different content according to the configuration.
- Keep the server non-blocking at all times. Use **one** `poll()` call, or one equivalent event mechanism, for all client/server I/O, including listening sockets. The mechanism must monitor reads and writes simultaneously.
- Never call `read`/`recv` or `write`/`send` on an I/O descriptor that can wait for data (for example, a socket or pipe/FIFO) without prior readiness from that event mechanism. Violating this rule results in a grade of 0. Regular disk files are exempt from readiness monitoring.
- Do not inspect `errno` after a read or write operation to adjust server behavior.
- `poll()` may be replaced with an equivalent such as `select()`, `kqueue()`, or `epoll()`. Associated macros and helper functions are allowed.
- Use `fork` only for CGI.

The subject recommends reading the HTTP RFCs and testing with telnet and NGINX before implementation. HTTP/1.0 is a suggested reference point, not a mandatory version, and the entire RFC is not required. Virtual hosting is outside the mandatory scope.

### macOS-specific rule

On macOS, file descriptors must be placed in non-blocking mode. `fcntl()` may be used only with `F_SETFL`, `O_NONBLOCK`, and `FD_CLOEXEC`; other flags are forbidden by the subject.

## Configuration file capabilities

The format may take inspiration from an NGINX `server` block. It must support:

1. All interface and port pairs on which the server listens.
2. Default error pages.
3. A maximum client request-body size.
4. Per-URL/route rules (regular expressions are not required):
   - Accepted HTTP methods.
   - HTTP redirects.
   - A filesystem root for requested files. For example, if `/kapouet` maps to `/tmp/www`, then `/kapouet/pouic/toto/pouet` resolves under `/tmp/www/pouic/toto/pouet`.
   - Directory listing enabled or disabled.
   - A default file for directory requests.
   - Permission to upload files and a storage location for them.
   - CGI execution selected by file extension, such as `.php`.

At least one CGI type must work. The CGI environment must expose the complete client request and arguments. For chunked requests, the server must remove chunk framing before passing the body to CGI; the CGI expects EOF to end the body. If CGI output has no `content_length`, EOF marks the end of its data. Run the CGI in the correct directory so relative paths work.

Provide configuration files and default files that demonstrate every required feature during evaluation. Additional configuration features, such as server names for optional virtual hosts, are allowed.

## Authorized external functions

All functionality must be implemented in C++98. The subject lists these external functions and related facilities as authorized:

`execve`, `pipe`, `strerror`, `gai_strerror`, `errno`, `dup`, `dup2`, `fork`, `socketpair`, `htons`, `htonl`, `ntohs`, `ntohl`, `select`, `poll`, `epoll` (`epoll_create`, `epoll_ctl`, `epoll_wait`), `kqueue` (`kqueue`, `kevent`), `socket`, `accept`, `listen`, `send`, `recv`, `chdir`, `bind`, `connect`, `getaddrinfo`, `freeaddrinfo`, `setsockopt`, `getsockname`, `getprotobyname`, `fcntl`, `close`, `read`, `write`, `waitpid`, `kill`, `signal`, `access`, `stat`, `open`, `opendir`, `readdir`, `closedir`.

The separate restrictions on `execve`, `fork`, and macOS `fcntl()` still apply.

## README requirements

Place `README.md` at the root of the Git repository and write it in English. Its **first line** must be italicized and read:

> *This project has been created as part of the 42 curriculum by <login1>[, <login2>[, <login3>[...]]].*

Include at least:

- **Description:** project goal and a brief overview.
- **Instructions:** compilation, installation, and/or execution information.
- **Resources:** conventional references and a description of how AI was used, including the tasks and project parts involved.

Other sections may be added where useful.

## Testing and evaluation

- Stress-test availability, exercise the features with a browser, and use more than one testing program. The subject suggests writing tests in Python, Go, C, or C++.
- A small provided tester may help find bugs, but its use is optional.
- Only repository contents are evaluated during the defense. Check filenames before submission.
- Evaluators may request a small, time-limited modification to verify understanding, such as changing behavior, updating a function, or adjusting a data structure.

## Optional bonus

- Cookies and session management, with simple examples.
- Multiple CGI types.

The bonus is assessed only after all mandatory requirements pass without issues.

## AI-use guidance in the subject

The subject encourages using AI for repetitive work and learning, while checking generated material, understanding and taking responsibility for anything used, and discussing work with peers. Its examples warn against copying code that cannot be explained during evaluation.
