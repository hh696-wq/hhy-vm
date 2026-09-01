# HHY Experimental Bytecode

HHY v1.3.0-alpha introduces an internal, verifiable Bytecode compiler skeleton.
It does not replace the AST evaluator and does not define a persistent `.hhyc`
format.

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
- `hhy run <file.hhy>` continues to use the AST evaluator.

## Non-commitments

Opcode numeric values and the in-memory Chunk layout are internal during alpha.
There is no Bytecode execution engine, disk cache, external loader, extension
Bytecode input, public ABI, or default-engine switch in this stage.

Any future persistent format must have a separate format version, source and
dependency fingerprints, documented portability, resource limits, and mandatory
verification before execution.
