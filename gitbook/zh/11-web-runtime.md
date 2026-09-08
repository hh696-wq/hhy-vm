# 11. Web Runtime

使用 HHY 构建常驻 JSON API、流式服务与多 Worker 应用。

## 11.1 从一个 API 开始

v1.4.3 完整包含 v1.4.0–v1.4.3 的 Web release train：应用在每个 Worker 中只加载一次，handler 可重复调用，单次异常返回 500 而不会终止后续请求。


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


## 11.2 v1.4.3 能力边界

| 层 | 能力 |
| --- | --- |
| Runtime | opaque C embedding、JSON 边界、AST/Bytecode、十万次重复调用测试 |
| HTTP | HTTP/1.1、Router、Query/Header/Cookie、请求体硬上限、稳定 4xx/5xx |
| 应用 | Middleware、静态文件 ETag、上传、CORS、signed cookie、可信代理、热重载 |
| 生产 | Stream、SSE、Range、多进程 Worker、健康检查、结构化日志、Prometheus metrics |


{% hint style="info" %}
生产部署应位于 Caddy、Nginx 或云负载均衡器之后，由代理终止 TLS/HTTP/2；WebSocket 不属于 v1.4。仅当所有直连客户端都是可信代理时启用 web.trust_proxy。
{% endhint %}


## 11.3 使用 HHY Web 框架

HHY Web 是构建在 v1.4.3 Web Runtime 之上的独立轻量框架。它借鉴 Flask 的易用 API 和 Go 的显式部署方式，提供应用工厂、五种 HTTP 路由、Middleware、Blueprint、统一 Problem JSON、Bearer 鉴权、上传、Stream 与 SSE 助手，但不复制 Router、Worker 或网络栈。


[在 GitHub 查看 HHY Web ↗](https://github.com/hh696-wq/hhy-web)

公开仓库包含中英文文档、Hello World、完整 JSON API、CLI、单测与真实 HTTP 冒烟测试。


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
        message: "你好，HHY Web！",
        request_id: request.id
    })
}

hhyweb.minimal()
    |> hhyweb.get("/", hello)
    |> hhyweb.serve({ host: "127.0.0.1", port: 8000, workers: 1 })
```


{% hint style="info" %}
HHY Web 当前版本为 0.1.0，要求 HHY Language v1.4.3 或更高版本。直接使用 import web 仍是稳定且受支持的底层方式。
{% endhint %}
