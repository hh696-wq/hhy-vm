# HHY Experimental Bytecode

HHY v1.3.0 provides an opt-in, verified Bytecode execution path. It does not
replace the default AST evaluator and does not define a persistent `.hhyc` format.

## Current boundary

- Parser, Checker, and Resolver remain the shared front end.
- Every AST node lowers to an explicit Opcode instruction.
- A Chunk owns its instruction array and deduplicated UTF-8 constant pool.
- Every instruction records source line and column, child count, and an optional
  constant index.
- The Verifier rejects unknown opcodes, invalid constants, malformed node trees,
  excessive nesting, missing or non-canonical `HALT`, and instructions after
  `HALT`.
- Instruction count, constant count, and nesting depth have hard limits.
- `hhy bytecode <file.hhy>` compiles, verifies, and prints the disassembly.
- `hhy run <file.hhy>` continues to use the AST evaluator by default.
- `hhy run --engine bytecode <file.hhy>` compiles and verifies the source,
  constructs a bounded instruction/frame/operand execution plan, then runs it
  through the shared semantic Runtime.
- The execution frame limit is derived from `RuntimeLimits.max_recursion`.
- AST/Bytecode conformance tests compare stdout, stderr, and exit status across
  closures, errors, Unicode, integer bounds, Maps, Flow, and recursion limits.
- `hhy profile --engine bytecode` labels the selected engine and emits HHY
  function/source locations, including `<bytecode-top-level>`.
- Structured errors carry bounded HHY call stacks with function and source
  positions instead of raw instruction offsets.
- `make benchmark-bytecode` records AST/Bytecode wall time, Bytecode compile
  cost, binary size, commit/platform metadata, and a machine-readable default
  engine decision. A failed gate keeps AST selected.

## Non-commitments

Opcode numeric values and the in-memory Chunk layout remain internal. The engine
deliberately reuses the one
Runtime implementation for Values, closure environments, exceptions, GC roots,
cancellation, Streams, effects, and resource accounting. A dedicated opcode
dispatch loop, disk cache, external loader, extension Bytecode input, public ABI,
  and external Bytecode ABI are not part of v1.3.0. The default engine remains AST
  because the reviewed performance switch gates did not all pass.

Any future persistent format must have a separate format version, source and
dependency fingerprints, documented portability, resource limits, and mandatory
verification before execution.
