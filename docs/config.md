# Webserv configuration proposal

This is a discussion draft for issue #9. It describes the syntax used by
`conf/default.conf` so that the listening-socket code (A), HTTP/route code
(B), and config/CGI code (C) can agree on one contract before the parser is
implemented. **Proposed** behavior below is not final until A and B review it.
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
The exact defaults and inheritance rules remain review questions.

| Directive | Proposed scope and syntax | Intended meaning | Example |
| --- | --- | --- | --- |
| `listen` | `server`: `listen <IPv4-address>:<port>;` | Open a listening socket for an interface/port pair. Repeat for more pairs if agreed. | `listen 127.0.0.1:8080;` |
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

## Questions for A and B to decide in the PR

Please answer these before treating the syntax or sample as an implementation
contract. Record each decision in the directive table and example config.

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
7. What are the default accepted methods and default `autoindex` value?
   Which status is sent for a method excluded by `methods`?
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

- [ ] A confirms the `listen` format and server-selection behavior.
- [ ] B confirms location matching, path mapping, and per-route behavior.
- [ ] C updates this document and `conf/default.conf` with the decisions.
- [ ] All ten directives have agreed syntax, scope, defaults, and an example.
- [ ] Open questions are resolved or explicitly deferred with an owner.
