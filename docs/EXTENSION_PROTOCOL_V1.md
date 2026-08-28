# HHY Process Extension Protocol v1

> Runtime implementation: `1.1.4`
> Protocol version: `1.0`

Process extensions communicate over UTF-8 JSON Lines. Runtime writes requests to
the child's stdin, reads responses from stdout, and leaves stderr for diagnostics.
Each line is limited to 1 MiB and duplicate JSON object keys are rejected.

Every message contains:

```json
{
  "type": "call",
  "request_id": "1",
  "extension_id": "database",
  "protocol_version": "1.0"
}
```

`request_id` is stable for a request/response pair. `extension_id` must equal the
manifest package name. An incompatible identity, response type or protocol version
terminates loading or fails the call.

## Lifecycle

1. Runtime validates the installed manifest and both recorded SHA-256 hashes.
2. Runtime starts `extension.command --protocol 1` with stdin/stdout pipes and a
   minimal environment containing only `PATH`.
3. Runtime sends `handshake`; the extension returns `handshake_result`.
4. Extension sends exactly one initial `register` message.
5. Runtime validates and copies callable contracts into its existing registry.
6. Runtime exchanges `call` and `call_result` or `error` messages.
7. Runtime sends `shutdown`, closes pipes and reaps the child process.

## Registration

```json
{
  "type": "register",
  "request_id": "register",
  "extension_id": "sample",
  "protocol_version": "1.0",
  "callables": [{
    "name": "sample.echo",
    "minimum_arity": 1,
    "maximum_arity": 1,
    "input": "Value",
    "output": "Value",
    "effect": "none",
    "lazy": false,
    "cancel": false,
    "sendable": true,
    "action": false,
    "threading": "isolated_process"
  }]
}
```

Names must stay under the package namespace. `hhy.*`, `std.*`, core callables and
duplicates are rejected. Process extensions must declare `isolated_process`.

## Calls and values

```json
{"type":"call","request_id":"7","extension_id":"sample","protocol_version":"1.0","callable":"sample.echo","arguments":[42]}
{"type":"call_result","request_id":"7","extension_id":"sample","protocol_version":"1.0","value":42}
```

The initial implementation accepts Null, Bool, Int, Float, String, List and Map.
System values, functions and streams cannot cross as ordinary JSON values.

Errors carry `kind`, `code` and `message`; Runtime currently maps them to an HHY
`ExtensionError` at the callable stage. Database diagnostics, SQL parameters and
credentials must not be included in protocol errors.

## Deliberately deferred within v1.x

The message names `stream_open/item/credit/close`, `cancel`, `handle_release` and
their lifecycle rules remain reserved by the roadmap. They are not accepted by
the `1.0` implementation yet. This prevents the synchronous bounded-call subset
from pretending to provide stream backpressure or opaque handles before those
contracts have acceptance coverage.
