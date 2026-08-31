# 2. Language Basics

Variables, values, functions, conditions, loops, and scope.

## 2.1 What dynamic typing means

HHY declarations omit types; each value carries its logical type at runtime. Dynamic does not mean coercive: conditions require Bool, String never automatically becomes Number, Bool, or Path, and invalid arity or operations raise structured errors. Use type(value) to inspect a type and is_type(value, name) to test it.


```hhy
let nothing = null
let enabled = true
let count = 42
let ratio = 0.75
let title = "HHY"
let pattern = /ERROR|WARN/i
let names = ["Ada", "Linus"]
let user = { name: "Ada", active: true }
let indexes = 0..3
let size_limit = 10mib
let timeout_limit = 5s
let completion = 80%

print(type(user))
print(is_type(title, "String"))
```


## 2.2 Scalar and unit types

| Type | Example | Use |
| --- | --- | --- |
| Null | null | Absence |
| Bool | true | Conditions and predicates |
| Int | 42 | Integer arithmetic |
| Float | 3.14 | Floating-point arithmetic |
| String | "hello" | UTF-8 text |
| Regex | /ERROR/i | Text matching |
| Bytes | 10mib | File or memory size |
| Duration | 5s | Timeouts and intervals |
| Percent | 80% | Ratios |
| DateTime | now() | Zoned time |
| Path | path("logs") | Filesystem paths |


Precise String, number, unit, and Path edge cases belong in Reference. For ordinary scripts, remember that HHY never implicitly converts among String, Number, Bool, and Path.


[Open the type and syntax reference →](/en/learn/syntax-reference)

Look up exact UTF-8, overflow, operator, and literal behavior.


## 2.3 List, Map, and Range

List indices start at zero and out-of-range access raises IndexError. Map keys are Strings and preserve insertion order; map.key equals map["key"]. A missing key normally returns null, while require distinguishes a missing key from a present key whose value is null. Range a..b includes a and excludes b without allocating a List.


```hhy
let original = ["Flow", "System"]
let extended = append(original, "Pipe")
let shortened = remove_at(extended, 1)

let config = { retries: 3, label: null }
let updated = put(config, "timeout", 5s)
let selected = pick(updated, ["retries", "timeout"])

print(original)
print(shortened)
print(get(config, "missing"))
print(require(config, "label"))
print(selected)
```


Lists and Maps are not mutated in place. append, remove_at, put, remove_key, and pick return new collections, leaving original and config unchanged. Lists and Maps support deep equality; Functions, Streams, and system resources do not support value equality.


## 2.4 Result, Stream, and system objects

| Type | Used for |
| --- | --- |
| Result | Explicit success values or Errors from one operation |
| Stream | Lazy files, lines, processes, responses, and events |
| Error | Failures with category, location, and Flow stage |
| Function | User functions and closures |
| System object | Dedicated values such as File, Process, and HttpResponse |


System objects are not Maps. Map or pick ordinary fields before JSON encoding. Flow explains Stream laziness and consumption in detail.


## 2.5 Bindings, scope, and immutability

```hhy
let service = "api"
let mut retries = 0
retries = retries + 1
```


let creates a binding that cannot be reassigned; use let mut when reassignment is required. List and Map update functions return new collections rather than mutating originals. Names follow block lexical scope and must be declared before use.


{% hint style="info" %}
Closures may capture outer values. A closure that captures let mut cannot be sent to a parallel worker; Parallel and Watch covers this concurrency boundary.
{% endhint %}


## 2.6 Conditions, loops, and functions

```hhy
fn classify(score) {
    if score >= 90 { return "excellent" }
    else if score >= 60 { return "pass" }
    else { return "retry" }
}

let mut total = 0
for score in [98, 72, 55] {
    if score < 60 { continue }
    total = total + score
}

let mut attempts = 0
while attempts < 3 {
    attempts = attempts + 1
}

print(classify(98))
print(total)
```


HHY supports if / else if / else, for item in iterable, while, break, and continue. for iterates Lists, Map entries, Ranges, or Streams; iterating a Stream consumes it. Functions use positional arguments checked at call time and return null without an explicit return.


```hhy
fn summarize(items) {
    let mut total = 0

    for item in items {
        if item.enabled {
            total = total + item.score
        }
    }

    return total
}

let users = [
    { name: "Ada", enabled: true, score: 98 },
    { name: "Linus", enabled: false, score: 86 }
]

summarize(users) |> print
```


A closure is { item -> expression }; a multi-statement closure must name its parameter and use return. A one-argument closure in an unambiguous Flow context may use { it * 2 }. V1.2.0 has no overloading, generics, or default arguments.
