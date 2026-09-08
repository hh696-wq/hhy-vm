# 3. Flow 与 Stream

理解管道传值、惰性流和单次消费语义。

## 3.1 Pipe 如何传值

Pipe 是普通函数调用的组合规则：x |> f 等价于 f(x)，x |> f(a) 等价于 f(x, a)，x |> obj.f(a) 等价于 obj.f(x, a)。它不会自动把标量变成 Stream、展开嵌套 Stream、访问 it 字段、忽略错误、字符串化值或启动 Shell。


```hhy
[1, 2, 3, 4, 5]
    |> stream
    |> map { number -> number * 2 }
    |> where { number -> number > 5 }
    |> take(2)
    |> print
```


## 3.2 Stream 的生命周期

Stream 是惰性、拉取式、单次消费序列。创建管道只组合 operator；终端开始拉取时，上游才逐项产生数据。生命周期是 open → next* → close，正常结束、take 提前停止、错误和取消都会从下游向上游关闭资源。


| 阶段 | 发生的事情 |
| --- | --- |
| 创建 | Source 返回 Stream，但尚未读取数据 |
| 组合 | map、where 等 operator 连接成 Pipeline |
| 消费 | print、collect、save 等终端开始逐项拉取 |
| 关闭 | 完成、提前停止、Error 或取消释放上游资源 |


{% hint style="info" %}
Stream 只能消费一次。不要把同一个 Stream 保存后交给两条 Pipeline；需要重复处理时重新创建 Source，或在有限输入上显式 collect。
{% endhint %}


## 3.3 逐项、过滤与观察算子

```hhy
[5, 2, 5, 1, 3]
    |> stream
    |> skip(1)
    |> take(4)
    |> inspect { number -> print("seen {number}") }
    |> where { number -> number >= 3 }
    |> map { number -> number * 10 }
    |> distinct
    |> collect
    |> print
```


### `map`

```text
map(Stream<T>, Function(T -> U)) -> Stream<U>
```

惰性地对每项调用闭包，一项输入对应一项输出，不自动展开。

### `where`

```text
where(Stream<T>, Function(T -> Bool)) -> Stream<T>
```

惰性保留闭包返回 true 的项目；闭包必须返回 Bool。

### `take`

```text
take(Stream<T>, Int) -> Stream<T>
```

惰性保留前 n 项，达到数量后提前关闭上游。

### `skip`

```text
skip(Stream<T>, Int) -> Stream<T>
```

惰性丢弃前 n 项，然后传递其余项目。

### `inspect`

```text
inspect(Stream<T>, Function(T -> Value)) -> Stream<T>
```

为每项执行观察闭包，再原样传递项目。

### `distinct`

```text
distinct(Stream<Hashable>) -> Stream<Hashable>
```

惰性去除重复的可 Hash 标量，并保存已见集合。


## 3.4 map 与 flat_map 的区别

map 的闭包返回什么，下游就收到什么。如果返回 Stream，结果是 Stream<Stream<T>>。flat_map 要求闭包返回 Stream，并把每个子流依次展开成一条 Stream。


```hhy
let batches = [[1, 2], [3, 4]]

batches
    |> stream
    |> flat_map { batch -> batch |> stream }
    |> print
```


## 3.5 Barrier 和终端算子到底做什么

逐项算子只需保存当前项；Barrier 必须先看完或保存大量输入才能产生正确结果。sort_by 要保存全部输入后排序，group_by 要保存每组的全部 values，collect 把全部项组成 List，reduce/count/sum 等终端算子读取到结束才返回标量。它们都受 max_memory、集合大小和运行时间限制。


```hhy
let ordered = [5, 1, 3, 2, 4]
    |> stream
    |> sort_by({ order: "asc" }) { number -> number }
    |> collect

let grouped = [
    { team: "core", name: "Ada" },
    { team: "web", name: "Linus" },
    { team: "core", name: "Grace" }
]
    |> stream
    |> group_by { person -> person.team }
    |> collect

print(ordered)
print(grouped)
```


### `sort_by`

```text
sort_by(Stream<T>, Map, Function(T -> Comparable)) -> Stream<T>
```

物化有限输入，按闭包 key 和 asc/desc 选项稳定排序。

### `group_by`

```text
group_by(Stream<T>, Function(T -> Hashable)) -> Stream<Group<T>>
```

物化有限输入并按 Hash key 输出 Group；Group 含 key 与 values。

### `collect`

```text
collect(Stream<T>) -> List<T>
```

消费有限 Stream 并物化为 List。

### `reduce`

```text
reduce(Stream<T>, U, Function(State<T,U> -> U)) -> U
```

以 initial 累积 Stream；闭包接收含 acc/item/index 的 state。

### `count`

```text
count(Stream<T>) -> Int
```

消费 Stream 并返回项目数。

### `sum`

```text
sum(Stream<Number>) -> Number
```

消费数值 Stream 并求和，遵守 Int 溢出规则。

### `min`

```text
min(Stream<Number>) -> Number | Null
```

消费数值 Stream，返回最小值或空流的 null。

### `max`

```text
max(Stream<Number>) -> Number | Null
```

消费数值 Stream，返回最大值或空流的 null。

### `first`

```text
first(Stream<T>) -> T | Null
```

返回第一项或 null，并提前关闭上游。

### `last`

```text
last(Stream<T>) -> T | Null
```

消费 Stream 并返回最后一项或 null。

### `any`

```text
any(Stream<T>, Function(T -> Bool)) -> Bool
```

任一项目满足谓词即返回 true，并短路关闭上游。

### `all`

```text
all(Stream<T>, Function(T -> Bool)) -> Bool
```

所有项目满足谓词才返回 true；首个 false 时短路。


{% hint style="info" %}
不要把 watch、every 或没有明确上限的输入直接送入 sort_by、group_by 或 collect。先用 take、时间窗口或其他业务边界把输入限制为有限流，否则 Runtime 会产生 PlanError。
{% endhint %}


## 3.6 副作用、错误与并发

只有终端或 Action 才会真正消费 Pipeline。print、for_each、save_*、run 和 send 会执行输出、文件、进程或网络操作。普通 Error 默认终止 Pipeline；需要逐项保留失败时使用 attempt，需要替换整条失败上游时使用 on_error。


parallel(n) 使用隔离 worker 并保持输出顺序。并发数量、可发送值和取消语义在“并发与监听”章节展开。
