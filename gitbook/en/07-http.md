# 7. HTTP

Build requests, configure timeout and retry, and process responses.

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


http.get/post/put/delete(url, options?) only build immutable HttpRequests without network access. timeout(request, duration) and retry(request, options) return new policy-adjusted Requests; send(request) performs the network effect and returns HttpResponse. Dry-run can therefore inspect the full plan without execution.


## 7.2 Request options and safe defaults

| Option | Purpose |
| --- | --- |
| query | URL query parameters |
| headers | Request headers |
| body | Request content |
| proxy | Proxy address |
| follow_redirects | Redirect policy |
| allow_private_networks | When false, reject private, loopback, and link-local resolved connection addresses |


TLS verification is enabled by default. Sensitive Authorization and Cookie headers are redacted in plans, logs, and Errors. Response bodies obey max_http_body.


## 7.3 Timeout, retries, and idempotency

retry({ count, backoff }) defaults to connection errors, timeouts, 429, and selected 5xx statuses. GET, PUT, and DELETE may retry by policy; POST does not retry automatically to avoid duplicate creation or charges. Timeout and Ctrl+C cancel libcurl work and release response resources.


{% hint style="info" %}
Retries do not erase failure. Give every request a timeout, evaluate POST idempotency, and preserve method, redacted URL, attempts, and Flow stage in the final Error.
{% endhint %}


## 7.4 HttpResponse and response bodies

send returns an in-memory HttpResponse. Use response_body for UTF-8 text and response_bytes for binary data; send_to(request, path) writes directly from curl into a sibling temporary file and atomically publishes it, returning only path and size.


```hhy
http.get("https://api.example.com/status")
    |> timeout(3s)
    |> send
    |> response_body
    |> parse_json
    |> print
```


## 7.5 Look up the complete API

[HTTP API Reference →](/en/learn/standard-library#fn-http-get)

Look up request builders, timeout, retry, send, and response readers.
