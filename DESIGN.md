# Compilation Stages

## Parsing

- Tokenization
- AST generation

## Reification

- Dependency resolution
- Compile time evaluation
  - Constant folding
  - Type reflection and synthesis
- Type inference

Takes the AST representation of the program as an input.

For each fragment, produces annotations marking types, compile time evaluation (where applicable), and locals.

## Generation (C Backend)

- Temporary folding
- Code generation
