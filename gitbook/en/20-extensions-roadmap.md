# 20. Extension System

The v1.2.0 process-extension model: the signed official Registry, source builds, manifests, capabilities, Protocol 1, and callable registration.

## 20.1 The current extension boundary

{% hint style="info" %}
v1.2.0 implements local install/list/remove, an Ed25519-signed Registry, manifest and SHA-256 validation, isolated-process handshakes, dynamic callable registration, synchronous calls, structured errors, and shutdown. Scripts can directly import installed packages.
{% endhint %}


| Capability | Current status | Boundary |
| --- | --- | --- |
| Extension distribution | Implemented | Signed official Registry or local source builds; identity, target, signatures, and file hashes are verified before installation |
| Process protocol | Implemented | handshake, register, call, call_result, error, shutdown |
| Value transport | Implemented | JSON protocol mapping for Null, Bool, numbers, String, List, and Map |
| Stream / handle / cancel | Not implemented | Reserved for a future protocol extension |
| Public Native ABI | Not committed | Evaluate only if measurements show the process model is insufficient |


A package name is its top-level namespace: package_name may register only package_name.* and cannot replace hhy.*, std.*, core callables, or another package. Importing an uninstalled package raises ModuleNotFoundError.


## 20.2 Available official extensions

| Extension | Version | Published | Status | Capability |
| --- | --- | --- | --- | --- |
| database | 0.2.0 | 2026-08-26 | Released | MySQL/PostgreSQL queries, writes, and transactions |
| html | 0.1.0 | 2026-08-27 | Released | Lexbor CSS selectors, text/attribute reads, and structured extraction |


[Database Extension Guide](/en/learn/database-extension)

Install database 0.2.0 and perform MySQL/PostgreSQL queries, writes, and transactions.


[HTML Extension and Crawler Framework](/en/learn/html-crawler-framework)

Use html 0.1.0 with URL normalization, a safe frontier, deduplication, and SSRF protection.


## 20.3 Where to get extensions

| Source | Best for | Address or action |
| --- | --- | --- |
| Official HHY Registry | Downloading signed official extensions for a specific platform | https://registry.hhylang.dev (index: /index.json; trust root: /root.json) |
| GitHub source | Reviewing code, auditing changes, or building from source | https://github.com/hh696-wq/hhy-vm/tree/main/extensions |
| Local source build | Extension development or a custom local dependency combination | make -C extensions/<name>, then install the local directory |


```sh
# Build and install from source (html example)
git clone https://github.com/hh696-wq/hhy-vm.git
cd hhy-vm
make
make -C extensions/html
./build/hhy install ./extensions/html

# Inspect installed extensions
./build/hhy list
```


[Open the official extension index ↗](https://registry.hhylang.dev/index.json)

One extension version may contain darwin-arm64, linux-x86_64, linux-arm64, and windows-x86_64 targets; the installer selects only its native target.


[Browse extension source on GitHub ↗](https://github.com/hh696-wq/hhy-vm/tree/main/extensions)

Source, hhy.toml manifests, tests, and build scripts for sample, html, and database.


{% hint style="info" %}
GitHub Releases are not currently an extension download channel. The official Registry provides signed distribution; GitHub provides auditable source. Build on the destination operating system and architecture—renaming a macOS binary does not make it usable on Linux or Windows.
{% endhint %}


## 20.4 Install, list, and remove

```sh
./build/hhy install ./path/to/extension
./build/hhy list
./build/hhy remove package-name
```


| Step | Actual behavior |
| --- | --- |
| install | Read hhy.toml; validate package name, author, requires_hhy, protocol, command, and integrity; display capabilities and ask the user to confirm |
| import / load | Recheck installed SHA-256 data, start the extension process, handshake, and register callables |
| list | Display each installed package's name, version, author, protocol, and declared capabilities |
| remove | Delete the local package record and installation directory; subsequent imports fail |


{% hint style="info" %}
The default extension home is ~/.hhy/extensions; set HHY_EXTENSION_HOME for isolated CI or tests. Capabilities are reviewable declarations, not a general operating-system sandbox. Treat third-party extensions as native executables.
{% endhint %}


## 20.5 Generic hhy.toml manifest

```text
[package]
name = "package-name"
version = "0.1.0"
author = "Your Organization"
requires_hhy = ">=1.1,<2.0"

[extension]
kind = "process"
command = "bin/hhy-package"
protocol = "1"

[capabilities]
read = []
write = []
network = []
process = false
```


| Field | Developer constraint |
| --- | --- |
| package.name | Unique top-level namespace using lowercase letters, digits, and hyphens |
| package.author | Shown during install and list to identify official or third-party provenance |
| requires_hhy | Runtime version range checked by the installer |
| extension.command | Must be an executable under the package bin/ directory and cannot escape its root |
| extension.protocol | Currently accepts Protocol 1 |
| capabilities | Declares file, network, and subprocess access for review |


## 20.6 How an extension loads

{% hint style="info" %}
View the interactive diagram for this section on [hhylang.dev](https://hhylang.dev/en/learn/extensions-roadmap).
{% endhint %}


| Stage | Runtime and extension responsibility |
| --- | --- |
| resolve | Runtime resolves import package_name to an installed package, parses its manifest, and validates command integrity |
| spawn | Runtime starts a separate process with --protocol 1 and opens stdin/stdout protocol pipes |
| handshake | Both sides confirm extension_id and protocol_version=1.0 |
| register | The extension sends one registration message; Runtime validates its package namespace and contracts |
| call | Runtime sends serializable arguments; request_id correlates each call and call_result |
| shutdown | Runtime sends shutdown and reaps protocol streams and the child process |


{% hint style="info" %}
Protocol 1 is synchronous and one-call-at-a-time. It does not provide Stream transport, opaque handles, or protocol-level cancellation.
{% endhint %}


## 20.7 What an extension author must implement

| Part | Requirement |
| --- | --- |
| Package | Provide hhy.toml, an in-package executable command, and SHA-256 integrity data verifiable by the installer |
| Startup | Accept only --protocol 1; write protocol messages only to stdout and logs to stderr |
| Handshake | Validate extension_id and protocol_version and return matching identity |
| Register | Send exactly one initial registration; every name must stay in the package namespace and provide a valid contract |
| Call | Return call_result or structured error for each request_id without exposing credentials or sensitive diagnostics |
| Shutdown | Idempotently release connections, memory, and other extension resources |
| Tests | Cover identity mismatch, invalid arguments, extension exit, protocol errors, and resource cleanup |


The extension process does not receive the complete host environment; Runtime passes only arguments explicitly supplied by the script. Errors should remain actionable without including passwords, tokens, complete connection addresses, or other sensitive information.
