# Webserv configuration proposal

This is a discussion draft for issue #9. It describes the syntax used by
`conf/default.conf` so that the listening-socket code (A), HTTP/route code
(B), and config/CGI code (C) can agree on one contract before the parser is
implemented. **Working decisions** below summarize A and B's PR replies; C's answers are proposals until the team confirms them.
At this stage, the goal is to agree on the format. The server, parser, sample
website, and CGI program are not ready, so this draft is not intended to run
or be tested yet.

## File shape

```text
server {
    listen 127.0.0.1:8080;
    root ./www/site-a;

    location /uploads {
        methods GET POST DELETE;
        upload_store ./www/uploads;
    }
}
```

Proposed grammar: a file contains one or more `server { ... }` blocks. A
`server` contains directives and zero or more `location <URL-prefix> { ... }`
blocks. A directive ends with `;`. Braces and semicolons are separate tokens;
spaces and newlines separate words. `#` starts a comment that ends at the next
newline. The sample uses simple paths without spaces or quoting. Paths and
matching rules need team agreement (see questions below).

The program may receive a config path as `./webserv <path>`; the default path
used when no argument is supplied is still to be decided.

## Directives

The table gives **proposed** syntax and meaning. `server` means the rule
applies to that server block; `location` means it applies to matching requests.
The table is a quick reference; the detailed rules and remaining decisions are recorded below.

| Directive | Proposed scope and syntax | Intended meaning | Example |
| --- | --- | --- | --- |
| `listen` | `server`: `listen <IPv4-address-or-localhost>:<port>;` or `listen <port>;` | Open an IPv4 listener; a bare port means `0.0.0.0:<port>`. A server may have several nonconflicting listeners. | `listen 127.0.0.1:8080;` |
| `root` | `server` or `location`: `root <directory>;` | Directory from which static URL paths are resolved. A location value overrides the server value. | `root ./www/site-a;` |
| `index` | `server` or `location`: `index <filename>;` | File to try when the mapped URL is a directory. | `index index.html;` |
| `error_page` | `server`: `error_page <status> <file>;` | Custom response file for that HTTP error status; otherwise use a built-in page. May repeat for different statuses. | `error_page 404 ./www/errors/404.html;` |
| `client_max_body_size` | `server`: `client_max_body_size <bytes>;` | Reject a request body larger than this many bytes. | `client_max_body_size 1048576;` |
| `methods` | `location`: `methods <method>...;` | Allow the listed methods on this route. At minimum, the project must support GET, POST and DELETE somewhere. | `methods GET POST DELETE;` |
| `autoindex` | `location`: `autoindex on\|off;` | Enable or disable a directory listing when no index file applies. | `autoindex off;` |
| `return` | `location`: `return <3xx-status> <target>;` | Redirect requests matching this route. | `return 301 /new;` |
| `upload_store` | `location`: `upload_store <directory>;` | Permit uploads on this route and save files in this directory. | `upload_store ./www/uploads;` |
| `cgi` | `location`: `cgi <extension> <executable>;` | Run matching script files through the named CGI executable. | `cgi .py /usr/bin/python3;` |

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
  is planned. Ports must be whole numbers from 1 to 65535. C stores host and
  port; A resolves them with `getaddrinfo` using `AF_INET` and `AI_PASSIVE`.
- A server may have multiple listeners. A resolved address/port collision
  anywhere in the file is a config error; a wildcard listener conflicts with
  any listener on the same port. Each listening socket belongs to exactly one
  server, selected by listening fd rather than the HTTP `Host` header.
- If any bind fails, startup fails with a useful host/port error, closes
  sockets already opened, and exits non-zero. The server must not run with
  only some configured listeners.

### B — routes and responses (questions 4–10)

- `location` matches a URI path prefix at a segment boundary: `/images`
  matches `/images` and `/images/a.png`, but not `/images-old/a.png`. The
  longest matching location wins.
- Remove the matching location prefix before joining the remainder to its
  `root`: `/images/a.png` with `root ./www/img` maps to `./www/img/a.png`.
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

- **Proposal:** Resolve relative paths against the config file's directory,
  so launching from another working directory does not change their meaning.
  If accepted, the `./www/...` paths in `conf/default.conf` need to become
  `../www/...` before that file is runnable. **Decision still needed:** choose
  config-directory or launch-directory relative paths.
- **Proposal:** Accept positive decimal byte counts only, with no size
  suffixes; reject zero, signs, non-digits, and overflow. Default
  `client_max_body_size` to 1 MiB (1,048,576 bytes) per server, as in the
  provisional C2 header. B returns 413 for an oversized request.
- **Proposal:** Parse the whole file before opening listeners. Reject unknown
  or misplaced directives, wrong argument counts, malformed values, unmatched
  braces, duplicate single-value directives, and duplicate keyed entries.
  Permit repeated `listen`, and distinct `error_page` status codes and `cgi`
  extensions. Require a nonempty server block; report `path:line: reason` and
  exit non-zero on a config error. **Decision still needed:** require an
  explicit `listen`, or keep C2's provisional `127.0.0.1:8080` fallback.
- **Proposal:** Support whitespace, braces, semicolons, unquoted tokens, and
  `#` comments to end of line. Reject quoting and escaping for now, and
  document that paths and targets cannot contain spaces.
- **Proposal:** With no argument, load `conf/default.conf`; an explicit
  argument selects another config file. Missing/unreadable config is a
  startup error.
- **Proposal:** Assign owners for later demo files and directories. Their
  absence does not block agreement on this format.

The original questions are kept below as the meeting agenda. Confirm or
revise the working decisions above, and record any deferred item with an owner.

## Questions for the team to confirm

### A — listening sockets and server selection

1. Does `listen` accept only IPv4 `address:port`, or also `port` alone,
   hostnames, and IPv6? Does `0.0.0.0` mean all IPv4 interfaces?
2. May one `server` contain multiple `listen` directives? May two servers
   share an address/port pair? If so, how is the server selected without
   requiring optional virtual-host support?
3. What should happen when a configured address or port cannot be bound:
   reject the whole config/startup, or continue with other listeners?

### B — requests, routes, and files

4. Is `location` a URL prefix? Should the longest matching prefix win, and
   must `/images` match `/images/a.png` but not `/images-old/a.png`?
5. When `location /images { root ./www/img; }` receives
   `/images/a.png`, is the file `./www/img/a.png` (strip the prefix) or
   `./www/img/images/a.png` (keep it)? The project brief's example suggests
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
   methods (HTTP requires that header on a 405).
8. If `return` is set, does it take precedence over static files, uploads,
   and CGI? Which redirect statuses and target forms are supported?
9. Is `upload_store` alone enough to enable uploads? How are upload filenames
   chosen, and does it apply to POST only or also another method?
10. Does `cgi` match the extension of the requested script path? Which
    methods may invoke it, and where is the CGI process's working directory?

### C — parser, validation, and shared decisions

11. Are paths resolved relative to the config file's directory, the process's
    working directory, or required to be absolute? Must paths exist at startup?
12. Should sizes be decimal byte counts only, or also units such as `1M`?
    What is the default body-size limit if the directive is absent?
13. What should happen for unknown, duplicate, misplaced, or malformed
    directives, an empty `server`, and an unmatched brace? Prefer a startup
    error with a line number if the team agrees.
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
- [ ] The team resolves C's open path and missing-`listen` decisions; C then updates this document and `conf/default.conf`.
- [ ] All ten directives have agreed syntax, scope, defaults, and an example.
- [ ] Open questions are resolved or explicitly deferred with an owner.
