# 4. 文件与路径

遍历目录、读取文本并安全写入结果。

## 4.1 Path 不是 String

所有文件 API 都要求 Path。path(text) 做词法规范化：折叠重复分隔符和 .，消除可以解析的 ..，但不访问文件系统也不解析符号链接。相对 Path 始终基于进程启动目录，不随被 import 的文件位置变化。


```hhy
let source = path("./src/../src/main.c")
let target = path_join(source.parent, "runtime.c")

print(source)
print(source.name)
print(source.extension)
print(source.parent)
print(target)
```


name、extension、parent 是 Path 的只读字段，不是 path_name()、path_extension()、path_parent() 函数。extension 包含前导点，无扩展名时为空字符串；path_join(base, child) 返回组合后的新 Path。


## 4.2 files：遍历、glob 与元数据

```hhy
path("./logs")
    |> files("**/*.log")
    |> where { file -> file.size > 1mib }
    |> flat_map { file -> read_lines(file.path) }
    |> where { line -> contains(line, "ERROR") }
    |> save_lines(path("errors.txt"))
```


files(root, pattern, options?) 返回惰性的 Stream<File | Directory>，不会返回遍历根本身。pattern 支持 *、?、**；默认不跟随目录符号链接，{ follow_symlinks: true } 可开启并自动跳过检测到的目录循环。


| 字段 | 含义 |
| --- | --- |
| path | 完整 Path |
| name | 文件或目录名 |
| extension | 包含前导点的扩展名 |
| size | Bytes 大小 |
| created | 创建时间；不可可靠取得时为 null |
| modified | 修改时间 |
| is_file / is_dir / is_symlink | 对象种类标记 |


File 和 Directory 是系统对象，不是 Map。写入 JSON 前，先把需要的字段映射成普通 Map。


## 4.3 读取文本与二进制

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


{% hint style="info" %}
文本 API 验证 UTF-8。图片、压缩包等任意二进制使用 read_bytes 和 write_bytes，不要放进 String。
{% endhint %}


## 4.4 写入、追加与原子保存

```hhy
let input = path("notes.txt")
let backup = path("backup/notes.txt")

write_text(input, "first line
", { overwrite: true })
append_text(input, "second line
")
copy(input, backup, { overwrite: false, create_parents: true })

read_lines(backup)
    |> map { line -> upper(line) }
    |> save_lines(path("backup/upper.txt"), { create_parents: true })
```


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


write_text、write_bytes、save_text、save_lines 的 options 支持 overwrite（默认 true）和 create_parents（默认 false）。这些 API 通过同目录临时文件加 rename 提交；overwrite: false 使用原子 no-replace，避免先检查后写入的竞态覆盖。


## 4.5 复制、移动、删除与 dry-run

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


```sh
hhy run --dry-run backup.hhy
hhy run backup.hhy
```


先检查 dry-run 计划，再执行包含复制、移动或删除的脚本。


{% hint style="info" %}
文件读取、遍历、写入、进程和网络操作都可能被 RuntimeLimits、取消或宿主权限中止。不要依赖 GC 关闭系统资源；Runtime 会在正常完成、错误、return、exit 和 cancel 路径显式清理。
{% endhint %}


## 4.6 查阅完整 API

[路径与文件 API Reference →](/zh/learn/standard-library#fn-path)

查阅全部签名、参数形式和函数锚点。
