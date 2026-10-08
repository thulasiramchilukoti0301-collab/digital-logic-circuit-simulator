# Digital Logic Circuit Simulator

An academic digital logic simulator with a C++17 simulation engine, a local HTTP API, a React/TypeScript visual editor, and SQLite-backed circuit persistence.

**Core:** C++17, CMake, SQLite | **Frontend:** React, TypeScript, Vite | **Project status:** In development

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Supported Components](#supported-components)
- [System Architecture](#system-architecture)
- [C++ Simulation Engine and OOP](#c-simulation-engine-and-oop)
- [Circuit Editor](#circuit-editor)
- [Persistence API and Database](#persistence-api-and-database)
- [Example Circuit](#example-circuit)
- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
- [Windows MSYS2 UCRT64 Setup](#windows-msys2-ucrt64-setup)
- [Current Development Status](#current-development-status)
- [Development Roadmap](#development-roadmap)
- [Scope](#scope)
- [Authors](#authors)

## Overview

The browser editor places and connects circuit components, presents component details, and sends validation and simulation requests to the local C++ server. C++ is the authoritative implementation of gate evaluation, circuit validation, simulation, and truth-table generation. SQLite stores saved editor circuits; it does not participate in evaluation.

## Features

**C++ simulation engine and API**

- C++17 combinational circuit model with validation, evaluation, and generated truth tables.
- HTTP endpoints for health, validation, simulation, and truth-table generation.
- Structured JSON requests and responses through the local C++ API.

**React/TypeScript editor**

- Add inputs, outputs, and gates by clicking items in the component palette.
- Drag existing components to reposition them and connect component pins by drawing wires.
- Toggle binary input values, select components, and inspect their type, ID, name, position, input count, or value. The inspector is read-only; component renaming is not currently available.
- Validate and simulate through the C++ API. Truth-table generation is available in the backend API; a truth-table display is not yet implemented in the frontend.

**SQLite persistence**

- Name and save a new circuit, list saved circuits, open a circuit, and save changes to the currently opened circuit.
- Restore component IDs, types, names, positions, gate input counts, input values, and wires.
- Save unfinished circuits with unconnected pins. Persistence checks document structure and references without requiring simulation validation.
- Confirm before replacing unsaved changes. Opening a circuit clears transient simulation results.
- Circuit persistence is implemented and Mac browser checks are complete. It remains on `feat/circuit-persistence`; it has not yet been committed or merged.

## Supported Components

| Component | Gate input count | Behavior |
|-----------|------------------|----------|
| AND | 2 or more | High only when every input is high |
| OR | 2 or more | High when at least one input is high |
| NOT | 1 | Inverts its input |
| XOR | 2 | High when its inputs differ |
| NAND | 2 or more | Inverts the AND result |
| NOR | 2 or more | Inverts the OR result |
| INPUT | 0 | User-set binary value, `0` or `1` |
| OUTPUT | 1 | Displays the connected signal after simulation |

## System Architecture

```text
Browser
┌───────────────────────────────┐
│ React / TypeScript circuit UI │
│ placement · dragging · wiring │
│ input controls · saved list   │
└──────────────┬────────────────┘
               │ HTTP / JSON
               ▼
┌───────────────────────────────┐
│ C++ HTTP API                  │
│ validate · simulate · tables  │
│ circuit save/list/open/update │
└──────────────┬────────────────┘
               │                 │
               ▼                 ▼
┌──────────────────────┐  ┌──────────────────────┐
│ C++ circuit engine   │  │ SQLite repository    │
│ validation/evaluation│  │ circuits/components/ │
│ truth-table creation │  │ wires                │
└──────────────────────┘  └──────────────────────┘
```

Circuit validation, gate evaluation, simulation, and truth-table generation stay in the C++ engine. Persistence validation is a separate API-layer check so incomplete circuits can be saved. The SQLite repository uses relational tables, foreign keys, constraints, prepared statements, and transactions for saves and updates.

## C++ Simulation Engine and OOP

The engine models components through polymorphic C++ classes. `Component` provides the shared component ID and a virtual destructor. `Gate` derives from `Component` and declares the pure virtual `compute` interface; AND, OR, NOT, XOR, NAND, and NOR gates implement it. `Input` and `Output` are also component subclasses.

`Circuit` owns components through `std::unique_ptr<Component>` and stores wires as values. It works through the component and gate abstractions and uses standard library containers for circuit data and evaluation. Gate-specific evaluation remains in the corresponding gate implementations.

| OOP concept | Implementation |
|-------------|----------------|
| Abstraction | `Component` and the pure virtual `Gate::compute` interface |
| Inheritance | Concrete gates, `Input`, and `Output` derive from `Component` directly or through `Gate` |
| Runtime polymorphism | `Circuit` stores base-class component pointers and dispatches virtual gate computation |
| Encapsulation | Component IDs, input values, and names are accessed through class interfaces |
| Composition and ownership | `Circuit` owns components with `std::unique_ptr` and contains its wires |
| STL and modular design | Standard containers and separate headers/source files organize circuit and gate behavior |

## Circuit Editor

Click a palette item to place a component in the workspace. Drag a placed component to move it. Start from an output connection point and connect to an unconnected input pin to create a wire. Input controls toggle between `0` and `1`. Select a component to view its details in the inspector.

The editor sends validation and simulation requests through the Vite `/api` proxy. It does not evaluate logic in JavaScript. The backend exposes truth-table generation, but truth-table UI remains future work.

## Persistence API and Database

- `GET /api/circuits` lists saved circuit summaries with `id`, `name`, and `updatedAt`.
- `POST /api/circuits` creates a saved circuit from `{ "name": "...", "circuit": { "version": 1, "components": [], "wires": [] } }`. Duplicate names return HTTP 409.
- `GET /api/circuits/{id}` returns the saved circuit and its component/wire document.
- `PUT /api/circuits/{id}` replaces the explicitly selected circuit atomically. A missing ID returns HTTP 404.

The database has `circuits`, `components`, and `wires` tables. Components and wires reference their parent circuit; wire endpoints reference components. Updates remove existing wires before components and replace the contents in one transaction. The database is accessed through CMake's `SQLite3::SQLite3` imported target and remains separate from the simulation engine.

The database defaults to `data/circuits.sqlite3`. Set `DIGITAL_LOGIC_DB_PATH` to select another database file; the server creates parent directories as needed. Runtime database files and SQLite journal, WAL, and SHM sidecars are ignored by Git.

## Example Circuit

Circuit: `OUTPUT = (A AND B) OR C`

With `A = 1`, `B = 1`, and `C = 0`:

```text
AND(1, 1) = 1
OR(1, 0)  = 1
OUTPUT    = 1
```

The backend can also generate truth-table rows from a circuit. For `A AND B`:

| A | B | Output |
|---|---|--------|
| 0 | 0 | 0 |
| 0 | 1 | 0 |
| 1 | 0 | 0 |
| 1 | 1 | 1 |

## Project Structure

```text
.
|-- backend/
|   |-- app/              # HTTP routes, request adapter, SQLite repository
|   |-- include/          # C++ engine headers
|   |-- src/              # C++ engine implementation
|   `-- tests/            # Engine, adapter, repository, and HTTP tests
|-- frontend/             # React, TypeScript, Vite visual editor
|-- third_party/          # Vendored cpp-httplib and nlohmann/json headers
|-- CMakeLists.txt
`-- README.md
```

## Getting Started

### Requirements

- C++17 compiler and CMake 3.15 or newer
- SQLite development headers and library discoverable by CMake
- Node.js 20.19+ and npm
- Modern web browser

### macOS

CMake uses the SQLite headers and library available to the selected toolchain. With Apple Command Line Tools installed:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/digital_logic_server
```

In a separate terminal, start the frontend:

```sh
cd frontend
npm install
npm run dev
```

Open the Vite URL shown in the terminal, normally `http://localhost:5173`. Vite proxies API requests to `http://127.0.0.1:8080`.

### Windows MSYS2 UCRT64 Setup

Install the compiler, CMake, Make, and SQLite development package in the MSYS2 UCRT64 environment:

```sh
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-sqlite3
```

From the UCRT64 shell, configure and build using that environment's toolchain and SQLite package:

```sh
cmake -S . -B build-ucrt64 -G "MinGW Makefiles"
cmake --build build-ucrt64
ctest --test-dir build-ucrt64 --output-on-failure
./build-ucrt64/digital_logic_server.exe
```

Start Vite in another terminal with `cd frontend`, `npm install`, and `npm run dev`. Windows build and browser testing remain pending; Windows setup instructions have not been validated on Windows.

## Current Development Status

- The C++17 engine supports circuit validation, evaluation, signal propagation, and truth-table generation.
- The C++ HTTP API exposes health, validation, simulation, truth-table, and SQLite circuit persistence endpoints.
- The React editor supports palette-click component placement, component dragging, visual wiring, input toggles, inspection, validation, and simulation.
- SQLite save/list/open/update is implemented, including persistence of unfinished circuits. Mac browser checks are complete.
- Truth-table UI remains pending. Windows testing remains pending.
- The persistence feature is on `feat/circuit-persistence` and is not yet committed or merged.

## Development Roadmap

### Completed

- [x] Repository and CMake project setup
- [x] Abstract gate model and AND, OR, NOT, XOR, NAND, and NOR gates
- [x] Input, Output, Wire, and Circuit classes
- [x] Circuit validation, evaluation, signal propagation, and truth-table generation
- [x] HTTP/JSON API for health, validation, simulation, and truth tables
- [x] React/TypeScript visual circuit editor with component placement, dragging, wiring, input controls, inspection, validation, and simulation integration
- [x] SQLite circuit save/list/open/update with transactional persistence and unfinished-circuit support

### Remaining

- [ ] Truth-table visualization in the frontend
- [ ] Windows build and browser verification
- [ ] Commit and merge the circuit-persistence feature
- [ ] Further UI refinement and final project review

## Scope

This project focuses on digital logic circuit editing, validation, simulation, generated truth tables, and local SQLite persistence. The C++ engine remains the authoritative simulation layer; gate evaluation is not reimplemented in JavaScript. Authentication, cloud infrastructure, payments, chat, and AI/LLM features are outside the project scope.

## Authors

| Name | Roll No. | Role |
|------|----------|------|
| **Thulasi Ram Chilukoti** | 251116 | Developer |
| **Durga Sai Nayak** | 251134 | Developer |

- **Course:** Object-Oriented Programming with C++ (Mini Project)
- **Branch:** Computer Science and Engineering (CSE), 2nd Year
- **Institution:** Indian Institute of Information Technology (IIIT) Tiruchirappalli
- **Guided by:** Dr. Anoop Jacob

---

*This project was developed for academic purposes.*
