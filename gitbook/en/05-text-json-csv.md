# 5. Text, JSON, and CSV

Process UTF-8 text, regular expressions, and structured data.

## 5.1 String and UTF-8

String is an immutable UTF-8 byte sequence. length counts Unicode code points, byte_length counts encoded bytes, and indexing returns a one-code-point String. Text functions return new values and never mutate the original.


```hhy
let line = "  ERROR: timeout  "

line
    |> trim
    |> replace("ERROR", "WARN")
    |> lower
    |> print
```


### `trim`

```text
trim(String) -> String
```

Remove whitespace from both ends of a String.

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

### `replace`

```text
replace(String, String, String) -> String
```

Return a new String with matching text replaced.

### `contains`

```text
contains(String | List, Value) -> Bool
```

Test whether a String contains a substring or a List contains an equal value.

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

### `lower`

```text
lower(String) -> String
```

Return a new String converted to Unicode lowercase.

### `upper`

```text
upper(String) -> String
```

Return a new String converted to Unicode uppercase.


## 5.2 Regex

A Regex literal is /pattern/flags with i (case-insensitive), m (multiline), s (dot matches newline), and u. regex_match returns Bool; regex_captures returns the full match, byte positions, numbered groups, and named captures, or null when unmatched.


{% hint style="info" %}
V1.2.0 uses PCRE2 8-bit with pattern, subject, match, depth, heap, and capture limits. Exceeding them raises ResourceLimitError.
{% endhint %}


## 5.3 JSON type mapping and errors

```hhy
read_text(path("users.json"))
    |> parse_json
    |> get("users")
    |> stream
    |> where { user -> user.active == true }
    |> collect
    |> encode_json({ pretty: true })
    |> save_text(path("active-users.json"))
```


| JSON | HHY |
| --- | --- |
| object | Map |
| array | List |
| string | String |
| integer | Int |
| decimal | Float |
| true / false | Bool |
| null | Null |


parse_json reports line and column on failure. encode_json accepts { pretty: true } for readable output. Function, Stream, and system objects require mapping to ordinary fields first.


## 5.4 CSV is a record stream

parse_csv accepts a complete String or Stream<String> and returns Stream<Map>; encode_csv accepts Stream<Map> and returns Stream<String> records without terminators. Neither requires a whole-file buffer.


```hhy
read_lines(path("employees.csv"))
    |> parse_csv({ header: true })
    |> where { row -> row.active == "true" }
    |> encode_csv({ header: true })
    |> save_lines(path("active-employees.csv"))
```


header controls field names; delimiter and quote must be single characters. CSV performs no schema inference, so numbers and Bools require explicit conversion. encode_csv omits terminators and composes with save_lines.


## 5.5 Look up the complete API

[Text and structured data API Reference →](/en/learn/standard-library#fn-contains)

Look up complete signatures for text, Regex, JSON, and CSV functions.
