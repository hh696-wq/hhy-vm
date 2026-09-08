# 7. HTTP

构建请求，配置超时与重试，并处理响应。

## 7.1 Request → Policy → Send → Response

```hhy
http.get("https://example.com/users")
    |> timeout(5s)
    |> retry({ count: 3, backoff: 200ms })
    |> send
    |> response_body
    |> parse_json
    |> print
```


http.get/post/put/delete(url, options?) 只创建不可变 HttpRequest，不访问网络。timeout(request, duration) 与 retry(request, options) 返回修改策略后的新 Request；send(request) 才产生 network effect 并返回 HttpResponse。这样 dry-run 能完整展示而不执行请求。


## 7.2 请求选项与安全默认值

| 选项 | 用途 |
| --- | --- |
| query | URL 查询参数 |
| headers | 请求 header |
| body | 请求内容 |
| proxy | 代理地址 |
| follow_redirects | 重定向策略 |
| allow_private_networks | 设为 false 时在实际连接地址上阻止私网、loopback 与 link-local |


TLS 验证默认开启。Authorization 和 Cookie 等敏感 header 会在计划、日志和 Error 中脱敏。响应体受 max_http_body 限制。


## 7.3 超时、重试与幂等性

retry({ count, backoff }) 默认只重试连接错误、timeout、429 和部分 5xx。GET、PUT、DELETE 可以按策略重试；POST 默认不自动重试，避免重复创建或扣款。timeout 和 Ctrl+C 都会取消 libcurl 操作并清理响应资源。


{% hint style="info" %}
重试不是让失败消失。为每个请求设置 timeout，谨慎评估 POST 幂等性，并让最终错误保留 URL（脱敏）、方法、尝试次数和 Flow stage。
{% endhint %}


## 7.4 HttpResponse 与响应 body

send 返回内存中的 HttpResponse。使用 response_body 读取 UTF-8 文本，使用 response_bytes 读取二进制；send_to(request, path) 则在 curl 回调中直接写同目录临时文件，成功后原子发布，响应只保留 path 和 size。


```hhy
http.get("https://api.example.com/status")
    |> timeout(3s)
    |> send
    |> response_body
    |> parse_json
    |> print
```


## 7.5 查阅完整 API

[HTTP API Reference →](/zh/learn/standard-library#fn-http-get)

查阅请求构造、timeout、retry、send 和响应读取函数。
