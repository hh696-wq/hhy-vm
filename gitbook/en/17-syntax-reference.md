# 17. Complete Syntax Reference

V1.2.0 lexical rules, literals, operators, statements, closures, and module syntax.

## 17.1 Source files and lexical rules

| Item | Rule |
| --- | --- |
| File | .hhy, UTF-8, LF or CRLF |
| Identifier | Case-sensitive ASCII letters, digits, and underscores; cannot start with a digit |
| Statement end | Newline or optional semicolon |
| Continuation | Open delimiters or leading/trailing \|> |
| Comment | # line comment; first line may be a shebang |
| / | Regex at expression start, division after a left operand |


## 17.2 Literals and native units

```hhy
let nothing = null
let flags = [true, false]
let numbers = [42, -10, 0xff, 0b1010, 1.5, 1e6]
let name = "HHY"
let strings = ["hello", "Hello, {name}"]
let pattern = /ERROR|WARN/i
let list = [1, 2, 3]
let record = { name: "Tom", age: 20 }
let interval = 1..10
let units = [10mib, 5s, 80%]
```


Ranges include the start and exclude the end. Bytes support b/kb/mb/gb/kib/mib/gib; Duration supports ns/us/ms/s/min/h; % attached to a number creates Percent. Strings support interpolation and \, ", \n, \r, \t, \b, \f, and \0 escapes.


## 17.3 Operator precedence (highest to lowest)

```text
()  []  .
not  -  +
*  /  %
+  -
<  <=  >  >=
==  !=
and
or
??
|>
=
```


and, or, and ?? short-circuit; = may assign only to a let mut binding; |> is left-associative. Conditions require Bool—0, empty strings, and null are not implicitly false.


## 17.4 Declarations, control flow, functions, and modules

```hhy
let name = "HHY"
let mut count = 0
count = count + 1
let enabled = true
let items = ["Flow", "Pipe"]

if enabled { print("yes") } else { print("no") }
for item in items { print(item) }
while count < 3 { count = count + 1 }

fn add(a, b) { return a + b }
let doubled = [1, 2] |> stream |> map { number -> number * 2 } |> collect

try { read_text(path("config.json")) } catch err { print_error(err) }
let result = attempt { read_text(path("config.json")) }

import { add as sum_two } from "./math.hhy"
export fn public_api(value) { return value }
```


| Construct | Form |
| --- | --- |
| Call | name(args) |
| Pipe | x \|> f(a) equals f(x, a) |
| Closure | { param -> expression } |
| Map | { key: value } |
| Control flow | if, for, while, break, continue, return |
| Module | import, as, export |


## 17.5 Core value types

```text
Null Bool Int Float String Regex BytesBuffer
List Map Range Function Error Result Stream
Bytes Duration Percent DateTime Path
File Directory FileEvent Process CommandResult
HttpRequest HttpResponse
```


{% hint style="info" %}
HHY is dynamically typed but does not perform dangerous String/Number or String/Bool coercions. Int is signed 64-bit and Float is IEEE 754 double.
{% endhint %}
