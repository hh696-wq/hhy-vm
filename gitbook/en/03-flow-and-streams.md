# 3. Flow and Streams

Understand pipe injection, lazy streams, and single-consumption semantics.

## 3.1 How Pipe passes values

Pipe is a composition rule for ordinary calls: x |> f means f(x), x |> f(a) means f(x, a), and x |> obj.f(a) means obj.f(x, a). It does not turn scalars into Streams, flatten nested Streams, access it fields, ignore errors, stringify values, or invoke a shell.


```hhy
[1, 2, 3, 4, 5]
    |> stream
    |> map { number -> number * 2 }
    |> where { number -> number > 5 }
    |> take(2)
    |> print
```


## 3.2 Stream lifecycle

A Stream is a lazy, pull-based, single-consumption sequence. Building a pipeline only composes operators; upstream produces items when a terminal starts pulling. Its lifecycle is open → next* → close, and normal completion, early take, errors, and cancellation all close resources upstream.


| Stage | What happens |
| --- | --- |
| Create | A Source returns a Stream without reading data |
| Compose | Operators such as map and where form a Pipeline |
| Consume | A terminal such as print, collect, or save starts pulling |
| Close | Completion, early stop, Error, or cancellation releases upstream resources |


{% hint style="info" %}
A Stream is consumed once. Do not save one Stream and feed two Pipelines; recreate the Source, or explicitly collect finite input.
{% endhint %}


## 3.3 Item, filter, and observation operators

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

Lazily transform each item one-to-one without automatic flattening.

### `where`

```text
where(Stream<T>, Function(T -> Bool)) -> Stream<T>
```

Lazily retain items whose predicate returns Bool true.

### `take`

```text
take(Stream<T>, Int) -> Stream<T>
```

Lazily retain the first n items and close upstream early.

### `skip`

```text
skip(Stream<T>, Int) -> Stream<T>
```

Lazily discard the first n items and pass the remainder.

### `inspect`

```text
inspect(Stream<T>, Function(T -> Value)) -> Stream<T>
```

Run an observation closure for each item and pass the item unchanged.

### `distinct`

```text
distinct(Stream<Hashable>) -> Stream<Hashable>
```

Lazily remove duplicate hashable scalars while retaining a seen set.


## 3.4 map versus flat_map

map sends exactly the closure result downstream. Returning a Stream therefore creates Stream<Stream<T>>. flat_map requires a Stream result and concatenates each child stream into one Stream.


```hhy
let batches = [[1, 2], [3, 4]]

batches
    |> stream
    |> flat_map { batch -> batch |> stream }
    |> print
```


## 3.5 What barriers and terminals actually do

Item operators retain only the current item. A barrier must inspect or retain substantial input before producing a correct result: sort_by stores all input before sorting, group_by stores every group's values, collect builds a List, and terminals such as reduce/count/sum read to completion before returning a scalar. All obey memory, collection-size, and runtime limits.


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

Materialize finite input and stably sort by closure key and asc/desc option.

### `group_by`

```text
group_by(Stream<T>, Function(T -> Hashable)) -> Stream<Group<T>>
```

Materialize finite input into Groups containing key and values.

### `collect`

```text
collect(Stream<T>) -> List<T>
```

Consume a finite Stream and materialize it as a List.

### `reduce`

```text
reduce(Stream<T>, U, Function(State<T,U> -> U)) -> U
```

Fold a Stream from initial; the closure receives state with acc/item/index.

### `count`

```text
count(Stream<T>) -> Int
```

Consume a Stream and return its item count.

### `sum`

```text
sum(Stream<Number>) -> Number
```

Consume and sum a numeric Stream, respecting Int overflow rules.

### `min`

```text
min(Stream<Number>) -> Number | Null
```

Consume a numeric Stream and return its minimum or null for empty input.

### `max`

```text
max(Stream<Number>) -> Number | Null
```

Consume a numeric Stream and return its maximum or null for empty input.

### `first`

```text
first(Stream<T>) -> T | Null
```

Return the first item or null and close upstream early.

### `last`

```text
last(Stream<T>) -> T | Null
```

Consume a Stream and return its last item or null.

### `any`

```text
any(Stream<T>, Function(T -> Bool)) -> Bool
```

Return true on the first matching item and short-circuit upstream.

### `all`

```text
all(Stream<T>, Function(T -> Bool)) -> Bool
```

Return true only if every item matches; short-circuit on the first false.


{% hint style="info" %}
Do not feed watch, every, or otherwise unbounded input directly into sort_by, group_by, or collect. Apply take, a time window, or another business bound first, or the Runtime raises PlanError.
{% endhint %}


## 3.6 Effects, errors, and parallelism

Only a terminal or Action consumes a Pipeline. print, for_each, save_*, run, and send perform output, filesystem, process, or network work. Ordinary Errors terminate the Pipeline; use attempt for per-item Results and on_error to replace a failed upstream Stream.


parallel(n) uses isolated workers and preserves output order. Parallel and Watch covers concurrency limits, Sendable values, and cancellation.
