# 18. Standard Library Function Index

Signatures and purposes for all 96 V1.2.0 core callables in the runtime Registry.

## 18.1 Reading the signatures

This page is sourced from the V1.2.0 Runtime Callable Contract Registry and contains all 96 core callables; dynamically registered callables are documented by their extensions. T/U are generic placeholders, ? marks an optional argument or nullable result, and Map? is an optional options Map. Every function supports ordinary calls; a pipe injects its left value as the first argument.


{% hint style="info" %}
This is the complete callable list. It excludes read-only special values such as args, env, and system, and does not mislabel read-only fields such as File.path or HttpResponse.status as functions.
{% endhint %}


## 18.2 Core values, collections, environment, and control (22)

### `print`

```text
print(Value...) -> Null
```

Write values to stdout; a Stream is consumed and printed item by item.

### `print_error`

```text
print_error(Value...) -> Null
```

Write values to stderr for diagnostics.

### `exit`

```text
exit(Int?) -> Never
```

End the script with an optional status code (default 0) and unwind resources.

### `length`

```text
length(String | List | Map) -> Int
```

Return String code points or List/Map elements; use count for a Stream.

### `byte_length`

```text
byte_length(String | BytesBuffer) -> Int
```

Return the UTF-8 byte count of String or size of BytesBuffer.

### `type`

```text
type(Value) -> String
```

Return a value's logical type name.

### `is_type`

```text
is_type(Value, String) -> Bool
```

Test whether a value has the named logical type.

### `to_int`

```text
to_int(Int | Float | String) -> Int
```

Explicitly convert Int/Float/String to Int; invalid or overflowing input raises ValueError.

### `to_float`

```text
to_float(Int | Float | String) -> Float
```

Explicitly convert Int/Float/String to Float; invalid input raises ValueError.

### `get`

```text
get(List | Map | Record, Int | String) -> Value | Null
```

Safely read a List index, Map key, or record field; missing values return null.

### `require`

```text
require(Map, String) -> Value
```

Read a required Map key; missing raises KeyError, while a present null stays null.

### `pick`

```text
pick(Map, List<String>) -> Map
```

Return a new Map containing selected keys, preserving present null fields.

### `put`

```text
put(Map, String, Value) -> Map
```

Return a new Map with one key inserted or replaced; the original is unchanged.

### `remove_key`

```text
remove_key(Map, String) -> Map
```

Return a new Map without the named key.

### `append`

```text
append(List<T>, T) -> List<T>
```

Return a new List with one item appended.

### `remove_at`

```text
remove_at(List<T>, Int) -> List<T>
```

Return a new List without the indexed item; out of range raises IndexError.

### `now`

```text
now() -> DateTime
```

Return the current zoned DateTime.

### `datetime.parse`

```text
datetime.parse(String, String, String) -> DateTime
```

Parse DateTime using an explicit format and timezone; invalid input raises ValueError.

### `require_env`

```text
require_env(String) -> String
```

Read a required environment variable; missing raises KeyError.

### `sleep`

```text
sleep(Duration) -> Null
```

Wait for a Duration while remaining cancellable.

### `cancel`

```text
cancel() -> Never
```

Trigger the execution's root cancellation token and begin cleanup.

### `throw`

```text
throw(Error) -> Never
```

Throw an Error through the call stack or Flow.


## 18.3 Flow and Stream (25)

Transformations such as map, where, and take stay lazy; terminals such as collect, count, and reduce consume the Stream. sort_by and group_by materialize input within resource limits. parallel uses bounded isolated workers and preserves output order.


### `stream`

```text
stream(List<T> | Map | Range) -> Stream<T>
```

Convert a List, Map entries, or Range into a lazy single-use Stream.

### `range`

```text
range(Int, Int) -> Stream<Int>
```

Create an Int Stream from start up to but excluding end.

### `map`

```text
map(Stream<T>, Function(T -> U)) -> Stream<U>
```

Lazily transform each item one-to-one without automatic flattening.

### `flat_map`

```text
flat_map(Stream<T>, Function(T -> Stream<U>)) -> Stream<U>
```

Return a child Stream per item and lazily concatenate child streams.

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

### `debounce`

```text
debounce(Stream<T>, Duration) -> Stream<T>
```

Coalesce rapid events within a Duration, commonly for watch streams.

### `on_error`

```text
on_error(Stream<T>, Function(Error -> Stream<T>)) -> Stream<T>
```

On Stream failure, invoke a closure whose returned Stream supplies recovery output.

### `parallel`

```text
parallel(Stream<T>, Int, Function(T -> U)) -> Stream<U>
```

Process with at most n isolated workers, ordered output, bounded buffering, and fail-fast errors.

### `collect`

```text
collect(Stream<T>) -> List<T>
```

Consume a finite Stream and materialize it as a List.

### `count`

```text
count(Stream<T>) -> Int
```

Consume a Stream and return its item count.

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

### `sum`

```text
sum(Stream<Number>) -> Number
```

Consume and sum a numeric Stream, respecting Int overflow rules.

### `reduce`

```text
reduce(Stream<T>, U, Function(State<T,U> -> U)) -> U
```

Fold a Stream from initial; the closure receives state with acc/item/index.

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

### `for_each`

```text
for_each(Stream<T>, Function(T -> Value)) -> Null
```

Consume a Stream, execute a closure for each item, and return null.


## 18.4 Text, Regex, JSON, and CSV (17)

### `contains`

```text
contains(String | List, Value) -> Bool
```

Test whether a String contains a substring or a List contains an equal value.

### `upper`

```text
upper(String) -> String
```

Return a new String converted to Unicode uppercase.

### `lower`

```text
lower(String) -> String
```

Return a new String converted to Unicode lowercase.

### `trim`

```text
trim(String) -> String
```

Remove whitespace from both ends of a String.

### `trim_start`

```text
trim_start(String) -> String
```

Remove leading whitespace from a String.

### `trim_end`

```text
trim_end(String) -> String
```

Remove trailing whitespace from a String.

### `starts_with`

```text
starts_with(String, String) -> Bool
```

Test whether a String starts with the given text.

### `ends_with`

```text
ends_with(String, String) -> Bool
```

Test whether a String ends with the given text.

### `replace`

```text
replace(String, String, String) -> String
```

Return a new String with matching text replaced.

### `split`

```text
split(String, String) -> List<String>
```

Split a String by delimiter text into List<String>.

### `join`

```text
join(List<String>, String) -> String
```

Join List<String> with delimiter text.

### `regex_match`

```text
regex_match(String, Regex) -> Bool
```

Test a String against a PCRE2 Regex under regex resource limits.

### `regex_captures`

```text
regex_captures(String, Regex) -> Map | Null
```

Return full match, byte positions, numbered and named captures; null when unmatched.

### `url_resolve`

```text
url_resolve(String, String?) -> Map
```

Resolve an absolute or relative HTTP(S) URL, remove fragments, default ports, and dot segments, and return host, path, and a stable fingerprint.

### `parse_json`

```text
parse_json(String) -> JsonValue
```

Strictly parse JSON String into ordinary HHY values with line/column errors.

### `encode_json`

```text
encode_json(JsonValue, Map?) -> String
```

Encode supported ordinary values as JSON; options may enable pretty output.

### `parse_csv`

```text
parse_csv(String | Stream<String>, Map?) -> Stream<Map>
```

Stream-parse a String or line Stream into Stream<Map>.

### `encode_csv`

```text
encode_csv(Stream<Map>, Map?) -> Stream<String>
```

Stream-encode Stream<Map> into CSV records without line terminators.


## 18.5 Paths, files, and watch (15)

read_* functions read data, write_* functions write directly, and save_* functions use a temporary file plus atomic replacement. Dry-run intercepts filesystem actions.


### `path`

```text
path(String) -> Path
```

Lexically normalize String into Path without filesystem access.

### `path_join`

```text
path_join(Path, String | Path) -> Path
```

Combine a Path with a child path and return a normalized Path.

### `files`

```text
files(Path, String, Map?) -> Stream<File | Directory>
```

Lazily walk a root with a glob and return a File/Directory Stream.

### `read_text`

```text
read_text(Path) -> String
```

Read an entire UTF-8 file as String.

### `read_lines`

```text
read_lines(Path) -> Stream<String>
```

Lazily read UTF-8 lines with terminators removed.

### `read_bytes`

```text
read_bytes(Path) -> BytesBuffer
```

Read an entire binary file as BytesBuffer.

### `write_text`

```text
write_text(Path, String, Map?) -> Path
```

Atomically replace with String, supporting overwrite/create_parents.

### `append_text`

```text
append_text(Path, String) -> Path
```

Append String to the end of a file.

### `write_bytes`

```text
write_bytes(Path, BytesBuffer, Map?) -> Path
```

Atomically replace a file with BytesBuffer.

### `save_text`

```text
save_text(String | Stream<String>, Path, Map?) -> Path
```

Atomically save a String or pull a text Stream directly to disk.

### `save_lines`

```text
save_lines(Stream<String>, Path, Map?) -> Path
```

Write a String Stream with LF per item and atomically replace the target.

### `copy`

```text
copy(Path, Path, Map?) -> Path
```

Copy a file with atomic no-replace and parent creation options.

### `move`

```text
move(Path, Path, Map?) -> Path
```

Move or rename a file while respecting overwrite options.

### `remove`

```text
remove(Path) -> Path
```

Remove an explicit Path and return it.

### `watch`

```text
watch(Path, Map?) -> Stream<FileEvent>
```

Return an infinite FileEvent Stream with recursive option and cancellation.


## 18.6 Processes, standard input, and timers (6)

run passes argv directly without a shell; only shell explicitly uses shell parsing. Process launches obey timeout, output, and process-count limits.


### `run`

```text
run(List<String>, Map?) -> CommandResult
```

Execute argv directly without a shell and return CommandResult.

### `shell`

```text
shell(String, Map?) -> CommandResult
```

Explicitly execute a String through a shell for redirects, pipes, and shell syntax.

### `stdout_lines`

```text
stdout_lines(CommandResult) -> Stream<String>
```

Expose CommandResult.stdout as a lazy line Stream.

### `processes`

```text
processes() -> Stream<Process>
```

Return a Stream<Process> snapshot of current processes.

### `stdin_lines`

```text
stdin_lines() -> Stream<String>
```

Lazily read stdin lines until EOF or cancellation.

### `every`

```text
every(Duration) -> Stream<Int>
```

Produce an infinite timer tick Stream at a Duration interval.


## 18.7 HTTP (9)

http.* only builds immutable request plans, timeout/retry transform a plan, and only send performs a network effect. response_body returns UTF-8 text; use response_bytes for binary data.


### `http.get`

```text
http.get(String, Map?) -> HttpRequest
```

Build a GET HttpRequest plan without network I/O.

### `http.post`

```text
http.post(String, Map?) -> HttpRequest
```

Build a POST HttpRequest plan without network I/O.

### `http.put`

```text
http.put(String, Map?) -> HttpRequest
```

Build a PUT HttpRequest plan without network I/O.

### `http.delete`

```text
http.delete(String, Map?) -> HttpRequest
```

Build a DELETE HttpRequest plan without network I/O.

### `timeout`

```text
timeout(HttpRequest, Duration) -> HttpRequest
```

Return a new HttpRequest with its timeout configured.

### `retry`

```text
retry(HttpRequest, Map) -> HttpRequest
```

Return a new HttpRequest configured with retry count and backoff.

### `send`

```text
send(HttpRequest) -> HttpResponse
```

Perform the HttpRequest network effect and return HttpResponse.

### `send_to`

```text
send_to(HttpRequest, Path) -> HttpResponse
```

Stream the HTTP body into an atomic file and return an HttpResponse with path and size.

### `response_body`

```text
response_body(HttpResponse) -> String
```

Validate response status and decode the bounded body as UTF-8 String.

### `response_bytes`

```text
response_bytes(HttpResponse) -> BytesBuffer
```

Validate response status and return the bounded binary BytesBuffer.
