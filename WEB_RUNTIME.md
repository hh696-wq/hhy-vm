# HHY Web Runtime

HHY v1.4 ships one Web release train. Applications use the HHY Runtime version;
there is no separate `Web 0.x` version line.

| HHY version | Web stage | Capability boundary |
| --- | --- | --- |
| v1.4.0 | Runtime foundation | Opaque C embedding handles, one-time application load, reusable calls, JSON ABI boundary |
| v1.4.1 | Web MVP | HTTP/1.1, Router, request data, JSON/text/HTML/redirect responses |
| v1.4.2 | Web application | Middleware, static files, multipart upload, CORS, signed cookies, gzip, health and dev reload |
| v1.4.3 | Streaming and workers | Response streams, SSE, Range requests, prefork workers, metrics and structured access logs |

## Run a Web application

```sh
make
./build/hhy serve examples/10-web-api.hhy -- 8080

# Restart automatically when the application source changes.
./build/hhy serve --dev examples/10-web-api.hhy -- 8080
```

`web.listen` binds to `127.0.0.1` by default. Put production services behind
Caddy, Nginx, or a managed load balancer for TLS and HTTP/2. WebSocket is not a
v1.4 capability.

```hhy
import web

fn user(request) {
    web.json({
        id: request.params.id,
        search: request.query_params.q,
        request_id: request.id
    })
}

web.app()
    |> web.request_id
    |> web.cors({ origin: "https://example.com" })
    |> web.health
    |> web.metrics
    |> web.gzip
    |> web.get("/users/:id", user)
    |> web.static("/assets", path("public"))
    |> web.listen({ host: "127.0.0.1", port: 8080, max_body: 8mib, workers: 4 })
```

Each worker loads the application once. A handler error produces a stable 500
response and the worker continues serving later requests. Top-level mutable
bindings are rejected by the embedding context so request handlers cannot use
them as hidden cross-request state.

## Request and response model

`WebRequest` exposes `id`, `method`, `path`, raw `query`, decoded
`query_params`, `headers`, `cookies`, route `params`, `remote_addr`, UTF-8
`body`, and binary `bytes`. Request bodies and headers are bounded before a
handler runs. Multipart files use request-scoped temporary paths and are
deleted on success and error paths.

Responses are created with `web.text`, `web.html`, `web.json`, `web.redirect`,
or `web.response`. `web.stream` uses HTTP chunked transfer and applies
backpressure by requesting the next item only after the previous chunk has
been written. `web.sse` formats a `Stream<String>` as server-sent events.
Static files reject traversal and symlink escape, publish ETag/Last-Modified,
and support a single byte Range.

`web.trust_proxy` must be enabled explicitly before `X-Forwarded-For` is used.
Only enable it when every direct client is a trusted reverse proxy. Structured
request logs include method, path, status, response size, duration, and request
ID; request headers and bodies are deliberately excluded.

## C embedding API

`make` produces `build/libhhy.a` and the public header `include/hhy/embed.h`;
release archives place both under `sdk/`.
The API uses opaque handles and JSON strings, so host code does not depend on
HHY's internal Value representation.

```c
#include <hhy/embed.h>

HhyApplication *app = hhy_application_load("handler.hhy");
HhyContext *ctx = hhy_context_new(app, NULL);
HhyEmbedResult result = hhy_call(ctx, "handler", "[{\"id\":42}]");

if (result.ok) puts(result.json);
else fputs(result.error_json, stderr);

hhy_embed_result_free(&result);
hhy_context_free(ctx);
hhy_application_free(app);
```

An application must outlive all contexts created from it. A context is not
thread-safe; use one context per host thread or worker. Both AST and Bytecode
contexts are available through `hhy_context_new_engine`; Bytecode is the
default. `make embed-test` performs AST/Bytecode differential checks and
100,000 repeated calls against one loaded application.

## Production boundary

- Configure `max_body`, Runtime time/memory/file/process limits, and worker
  count for the deployment.
- Use `GET /healthz` from `web.health` for liveness/readiness and expose the
  `web.metrics` path only to the monitoring network.
- Terminate TLS and HTTP/2 at the reverse proxy. Restrict direct access to HHY.
- Treat signed cookies as integrity protection, not encryption. Rotate secrets
  through the deployment environment and never log them.
- Run `python3 tests/check-web-server.py ./build/hhy` on a socket-capable host
  before deployment.
