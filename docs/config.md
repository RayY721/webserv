# Webserv configuration proposal

This is a discussion draft for issue #9. It describes the syntax used by
`conf/default.conf` so that the listening-socket code (A), HTTP/route code
(B), and config/CGI code (C) can agree on one contract before the parser is
implemented. **Working decisions** below summarize A and B's PR replies.
C1 selects the defaults, config-relative paths, and explicit-listen rule below
for review; this does not claim that the whole team has approved them.
At this stage, the goal is to agree on the format. The server, parser, sample
website, and CGI program are not ready, so this draft is not intended to run
or be tested yet.

## File shape

```text
server {
    listen 127.0.0.1:8080;
    root ../www/site-a;

    location /uploads {
        methods GET POST DELETE;
        upload_store ../www/uploads;
    }
}
```

Proposed grammar: a file contains one or more `server { ... }` blocks. A
`server` contains directives and zero or more `location <URL-prefix> { ... }`
blocks. A directive ends with `;`. Braces and semicolons are separate tokens;
spaces and newlines separate words. `#` starts a comment that ends at the next
newline. The sample uses simple paths without spaces or quoting. Relative
filesystem paths use the config file directory, as specified below.

The program may receive a config path as `./webserv <path>`. C proposes
`conf/default.conf` when no argument is supplied (see question 15).

## Directives

The table gives the syntax, meaning, and defaults selected by C1 for review.
`server` means the rule applies to that server block; `location` means it applies to matching requests.
The table is a quick reference; the detailed rules and remaining decisions are recorded below.

| Directive | Scope and syntax | Intended meaning | Default | Example |
| --- | --- | --- | --- | --- |
| `listen` | `server`: `listen <IPv4-address-or-localhost>:<port>;` or `listen <port>;` | Open an IPv4 listener; a bare port means `0.0.0.0:<port>`. A server may have several nonconflicting listeners. | Required in every server; no default listener. | `listen 127.0.0.1:8080;` |
| `root` | `server` or `location`: `root <directory>;` | Directory from which static URL paths are resolved. A location value overrides the server value. | Server: `.` (config file directory). Location: inherit server root. | `root ../www/site-a;` |
| `index` | `server` or `location`: `index <filename>;` | File to try when the mapped URL is a directory. | Server: `index.html`. Location: inherit server index. | `index index.html;` |
| `error_page` | `server`: `error_page <status> <file>;` | Custom response file for that HTTP error status; otherwise use a built-in page. May repeat for different statuses. | Unset: use the built-in error page for that status. | `error_page 404 ../www/errors/404.html;` |
| `client_max_body_size` | `server`: `client_max_body_size <bytes>;` | Reject a request body larger than this many bytes. | 1048576 bytes (1 MiB) per server. | `client_max_body_size 1048576;` |
| `methods` | `location`: `methods <method>...;` | Allow the listed methods on this route. Only uppercase `GET`, `POST`, `DELETE` are valid; anything else is a config error. At minimum, the project must support GET, POST and DELETE somewhere. | `GET` only. | `methods GET POST DELETE;` |
| `autoindex` | `location`: `autoindex on\|off;` | Enable or disable a directory listing when no index file applies. | `off`. | `autoindex off;` |
| `return` | `location`: `return <3xx-status> <target>;` | Redirect requests matching this route. | Unset: no redirect. | `return 301 /new;` |
| `upload_store` | `location`: `upload_store <directory>;` | Permit uploads on this route and save files in this directory. | Unset: uploads disabled. | `upload_store ../www/uploads;` |
| `cgi` | `location`: `cgi <extension> <executable>;` | Run matching script files through the named CGI executable. | Unset: CGI disabled. | `cgi .py /usr/bin/python3;` |

`conf/default.conf` is an illustrative proposal. It shows all ten directives,
two listening ports, static files, a redirect, an upload route, and one CGI
type. Its paths are placeholders for discussing the intended format; they do
not imply that the files or directories exist yet.

## Working decisions from the PR discussion

These are A and B's written answers, recorded here for the team to confirm at
the next meeting. They describe intended behavior, not tested implementation.

### A — listeners (questions 1–3)

- `listen` accepts a dotted IPv4 address or `localhost` plus a port, or a bare
  port meaning `0.0.0.0:<port>`. `0.0.0.0` means every IPv4 interface. No IPv6
  is planned. C validates each listen value: a dotted IPv4 address or
  `localhost` with a port, or a bare port. Ports must contain decimal digits
  only and be in 1–65535; reject invalid values and overflow before narrowing
  to `unsigned short`. C stores host and port; A resolves them with
  `getaddrinfo` using `AF_INET` and `AI_PASSIVE`.
- Every server must contain at least one explicit `listen`. C rejects a
  missing listener with `path:line: server block has no listen directive`,
  using the server block opening line. There is no fallback listener.
- A server may have multiple listeners. A resolved address/port collision
  anywhere in the file is a config error; a wildcard listener conflicts with
  any listener on the same port. Each listening socket belongs to exactly one
  server, selected by listening fd rather than the HTTP `Host` header.
  **A owns this check in A8**, after resolving all configured addresses and
  before any `bind()`. For example, `localhost:8080` and `127.0.0.1:8080`
  conflict when localhost resolves to that address. C owns syntax and
  structural validation, not resolved endpoint collision detection.
- If any bind fails, startup fails with a useful host/port error, closes
  sockets already opened, and exits non-zero. The server must not run with
  only some configured listeners.

### B — routes and responses (questions 4–10)

- `location` matches a URI path prefix at a segment boundary: `/images`
  matches `/images` and `/images/a.png`, but not `/images-old/a.png`. The
  longest matching location wins.
- **Agreed in #26 (B and C):** the parser stores a location path without a
  trailing `/`, except `/` itself: `location /uploads/` and
  `location /uploads` both become `/uploads`. Route code therefore handles
  one form only.
- **Agreed in #26 (B and C):** if a server has no `location /`, the parser
  adds one with no directives of its own: it uses the server's `root` and
  `index`, allows GET only, and has autoindex off. Every URI then matches at
  least one location, and route code needs no "no location matched" case.
- **Agreed in #26 (B and C):** the parser converts `methods` words to the
  shared `Method` enum (`include/Method.hpp`, also used by `Request`).
  Method names are uppercase `GET`, `POST`, and `DELETE` only; any other word
  (`get`, `GETT`, `PUT`) is a config error with its line number. The 405 check
  compares the request's `Method` directly with the location's set.
- Remove the matching location prefix before joining the remainder to its
  `root`: `/images/a.png` with `root ../www/img` maps to `../www/img/a.png`.
  Joining must produce exactly one separator. This `root` behavior resembles
  NGINX `alias` rather than NGINX `root`.
- Only `root` and `index` may appear at both server and location levels;
  a location value overrides the inherited server value. `listen`,
  `error_page`, and `client_max_body_size` are server-only. `methods`,
  `autoindex`, `return`, `upload_store`, and `cgi` are location-only.
- Absent `methods` means GET only; absent `autoindex` means off. A disallowed
  method gets 405 with an `Allow` header listing permitted methods.
- `return` takes precedence over file serving, uploads, and CGI. Supported
  redirect codes are 301, 302, 303, 307, and 308; other codes are a config
  error. The target is copied to `Location` as a path or full URL.
- Uploads require both `upload_store` and POST in `methods`; POST creates a
  file and returns 201. B proposes a form filename, or the final URI segment
  for a raw body, and replacement of an existing file. **Still discuss**
  filename sanitization, URL decoding, and overwrite policy before treating
  this as a safe file-writing rule.
- A request runs as CGI when its script extension matches `cgi` in the
  selected location and its method is GET or POST and permitted by `methods`.
  Other files are served normally; DELETE does not run CGI. The CGI working
  directory is the script's own directory.

### C — parser and configuration (questions 11–16)

- **C1 decision:** Resolve relative `root`, `error_page`, `upload_store`, and
  CGI executable paths against the directory containing the config file.
  Absolute paths stay absolute. Resolve the selected config path to a stable
  absolute location before interpreting its contents. The default server
  `root` of `.` also means the config file directory, not the launch directory.
  For `/project/conf/default.conf`, `root .` means `/project/conf`, while
  `root ../www/site-a` means `/project/www/site-a`. The sample uses these
  config-relative paths. `index` is a filename within the mapped request
  directory, and redirect targets are URLs, not config-relative file paths.
  Preserve original values and source lines for diagnostics. Required demo
  assets and their startup availability checks remain later implementation work.
- **C1 decision:** Keep C2's server defaults `root .` and `index index.html`;
  a location without either value inherits that value from its server.
- **C1 decision:** Accept positive decimal byte counts only, with no size
  suffixes; reject zero, signs, non-digits, and overflow. Default
  `client_max_body_size` to 1 MiB (1,048,576 bytes) per server, as in the
  C2 header. B returns 413 for an oversized request.
- **Proposal:** Parse the whole file before opening listeners. Reject unknown
  or misplaced directives, wrong argument counts, malformed values, unmatched
  braces, duplicate single-value directives, and duplicate keyed entries.
  Permit repeated `listen`, and distinct `error_page` status codes and `cgi`
  extensions. Require at least one server block; report `path:line: reason`
  and exit non-zero on a config error. **C1 decision:** each server must have
  an explicit `listen`; reject an empty or listener-less block instead of
  adding a fallback. A then performs the resolved endpoint checks in A8
  before any bind; bind failures still require cleanup and a non-zero exit.
- **Proposal:** Support whitespace, braces, semicolons, unquoted tokens, and
  `#` comments to end of line. Reject quoting and escaping for now, and
  document that paths and targets cannot contain spaces.
- **Proposal:** With no argument, load `conf/default.conf`; an explicit
  argument selects another config file. Missing/unreadable config is a
  startup error.
- **Proposal:** Assign owners for later demo files and directories. Their
  absence does not block agreement on this format.

### C2 — runtime structures (agreed in #26 review)

These rules come from A's and B's review of the C2 headers and bind the code
that uses them.

- `ServerConfig` and `LocationConfig` objects are built once by the parser and
  never modified, added to, or resized afterwards. `Client` keeps a
  `const ServerConfig*`, and route code may keep pointers into `locations`,
  so later changes could leave dangling pointers.
- `CgiProcess` owns the parent-side pipe fds and the child process. It cannot
  be copied. Its destructor closes any open fd and kills and reaps a child not
  yet reaped, because `Client` holds it in a `std::unique_ptr` and drops it on
  disconnect. Code that closes an fd early or reaps the child sets that field
  to `-1`.
- `CgiProcess::start_time` uses `std::chrono::steady_clock`, the same clock
  as `Client::lastActivity`, so one timeout sweep can check both.

The original questions are kept below as the meeting agenda. Confirm or
revise the working decisions above, and record any deferred item with an owner.

## Questions for the team to confirm

### A — listening sockets and server selection

1. Does `listen` accept only IPv4 `address:port`, or also `port` alone,
   hostnames, and IPv6? Does `0.0.0.0` mean all IPv4 interfaces?
2. May one `server` contain multiple `listen` directives? May two servers
   share an address/port pair? If so, how is the server selected without
   requiring optional virtual-host support?

   **Working answer (A, recorded by C1):** Multiple listeners are allowed,
   but duplicate resolved interface:port pairs and wildcard overlaps are
   startup errors. A8 checks all resolved endpoints before any `bind()`.
   C validates listen syntax, port range, and presence in each server.
   Each listening fd selects exactly one ServerConfig.
3. What should happen when a configured address or port cannot be bound:
   reject the whole config/startup, or continue with other listeners?

### B — requests, routes, and files

4. Is `location` a URL prefix? Should the longest matching prefix win, and
   must `/images` match `/images/a.png` but not `/images-old/a.png`?

   **Working answer (B and C):** Yes to all three. The parser removes a
   trailing `/` from location paths (except `/`) and adds a default
   `location /` to any server without one.
5. When `location /images { root ../www/img; }` receives
   `/images/a.png`, is the file `../www/img/a.png` (strip the prefix) or
   `../www/img/images/a.png` (keep it)? The project brief's example suggests
   stripping the prefix.
6. Do `root` and `index` inherit from `server` into a `location`? Which
   directives may be set at both levels, and how does an override work?

   **Working answer (B and C):** Only `root` and `index` may appear in both `server` and
   `location`. A value set in a `location` overrides the server's, and if it
   is absent the location uses the server's value. `listen`, `error_page` and
   `client_max_body_size` are server-only. `methods`, `autoindex`, `return`,
   `upload_store` and `cgi` are location-only. Putting any directive at the
   wrong level is a config error.
7. What are the default accepted methods and default `autoindex` value?
   Which status is sent for a method excluded by `methods`?

   **Working answer (B and C):** If `methods` is absent, the location allows only `GET`.
   `autoindex` defaults to `off`. A request whose method is not in the list
   gets `405 Method Not Allowed` with an `Allow` header listing the allowed
   methods (HTTP requires that header on a 405). Method names are uppercase
   `GET`, `POST`, `DELETE`; anything else in `methods` is a config error.
8. If `return` is set, does it take precedence over static files, uploads,
   and CGI? Which redirect statuses and target forms are supported?
9. Is `upload_store` alone enough to enable uploads? How are upload filenames
   chosen, and does it apply to POST only or also another method?
10. Does `cgi` match the extension of the requested script path? Which
    methods may invoke it, and where is the CGI process's working directory?

### C — parser, validation, and shared decisions

11. Are paths resolved relative to the config file's directory, the process's
    working directory, or required to be absolute? Must paths exist at startup?

    **C1 decision:** Use the config file directory for relative filesystem
    paths, including the default `root .`; absolute paths stay absolute.
    Demo assets and availability checks remain later implementation work.
12. Should sizes be decimal byte counts only, or also units such as `1M`?
    What is the default body-size limit if the directive is absent?

    **C1 decision:** Positive decimal bytes only; default 1048576 bytes per
    server. Reject zero, signs, non-digits, and overflow.
13. What should happen for unknown, duplicate, misplaced, or malformed
    directives, an empty `server`, and an unmatched brace? Prefer a startup
    error with a line number if the team agrees.

    **C1 decision on missing listen:** Every server requires an explicit
    `listen`. Report `path:line: server block has no listen directive` and
    exit non-zero; do not insert a default listener.
14. Are comments, quoted paths containing spaces, and escaping needed now?
    The sample uses only `#` comments and unquoted paths.
15. Should `./webserv` load `conf/default.conf` by default when no path is
    supplied, or should it require an explicit path?
16. Which example files and directories should the team eventually add, and
    who will own that work? This decision does not block discussion of the
    format now.

## Review checklist

- [ ] The team confirms A's listener and server-selection answers.
- [ ] The team confirms B's route and response answers, including upload safety.
- [ ] The team reviews C1's selected defaults, config-relative paths, and explicit-`listen` rule; the document and illustrative config reflect these choices.
- [ ] The team confirms the C2 structure agreements from #26 (path normalization, default `location /`, `Method` enum, immutable configs, `CgiProcess` ownership and clock).
- [ ] All ten directives have agreed syntax, scope, defaults, and an example.
- [ ] Open questions are resolved or explicitly deferred with an owner.
