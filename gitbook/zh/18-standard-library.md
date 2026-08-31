# 18. 标准库函数索引

运行时 Registry 中全部 96 个 V1.2.0 核心 callable 的签名与用途。

## 18.1 如何阅读签名

本页以 V1.2.0 Runtime 的 Callable Contract Registry 为权威来源，共 96 个核心 callable；扩展动态注册的 callable 在各扩展文档中说明。T/U 表示泛型占位值，? 表示可选参数或可空结果，Map? 表示可选 options Map。所有函数都可普通调用；在管道中，左侧值会注入为第一个参数。


{% hint style="info" %}
这是完整 callable 清单，不含 args、env、system 等只读特殊值，也不把 File.path、HttpResponse.status 等只读字段误列为函数。
{% endhint %}


## 18.2 核心值、集合、环境与控制（22）

### `print`

```text
print(Value...) -> Null
```

把值写到标准输出；传入 Stream 时逐项输出并消费它。

### `print_error`

```text
print_error(Value...) -> Null
```

把值写到标准错误；适合诊断信息。

### `exit`

```text
exit(Int?) -> Never
```

立即以给定状态码结束脚本，省略时使用 0，并触发资源清理。

### `length`

```text
length(String | List | Map) -> Int
```

返回 String 的 code point 数或 List/Map 的元素数；Stream 应使用 count。

### `byte_length`

```text
byte_length(String | BytesBuffer) -> Int
```

返回 String 的 UTF-8 字节数或 BytesBuffer 大小。

### `type`

```text
type(Value) -> String
```

返回值的逻辑类型名。

### `is_type`

```text
is_type(Value, String) -> Bool
```

判断值是否具有指定逻辑类型，返回 Bool。

### `to_int`

```text
to_int(Int | Float | String) -> Int
```

把 Int/Float/String 显式转换为 Int，失败或溢出产生 ValueError。

### `to_float`

```text
to_float(Int | Float | String) -> Float
```

把 Int/Float/String 显式转换为 Float，失败产生 ValueError。

### `get`

```text
get(List | Map | Record, Int | String) -> Value | Null
```

安全读取 List 索引、Map 键或对象字段；缺失返回 null。

### `require`

```text
require(Map, String) -> Value
```

读取必需 Map 键；键缺失产生 KeyError，存在且为 null 时返回 null。

### `pick`

```text
pick(Map, List<String>) -> Map
```

返回只保留指定键的新 Map，并保留存在的 null 字段。

### `put`

```text
put(Map, String, Value) -> Map
```

返回新增或替换一个键的新 Map，不修改原 Map。

### `remove_key`

```text
remove_key(Map, String) -> Map
```

返回移除指定键的新 Map。

### `append`

```text
append(List<T>, T) -> List<T>
```

返回末尾增加一个元素的新 List。

### `remove_at`

```text
remove_at(List<T>, Int) -> List<T>
```

返回移除指定索引的新 List；越界产生 IndexError。

### `now`

```text
now() -> DateTime
```

返回带时区的当前 DateTime。

### `datetime.parse`

```text
datetime.parse(String, String, String) -> DateTime
```

按明确的格式和时区解析 DateTime，非法输入产生 ValueError。

### `require_env`

```text
require_env(String) -> String
```

读取必需环境变量；不存在时产生 KeyError。

### `sleep`

```text
sleep(Duration) -> Null
```

可取消地等待指定 Duration。

### `cancel`

```text
cancel() -> Never
```

触发当前执行的根取消令牌并开始统一清理。

### `throw`

```text
throw(Error) -> Never
```

抛出 Error，并沿调用栈或 Flow 传播。


## 18.3 Flow 与 Stream（25）

map/where/take 等转换保持惰性；collect、count、reduce 等终端操作消费 Stream；sort_by 与 group_by 会在资源上限内物化输入。parallel 使用有界隔离 worker 并保持输出顺序。


### `stream`

```text
stream(List<T> | Map | Range) -> Stream<T>
```

把 List、Map entries 或 Range 转成惰性单次消费 Stream。

### `range`

```text
range(Int, Int) -> Stream<Int>
```

创建从 start 到 end（不含 end）的 Int Stream。

### `map`

```text
map(Stream<T>, Function(T -> U)) -> Stream<U>
```

惰性地对每项调用闭包，一项输入对应一项输出，不自动展开。

### `flat_map`

```text
flat_map(Stream<T>, Function(T -> Stream<U>)) -> Stream<U>
```

对每项返回一个子 Stream，并惰性地把子流依次展开。

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

### `debounce`

```text
debounce(Stream<T>, Duration) -> Stream<T>
```

在指定 Duration 内合并快速连续事件，常用于 watch。

### `on_error`

```text
on_error(Stream<T>, Function(Error -> Stream<T>)) -> Stream<T>
```

当 Stream 失败时调用闭包，用返回的 Stream 恢复或替换后续输出。

### `parallel`

```text
parallel(Stream<T>, Int, Function(T -> U)) -> Stream<U>
```

用最多 n 个隔离 worker 并发处理，保序、有界缓冲且 fail-fast。

### `collect`

```text
collect(Stream<T>) -> List<T>
```

消费有限 Stream 并物化为 List。

### `count`

```text
count(Stream<T>) -> Int
```

消费 Stream 并返回项目数。

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

### `sum`

```text
sum(Stream<Number>) -> Number
```

消费数值 Stream 并求和，遵守 Int 溢出规则。

### `reduce`

```text
reduce(Stream<T>, U, Function(State<T,U> -> U)) -> U
```

以 initial 累积 Stream；闭包接收含 acc/item/index 的 state。

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

### `for_each`

```text
for_each(Stream<T>, Function(T -> Value)) -> Null
```

消费 Stream 并为每项执行闭包，返回 null。


## 18.4 文本、Regex、JSON 与 CSV（17）

### `contains`

```text
contains(String | List, Value) -> Bool
```

判断 String 是否含子串，或 List 是否含相等值。

### `upper`

```text
upper(String) -> String
```

返回 Unicode 大写转换后的新 String。

### `lower`

```text
lower(String) -> String
```

返回 Unicode 小写转换后的新 String。

### `trim`

```text
trim(String) -> String
```

移除 String 两端空白。

### `trim_start`

```text
trim_start(String) -> String
```

移除 String 开头空白。

### `trim_end`

```text
trim_end(String) -> String
```

移除 String 末尾空白。

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

### `replace`

```text
replace(String, String, String) -> String
```

返回把匹配文本替换后的新 String。

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

### `regex_match`

```text
regex_match(String, Regex) -> Bool
```

判断 PCRE2 Regex 是否匹配 String，受正则资源限制。

### `regex_captures`

```text
regex_captures(String, Regex) -> Map | Null
```

返回完整匹配、字节位置、编号和命名捕获；不匹配返回 null。

### `url_resolve`

```text
url_resolve(String, String?) -> Map
```

解析绝对或相对 HTTP(S) URL，移除 fragment、默认端口与点路径，并返回 host、path 和稳定指纹。

### `parse_json`

```text
parse_json(String) -> JsonValue
```

严格解析 JSON String 为普通 HHY 值，错误包含行列。

### `encode_json`

```text
encode_json(JsonValue, Map?) -> String
```

把可编码普通值转成 JSON；options 可启用 pretty。

### `parse_csv`

```text
parse_csv(String | Stream<String>, Map?) -> Stream<Map>
```

把 String 或行 Stream 流式解析成 Stream<Map>。

### `encode_csv`

```text
encode_csv(Stream<Map>, Map?) -> Stream<String>
```

把 Stream<Map> 流式编码为不含换行符的 CSV record Stream。


## 18.5 路径、文件与监听（15）

read_* 是读取操作；write_* 直接写入；save_* 使用临时文件加原子替换。文件系统 action 会被 dry-run 拦截。


### `path`

```text
path(String) -> Path
```

把 String 词法规范化为 Path，不访问文件系统。

### `path_join`

```text
path_join(Path, String | Path) -> Path
```

组合 Path 与子路径并返回规范化的新 Path。

### `files`

```text
files(Path, String, Map?) -> Stream<File | Directory>
```

按 glob 惰性遍历根目录，返回 File/Directory Stream。

### `read_text`

```text
read_text(Path) -> String
```

完整读取 UTF-8 文件为 String。

### `read_lines`

```text
read_lines(Path) -> Stream<String>
```

逐行惰性读取 UTF-8 文件并移除行终止符。

### `read_bytes`

```text
read_bytes(Path) -> BytesBuffer
```

完整读取二进制文件为 BytesBuffer。

### `write_text`

```text
write_text(Path, String, Map?) -> Path
```

以原子替换方式写 String，支持 overwrite/create_parents。

### `append_text`

```text
append_text(Path, String) -> Path
```

把 String 追加到文件末尾。

### `write_bytes`

```text
write_bytes(Path, BytesBuffer, Map?) -> Path
```

以原子替换方式写 BytesBuffer。

### `save_text`

```text
save_text(String | Stream<String>, Path, Map?) -> Path
```

把 String 或文本 Stream 边拉取边原子保存。

### `save_lines`

```text
save_lines(Stream<String>, Path, Map?) -> Path
```

把 String Stream 逐项写入并补 LF，最终原子替换。

### `copy`

```text
copy(Path, Path, Map?) -> Path
```

复制文件，支持原子 no-replace 与创建父目录。

### `move`

```text
move(Path, Path, Map?) -> Path
```

移动或重命名文件，遵守覆盖选项。

### `remove`

```text
remove(Path) -> Path
```

删除明确 Path，并返回该 Path。

### `watch`

```text
watch(Path, Map?) -> Stream<FileEvent>
```

返回无限 FileEvent Stream，支持 recursive 选项并响应取消。


## 18.6 进程、标准输入与定时（6）

run 直接传递 argv，不经过 Shell；只有 shell 明确采用 Shell 解析。进程启动会受 timeout、输出和进程数限制。


### `run`

```text
run(List<String>, Map?) -> CommandResult
```

直接执行 argv，不经过 Shell；返回 CommandResult。

### `shell`

```text
shell(String, Map?) -> CommandResult
```

显式用 Shell 执行 String；仅在需要重定向、管道等 Shell 语义时使用。

### `stdout_lines`

```text
stdout_lines(CommandResult) -> Stream<String>
```

把 CommandResult.stdout 转为惰性行 Stream。

### `processes`

```text
processes() -> Stream<Process>
```

获取当前进程快照的 Stream<Process>。

### `stdin_lines`

```text
stdin_lines() -> Stream<String>
```

惰性读取标准输入行，直到 EOF 或取消。

### `every`

```text
every(Duration) -> Stream<Int>
```

按指定 Duration 产生无限计时 tick Stream。


## 18.7 HTTP（9）

http.* 只构造不可变请求计划，timeout/retry 修改计划，只有 send 产生网络副作用。response_body 返回 UTF-8 文本，二进制响应使用 response_bytes。


### `http.get`

```text
http.get(String, Map?) -> HttpRequest
```

构造 GET HttpRequest 计划，不发送网络请求。

### `http.post`

```text
http.post(String, Map?) -> HttpRequest
```

构造 POST HttpRequest 计划，不发送网络请求。

### `http.put`

```text
http.put(String, Map?) -> HttpRequest
```

构造 PUT HttpRequest 计划，不发送网络请求。

### `http.delete`

```text
http.delete(String, Map?) -> HttpRequest
```

构造 DELETE HttpRequest 计划，不发送网络请求。

### `timeout`

```text
timeout(HttpRequest, Duration) -> HttpRequest
```

返回设置请求超时的新 HttpRequest。

### `retry`

```text
retry(HttpRequest, Map) -> HttpRequest
```

返回配置重试次数和退避的新 HttpRequest。

### `send`

```text
send(HttpRequest) -> HttpResponse
```

执行 HttpRequest 网络副作用并返回 HttpResponse。

### `send_to`

```text
send_to(HttpRequest, Path) -> HttpResponse
```

把 HTTP body 流式写入原子文件，返回包含 path 和 size 的 HttpResponse。

### `response_body`

```text
response_body(HttpResponse) -> String
```

验证响应状态并把有界 body 解码为 UTF-8 String。

### `response_bytes`

```text
response_bytes(HttpResponse) -> BytesBuffer
```

验证响应状态并返回有界二进制 BytesBuffer。
