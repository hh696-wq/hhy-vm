# 8. Parallel and Watch

Bounded parallel work, cancellation, and filesystem event streams.

## 8.1 parallel is a bounded concurrent map

```hhy
let urls = [
    "https://example.com",
    "https://example.org"
]

urls
    |> parallel(2) { url ->
    http.get(url)
        |> timeout(5s)
        |> send
}
    |> print
```


| Behavior | Guarantee |
| --- | --- |
| Concurrency | At most n workers, subject to RuntimeLimits |
| Output order | Matches input order |
| Backpressure | Input and result buffers are bounded |
| Errors | The first unhandled Error cancels remaining work |
| Return | Concurrent map; child Streams are not flattened |


If a closure returns Stream, apply flat_map after parallel. Dry-run creates no workers but still checks closure Effects in order.


## 8.2 Sendable values and isolation

Workers receive frozen snapshots of input and captured values, never shared mutable objects. Null, Bool, numbers, String, units, Path, and ordinary Lists/Maps/system snapshots whose fields are sendable can be copied.


{% hint style="info" %}
Capturing a let mut Cell, Stream, open File handle, request body stream, or other process-local resource raises CheckError. V1.2.0 exposes no threads, locks, or async/await.
{% endhint %}


## 8.3 watch and FileEvent

```hhy
watch(path("./src"))
    |> where { event -> event.kind == "write" }
    |> debounce(300ms)
    |> for_each { event ->
    print(event.path)
}
```


watch(path, { recursive? }) returns an infinite Stream<FileEvent>. FileEvent exposes read-only kind, path, old_path, and timestamp; kind is created, modified, removed, or renamed, and old_path exists only for renamed.


| Field | Contents |
| --- | --- |
| kind | created, modified, removed, or renamed |
| path | Target Path |
| old_path | Original Path for renamed; otherwise null |
| timestamp | Event DateTime |


watch is an infinite Stream. End it with Ctrl+C, timeout, or cancel. Recursive watch obeys max_open_files, and filesystems may merge rapid duplicate events.


## 8.4 debounce and every

debounce(window) is leading-edge: the first scalar or kind + path FileEvent emits immediately; duplicates inside the window are coalesced and reset the window. Different event keys do not block each other.


every(duration) returns an infinite tick Stream. If downstream is busy, backpressure prevents overlapping the same tick. Apply take or a business window before collect/sort/group.


## 8.5 Cancellation and cleanup

Ctrl+C, timeout, cancel(), and unhandled errors trigger one root CancellationToken. Watchers, sleep, HTTP, child processes, and workers poll it; cancellation closes queues and handles, terminates children, and propagates Stream close upstream.
