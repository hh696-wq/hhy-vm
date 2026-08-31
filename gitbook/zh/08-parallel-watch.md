# 8. 并发与监听

有界并发处理、取消和文件事件流。

## 8.1 parallel 是有界并发 map

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


| 行为 | 保证 |
| --- | --- |
| 并发上限 | 最多运行 n 个 worker，并受 RuntimeLimits 约束 |
| 输出顺序 | 与输入顺序一致 |
| 背压 | 输入队列和结果缓冲都有界 |
| 错误 | 首个未处理 Error 取消剩余任务 |
| 返回值 | 等同并发 map，不自动展开子 Stream |


如果闭包返回 Stream，在 parallel 后显式使用 flat_map。dry-run 不创建 worker，但仍会按顺序检查闭包中的 Effect。


## 8.2 Sendable 与隔离

worker 收到输入和闭包捕获值的冻结快照，不共享可变对象。Null、Bool、数字、String、单位、Path，以及字段均可发送的普通 List/Map/系统快照可复制过去。


{% hint style="info" %}
捕获 let mut Cell、Stream、打开的 File handle、请求 body stream 或其他进程内资源会产生 CheckError。HHY V1.2.0 不公开线程、锁或 async/await。
{% endhint %}


## 8.3 watch 与 FileEvent

```hhy
watch(path("./src"))
    |> where { event -> event.kind == "write" }
    |> debounce(300ms)
    |> for_each { event ->
    print(event.path)
}
```


watch(path, { recursive? }) 返回无限 Stream<FileEvent>。FileEvent 有 kind、path、old_path、timestamp 只读字段；kind 是 created、modified、removed 或 renamed，old_path 仅 renamed 时存在。


| 字段 | 内容 |
| --- | --- |
| kind | created、modified、removed 或 renamed |
| path | 事件目标 Path |
| old_path | renamed 的原 Path，其余事件为 null |
| timestamp | 事件 DateTime |


watch 是无限 Stream。使用 Ctrl+C、timeout 或 cancel 结束监听；递归监听受 max_open_files 限制。底层文件系统可能合并短时间内的重复事件。


## 8.4 debounce 与 every

debounce(window) 使用 leading-edge：某个值或同一 kind + path 的 FileEvent 第一项立即输出，窗口内重复项被合并，并从最后一次重复重新计时；不同事件 key 互不阻塞。


every(duration) 返回无限 tick Stream。若下游还在处理，上游遵守背压，不重叠执行同一个 tick。定时和监听流进入 collect/sort/group 前必须先有 take 或业务窗口。


## 8.5 取消与清理

Ctrl+C、timeout、cancel() 和未处理错误触发同一个根 CancellationToken。watcher、sleep、HTTP、子进程与 worker 定期检查它；取消后关闭队列和句柄、终止子进程，并让 Stream close 从下游传播到上游。
