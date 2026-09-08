# 11. Web Runtime

Build persistent JSON APIs, streaming services, and multi-worker applications with HHY.

## 11.1 Start with one API

v1.4.3 contains the complete v1.4.0–v1.4.3 Web release train. Each worker loads the application once, handlers are reusable, and one failed request returns 500 without terminating later requests.


```hhy
import web

fn hello(request) {
    web.json({ ok: true, name: request.query_params.name })
}

web.app()
    |> web.request_id
    |> web.health
    |> web.metrics
    |> web.gzip
    |> web.get("/hello", hello)
    |> web.listen({ host: "127.0.0.1", port: 8080, workers: 4 })
```


```sh
hhy serve app.hhy
hhy serve --dev app.hhy
```


## 11.2 v1.4.3 capability boundary

| Layer | Capabilities |
| --- | --- |
| Runtime | Opaque C embedding, JSON boundary, AST/Bytecode, 100,000-call repetition test |
| HTTP | HTTP/1.1, Router, Query/Header/Cookie, hard body limits, stable 4xx/5xx |
| Application | Middleware, ETag static files, upload, CORS, signed cookies, trusted proxy, hot reload |
| Production | Stream, SSE, Range, prefork workers, health checks, structured logs, Prometheus metrics |


{% hint style="info" %}
Deploy behind Caddy, Nginx, or a managed load balancer for TLS/HTTP/2; WebSocket is outside v1.4. Enable web.trust_proxy only when every direct client is a trusted proxy.
{% endhint %}


## 11.3 Use the HHY Web framework

HHY Web is a separate lightweight framework built on the v1.4.3 Web Runtime. It combines a Flask-inspired approachable API with Go-like explicit deployment, adding an application factory, five HTTP verbs, middleware, blueprints, stable Problem JSON, bearer authentication, uploads, Stream, and SSE helpers without duplicating the Router, worker model, or network stack.


[View HHY Web on GitHub ↗](https://github.com/hh696-wq/hhy-web)

The public repository includes English and Chinese guides, Hello World, a complete JSON API, CLI, unit tests, and a real HTTP smoke test.


```sh
git clone https://github.com/hh696-wq/hhy-web.git
cd hhy-web
./bin/hhy-web run examples/hello/app.hhy --port 8000 --dev
curl http://127.0.0.1:8000/
```


**app.hhy**

```hhy
import "./lib/hhyweb.hhy" as hhyweb

fn hello(request) {
    return hhyweb.json({
        message: "Hello, HHY Web!",
        request_id: request.id
    })
}

hhyweb.minimal()
    |> hhyweb.get("/", hello)
    |> hhyweb.serve({ host: "127.0.0.1", port: 8000, workers: 1 })
```


{% hint style="info" %}
HHY Web is currently at 0.1.0 and requires HHY Language v1.4.3 or newer. Using import web directly remains the stable, supported low-level path.
{% endhint %}
