# 5. 文本、JSON 与 CSV

处理 UTF-8 文本、正则表达式和结构化数据。

## 5.1 String 与 UTF-8

String 是不可变 UTF-8 字节序列。length 统计 Unicode code point，byte_length 统计编码后的字节；索引返回一个 code point 对应的单字符 String。文本函数返回新值，不修改原 String。


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

移除 String 两端空白。

### `split`

```text
split(String, String) -> List<String>
```

按分隔文本把 String 分割成 List<String>。

### `join`

```text
join(List<String>, String) -> String
```

用分隔文本连接 List<String>。

### `replace`

```text
replace(String, String, String) -> String
```

返回把匹配文本替换后的新 String。

### `contains`

```text
contains(String | List, Value) -> Bool
```

判断 String 是否含子串，或 List 是否含相等值。

### `starts_with`

```text
starts_with(String, String) -> Bool
```

判断 String 是否以指定文本开头。

### `ends_with`

```text
ends_with(String, String) -> Bool
```

判断 String 是否以指定文本结尾。

### `lower`

```text
lower(String) -> String
```

返回 Unicode 小写转换后的新 String。

### `upper`

```text
upper(String) -> String
```

返回 Unicode 大写转换后的新 String。


## 5.2 Regex

Regex 字面量写作 /pattern/flags，支持 i（忽略大小写）、m（多行）、s（点匹配换行）和 u。regex_match 只返回是否匹配；regex_captures 返回完整匹配、字节位置、groups 编号捕获与 named 命名捕获，不匹配时返回 null。


{% hint style="info" %}
V1.2.0 使用 PCRE2 8-bit，并限制 pattern、subject、match、depth、heap 和捕获组数量；超限产生 ResourceLimitError，避免恶意正则耗尽运行时。
{% endhint %}


## 5.3 JSON 的类型映射与错误

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


parse_json 的错误包含行列。encode_json 可以使用 { pretty: true } 输出可读格式。Function、Stream 和系统对象不能直接编码；先选择普通字段。


## 5.4 CSV 是流式 record

parse_csv 接受完整 String 或 Stream<String>，返回 Stream<Map>；encode_csv 接受 Stream<Map>，返回不带行终止符的 Stream<String>。两者不需要加载完整文件。


```hhy
read_lines(path("employees.csv"))
    |> parse_csv({ header: true })
    |> where { row -> row.active == "true" }
    |> encode_csv({ header: true })
    |> save_lines(path("active-employees.csv"))
```


header 控制首行字段名，delimiter 和 quote 必须是单字符。CSV 不做 schema 推断；数字和 Bool 需要显式转换。encode_csv 不附加换行符，与 save_lines 配合写出。


## 5.5 查阅完整 API

[文本与结构化数据 API Reference →](/zh/learn/standard-library#fn-contains)

查阅文本、Regex、JSON 和 CSV 的完整函数签名。
