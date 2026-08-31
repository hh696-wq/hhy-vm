# 4. Files and Paths

Walk directories, read text, and write results safely.

## 4.1 Path is not String

Every filesystem API requires Path. path(text) normalizes lexically: it collapses repeated separators and ., and resolves removable .. segments without accessing the filesystem or resolving symlinks. A relative Path is always based on the process startup directory, not the importing file.


```hhy
let source = path("./src/../src/main.c")
let target = path_join(source.parent, "runtime.c")

print(source)
print(source.name)
print(source.extension)
print(source.parent)
print(target)
```


name, extension, and parent are read-only Path fields—not path_name(), path_extension(), or path_parent() functions. extension includes the leading dot and is empty when absent; path_join(base, child) returns a new combined Path.


## 4.2 files: traversal, globs, and metadata

```hhy
path("./logs")
    |> files("**/*.log")
    |> where { file -> file.size > 1mib }
    |> flat_map { file -> read_lines(file.path) }
    |> where { line -> contains(line, "ERROR") }
    |> save_lines(path("errors.txt"))
```


files(root, pattern, options?) returns a lazy Stream<File | Directory> and excludes the traversal root itself. Patterns support *, ?, and **. Directory symlinks are not followed by default; { follow_symlinks: true } opts in with cycle detection.


| Field | Meaning |
| --- | --- |
| path | Full Path |
| name | File or directory name |
| extension | Extension including its leading dot |
| size | Size as Bytes |
| created | Creation time, or null when unavailable |
| modified | Modification time |
| is_file / is_dir / is_symlink | Object-kind flags |


File and Directory are system objects, not Maps. Map required fields into an ordinary Map before JSON encoding.


## 4.3 Reading text and binary data

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


{% hint style="info" %}
Text APIs validate UTF-8. Use read_bytes and write_bytes for images, archives, and arbitrary binary data rather than storing it in String.
{% endhint %}


## 4.4 Writing, appending, and atomic saves

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


write_text, write_bytes, save_text, and save_lines accept overwrite (default true) and create_parents (default false). They commit through a same-directory temporary file plus rename; overwrite: false uses atomic no-replace to prevent check-then-write races.


## 4.5 Copy, move, remove, and dry-run

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


```sh
hhy run --dry-run backup.hhy
hhy run backup.hhy
```


Review the dry-run plan before executing a script that copies, moves, or removes files.


{% hint style="info" %}
Filesystem reads, traversal, writes, processes, and networking may stop because of RuntimeLimits, cancellation, or host permissions. The Runtime explicitly cleans resources on completion, errors, return, exit, and cancel; it does not rely on GC finalizers.
{% endhint %}


## 4.6 Look up the complete API

[Paths and files API Reference →](/en/learn/standard-library#fn-path)

Look up every signature, parameter form, and stable function anchor.
