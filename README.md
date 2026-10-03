# Digital Logic Circuit Simulator

The project combines a complete C++17 combinational simulation engine and local HTTP API with a React, TypeScript, Vite frontend shell. The C++ engine is authoritative for circuit validation, simulation, and truth-table results. The visual editor is future work.

**Core:** C++17, CMake, STL | **Frontend:** React, TypeScript, Vite | **Status:** In development

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Supported Components](#supported-components)
- [System Architecture](#system-architecture)
- [C++ Simulation Engine](#c-simulation-engine)
- [Frontend Shell](#frontend-shell)
- [OOP Concepts Demonstrated](#oop-concepts-demonstrated)
- [Example Circuit](#example-circuit)
- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
- [Frontend Development](#frontend-development)
- [Current Development Status](#current-development-status)
- [Development Roadmap](#development-roadmap)
- [Scope](#scope)
- [Authors](#authors)

---

## Overview

The Digital Logic Circuit Simulator includes a browser shell that checks its connection to the local C++ server. The interactive circuit editor has not been implemented yet.

The project has two layers:

1. **C++ digital logic simulation engine:** the academic and technical core. It models gates, wires, inputs, outputs and circuits using object-oriented design, and is the single authoritative source of simulation results.
2. **React, TypeScript, Vite frontend shell:** a presentation layer with a backend connection check and an API client prepared for future editor interactions. It contains no gate logic.

## Features

**Simulation engine (C++17)**

- Abstract `Gate` base class with a common virtual evaluation interface
- Concrete gates: AND, OR, NOT, XOR, NAND, NOR
- Binary inputs, outputs and wires/connections
- Signal propagation through connected components
- Circuit evaluation and validation
- Truth-table generation based on the actual circuit (never hardcoded)
- Easy to add new gates without modifying core `Circuit` logic

**Frontend shell (React, TypeScript, Vite)**

- Responsive application shell with a circuit-editor placeholder
- Startup health check with connected/unavailable status and retry action
- Typed API client for health, validation, simulation, and truth-table endpoints
- Vite development proxy to the local C++ server

## Supported Components

| Component | Inputs | Output |
|-----------|--------|--------|
| AND   | 2+ | `1` only when all inputs are `1` |
| OR    | 2+ | `1` when at least one input is `1` |
| NOT   | 1  | Complement of the input |
| XOR   | 2  | `1` when the inputs differ |
| NAND  | 2+ | Complement of AND |
| NOR   | 2+ | Complement of OR |
| INPUT | 0  | User-set value, `0` or `1` |
| OUTPUT| 1  | Displays the value of the signal connected to it |

## System Architecture

```text
        Web Browser (React application shell)
                     |
                     |  API / communication layer
                     v
        C++ Application / Simulation Engine
                     |
                     v
               Circuit Model
                     |
        +------------+-------------+
        |            |             |
      Gates        Wires     Inputs / Outputs
```

The browser communicates with the C++ application through its local HTTP API. The current shell checks the backend health endpoint; its API client also defines calls for validation, simulation, and truth tables. The future editor will own presentation and interaction, while validation, simulation, and truth-table generation remain in the C++ engine.

## C++ Simulation Engine

The engine is built around an abstract `Gate` class. Every concrete gate derives directly from it, and the `Circuit` works with gates only through the base-class abstraction, never through concrete types.

```text
                        Gate  (abstract)
                  virtual compute(inputs) = 0
                           |
     +----------+----------+----------+----------+----------+
     |          |          |          |          |          |
  ANDGate    ORGate     NOTGate    XORGate    NANDGate   NORGate
```

Signals flow through connections between component outputs and inputs:

```text
 A ──┐
     ├──► [ AND ] ──┐
 B ──┘              ├──► [ OR ] ──► OUTPUT
 C ─────────────────┘
```

When a circuit is evaluated, each gate reads the values on its inputs, computes its result through its overridden virtual function, and passes the result on to the components it feeds.

## Frontend Shell

The current frontend provides the application frame, editor placeholder, and backend status. Circuit placement, wiring, simulation controls, and truth-table display are not part of this milestone.

The API client sends relative `/api/...` requests through Vite's development proxy. The browser does not evaluate gates; the C++ engine remains authoritative.

## OOP Concepts Demonstrated

| Concept | Where it is used |
|---------|------------------|
| Abstraction | `Gate` defines the common evaluation interface |
| Inheritance | Each specific gate derives from `Gate` |
| Runtime polymorphism / dynamic binding | Circuit evaluates gates through base-class references and pointers |
| Virtual functions | Gate evaluation is overridden in each derived gate |
| Virtual destructors | Safe cleanup of derived objects through base pointers |
| Encapsulation | Component state kept private behind clean interfaces |
| Composition | `Circuit` is composed of gates, wires, inputs and outputs |
| STL containers | Storage and lookup of components and connections |
| Smart pointers | Ownership of gates and other components |
| Modular design | Separate headers and sources per component |

## Example Circuit

Circuit: `OUTPUT = (A AND B) OR C`

With `A = 1`, `B = 1`, `C = 0`:

```text
AND(1, 1) = 1
OR(1, 0)  = 1
OUTPUT    = 1
```

Truth table for `A AND B` (generated from the circuit itself):

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
|   |-- app/              # HTTP routes and JSON adapter
|   |-- include/          # C++ engine headers
|   |-- src/              # C++ engine implementation
|   `-- tests/            # Engine, adapter, and HTTP tests
|-- frontend/             # React, TypeScript, Vite frontend shell
|-- third_party/          # Vendored C++ HTTP and JSON headers
|-- CMakeLists.txt
`-- README.md
```

The structure shown reflects the current repository.

## Getting Started

### Prerequisites

- A C++17-compatible compiler (GCC 9+, Clang 10+, or MSVC 2019+)
- CMake 3.15 or newer
- A modern web browser

### Build the C++ engine

```bash
git clone https://github.com/<your-username>/<repo-name>.git
cd <repo-name>

cmake -S . -B build
cmake --build build
```

### Frontend Development

Prerequisites: Node.js 20.19+ and npm.

Install frontend packages and start Vite:

```powershell
cd frontend
npm install
npm run dev
```

In a separate terminal, build and start the C++ API server from the repository root:

```powershell
& 'C:\Program Files\CMake\bin\cmake.exe' -S . -B build-ucrt64 `
  -G 'MinGW Makefiles' `
  -DCMAKE_MAKE_PROGRAM='C:\msys64\ucrt64\bin\mingw32-make.exe' `
  -DCMAKE_CXX_COMPILER='C:\msys64\ucrt64\bin\g++.exe'
& 'C:\Program Files\CMake\bin\cmake.exe' --build build-ucrt64 --target digital_logic_server
.\build-ucrt64\digital_logic_server.exe
```

Open the Vite URL shown in the terminal (normally `http://localhost:5173`). Vite proxies `/api` requests to `http://127.0.0.1:8080`, so the frontend uses relative API paths. The shell checks `/api/health` on startup and shows whether the C++ backend is available.

## Current Development Status

- The C++17 simulation engine supports combinational circuit validation, evaluation, and truth-table generation.
- The C++ HTTP API exposes health, validation, simulation, and truth-table endpoints.
- The React, TypeScript, Vite frontend shell checks backend connectivity and provides an editor placeholder.
- Next work is the visual circuit editor.

## Development Roadmap

### Completed

- [x] Phase 0: Repository setup and README
- [x] Phase 1: C++ project and CMake setup
- [x] Phase 2: Abstract `Gate` class and AND, OR, NOT gates
- [x] Phase 3: XOR, NAND, and NOR gates
- [x] Phase 4: Input, Output, and Wire components
- [x] Phase 5: Circuit class and structural validation
- [x] Phase 6: Circuit evaluation and signal propagation
- [x] Phase 7: Truth-table generation
- [x] Phase 8: Independent engine testing and hardening
- [x] Phase 9: HTTP/JSON adapter and C++ HTTP API (`/api/health`, `/api/validate`, `/api/simulate`, `/api/truth-table`)
- [x] Phase 10: React, TypeScript, Vite frontend shell

### Future work

- [ ] Phase 11: Visual circuit canvas
- [ ] Phase 12: Gate placement and movement
- [ ] Phase 13: Visual wire connections
- [ ] Phase 14: Interactive binary inputs and simulation UI
- [ ] Phase 15: Truth-table visualization
- [ ] Phase 16: Circuit save/load
- [ ] Phase 17: Final testing, documentation, and UI improvements

## Scope

The project stays focused on digital logic simulation. The C++ engine remains the authoritative simulation layer, and gate evaluation is never reimplemented in JavaScript. Features such as user authentication, databases, payments, chat, AI/LLM features and cloud infrastructure are intentionally out of scope.

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
