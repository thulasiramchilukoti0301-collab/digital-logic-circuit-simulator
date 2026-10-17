# Decision 001: SQLite Circuit Persistence

## Context

The original project scope was a React visual editor connected to a C++17 simulation engine through an HTTP API. It did not include a database.

A later requirement from the professor calls for database integration and complete features across the frontend, backend, and database.

## Decision

Retain the C++ backend and use SQLite to persist named circuit definitions.

## Rationale

- The application runs locally, so SQLite avoids requiring a separate database server.
- SQLite integrates directly with the C++ application and its build.
- Transactions allow a circuit and its related components and wires to be saved or replaced atomically.
- Relational constraints can enforce relationships among circuits, components, and wires.

## Alternatives considered for this requirement

JSON files would not satisfy the database-integration requirement. A server database would add deployment and setup work for a local college project. These alternatives are considered here in response to the database requirement; they are not being represented as options formally evaluated during the original project scope.

## Persistence scope

Persist circuit definitions, component positions, binary input values, and wires. Do not persist simulation outputs, which are transient results produced by the C++ engine.

Allow unfinished circuits to be saved so the editor can preserve work in progress. Circuit simulation validation remains a separate operation; saving a circuit does not require it to be simulation-ready.

Each laptop uses its own local database. Git shares the application code and database schema, not saved circuit data.

## Limitations

This milestone does not provide a shared cloud database or user accounts. Saved circuits remain local to the laptop where they were created.

## Implementation history

The original persistence work added SQLite-backed named-circuit save, list, open, and update behavior. Subsequent review fixes hardened SQLite error handling, stale frontend request handling, CMake target compatibility, and saved-circuit ID parsing. Those review fixes improve the implementation; they are separate from the original persistence decision.
