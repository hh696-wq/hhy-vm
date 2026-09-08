# 6. Processes and System

Run commands, consume output, and inspect system state.

## 6.1 The safety boundary between run and shell

```hhy
run(["git", "log", "--oneline"], { timeout: 5s })
    |> stdout_lines
    |> take(10)
    |> print
```


run(argv, options?) passes List<String> directly to the OS without a shell, so spaces, globs, $, redirects, and pipes are not reinterpreted. Use shell(command, options?) only when shell syntax is intentional; the Checker emits a safety hint.


| Option | Purpose |
| --- | --- |
| cwd | Child working-directory Path |
| env | Overrides only the child environment |
| stdin | Text supplied on standard input |
| timeout | Maximum command duration |
| max_output | stdout and stderr capture limit |


{% hint style="info" %}
Prefer run when values include user input. shell interprets redirects, pipes, and expansion and should be reserved for intentional shell syntax.
{% endhint %}


## 6.2 CommandResult

run and shell wait by default and return CommandResult. A nonzero child exit code is not automatically an HHY Error; inspect exit_code for business success.


| Field | Contents |
| --- | --- |
| exit_code | Child exit status |
| stdout | Standard-output String |
| stderr | Standard-error String |
| duration | Command Duration |


A nonzero exit_code does not automatically become an HHY Error; interpret it according to the command. stdout_lines(result) exposes captured output as a line Stream.


## 6.3 Process snapshots and fields

processes() returns a current Stream<Process> snapshot. Process is not a Map and exposes read-only pid, name, cpu, memory, status, and command fields. Explicitly map ordinary fields before JSON encoding.


{% hint style="info" %}
sort_by({ order: "desc" }) accepts only asc or desc (default asc). It is a stable barrier that materializes the finite snapshot.
{% endhint %}


## 6.4 args, env, system, and stdin

| Value | Contents |
| --- | --- |
| args | List<String> excluding the script path |
| env | Read-only environment view |
| system | OS, architecture, host, CPU, memory, and directories |
| stdin_lines() | Standard-input line Stream |


```hhy
if length(args) != 1 {
    print_error("usage: script.hhy <input>")
    exit(3)
}

let input = path(args[0])
```


## 6.5 Look up the complete API

[Processes and system API Reference →](/en/learn/standard-library#fn-run)

Look up complete signatures for run, shell, processes, stdin_lines, and every.
