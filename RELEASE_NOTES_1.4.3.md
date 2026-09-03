# HHY 1.4.3 Release Notes

HHY 1.4.3 delivers the complete v1.4 Web Runtime release train in one formal
release. The intermediate v1.4.0, v1.4.1, and v1.4.2 boundaries remain
documented capability milestones; users install v1.4.3.

## Highlights

- Persistent Runtime embedding through opaque `HhyApplication` and
  `HhyContext` handles, a JSON ABI boundary, one-time source/Bytecode loading,
  reusable `hhy_call`, AST/Bytecode selection, and installed `libhhy.a` headers.
- `hhy serve` HTTP/1.1 applications with exact and `:param` routing, decoded
  Query values, headers, cookies, bounded text/binary bodies, stable errors,
  redirects, and JSON/text/HTML/custom responses.
- Middleware, secure static roots, ETag/Last-Modified, multipart uploads with
  request-scoped cleanup, CORS preflight, signed cookies, request IDs, trusted
  proxy opt-in, gzip, health/readiness, and `--dev` reload.
- Backpressured chunked responses, SSE, large static-file streaming, byte
  Ranges, prefork workers with restart supervision, structured access/error
  logs, and Prometheus counters for requests, errors, bytes, and handler time.

TLS and HTTP/2 remain the responsibility of Caddy, Nginx, or a managed load
balancer. WebSocket is intentionally outside the v1.4 capability boundary.

## Verification evidence

- 100,000 repeated calls through one loaded embedding context, with AST and
  Bytecode differential output checks.
- 1,000,000 loopback HTTP requests at 16 concurrent clients: 0 failures in
  131.439 seconds (7,608.1 requests/second on the release-validation host).
- End-to-end HTTP coverage for Router, Query, middleware, 404/405/413/500,
  post-error recovery, static cache validators, upload cleanup, CORS, signed
  cookies, gzip, streaming, SSE, Range, metrics, health, multi-worker serving,
  and development reload.
- Release and Debug/sanitizer Runtime suites, AST/Bytecode suites, documentation
  checks, website lint/typecheck/build, and established performance gates.

See [Web Runtime](WEB_RUNTIME.md) for the API and deployment boundary.
