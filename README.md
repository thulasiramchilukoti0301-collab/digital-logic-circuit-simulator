# Digital Logic Circuit Simulator

An interactive, web-based digital logic circuit design and simulation tool. Users build circuits visually on a canvas, wire logic gates together, set binary inputs, and view the outputs, while all gate evaluation is performed by an object-oriented **C++17 simulation engine**.

**Core:** C++17, CMake, STL | **Frontend:** HTML5, CSS3, JavaScript | **Status:** In development

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Supported Components](#supported-components)
- [System Architecture](#system-architecture)
- [C++ Simulation Engine](#c-simulation-engine)
- [Web Visual Editor](#web-visual-editor)
- [OOP Concepts Demonstrated](#oop-concepts-demonstrated)
- [Example Circuit](#example-circuit)
- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
- [Development Roadmap](#development-roadmap)
- [Scope](#scope)
- [Authors](#authors)

---

## Overview

The Digital Logic Circuit Simulator lets a user open the application in a browser, place gates on a canvas, connect them with wires, toggle binary inputs, run the simulation, and observe the resulting outputs, all without touching a terminal.

The project has two layers:

1. **C++ digital logic simulation engine:** the academic and technical core. It models gates, wires, inputs, outputs and circuits using object-oriented design, and is the single authoritative source of simulation results.
2. **Web-based visual circuit editor:** a presentation and interaction layer for building circuits and displaying results. It does not contain gate logic; it sends the circuit to the C++ engine and shows what comes back.

## Features

**Simulation engine (C++17)**

- Abstract `Gate` base class with a common virtual evaluation interface
- Concrete gates: AND, OR, NOT, XOR, NAND, NOR
- Binary inputs, outputs and wires/connections
- Signal propagation through connected components
- Circuit evaluation and validation
- Truth-table generation based on the actual circuit (never hardcoded)
- Easy to add new gates without modifying core `Circuit` logic

**Visual editor (web)**

- Gate library / toolbar: AND, OR, NOT, XOR, NAND, NOR, INPUT, OUTPUT
- Canvas to add, move, select and delete components
- Create and remove wire connections visually
- Toggle input values between `0` and `1`
- One-click **Simulate** with outputs shown in the UI
- Truth-table view for the constructed circuit
- Save/load circuits (if time permits)

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
        Web Browser (visual circuit editor)
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

The browser handles drawing and interaction only. When the user clicks **Simulate**, the circuit definition and input values are sent through the communication layer to the C++ engine, which evaluates the circuit and returns the output values for the UI to display.

> The exact communication mechanism between the frontend and the C++ engine will be finalized during development.

## C++ Simulation Engine

The engine is built around an abstract `Gate` class. Every concrete gate derives directly from it, and the `Circuit` works with gates only through the base-class abstraction, never through concrete types.

```text
                        Gate  (abstract)
                  virtual evaluate() = 0
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

## Web Visual Editor

Typical workflow:

```text
 Open application
        |
 Select a gate
        |
 Place gate on canvas
        |
 Add inputs and outputs
        |
 Connect components with wires
        |
 Set binary input values
        |
 Run simulation
        |
 View output
        |
 Generate truth table
```

The visual connections on the canvas correspond directly to the circuit representation used by the C++ engine.

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
├── CMakeLists.txt
├── include/            # C++ headers (Gate, Circuit, Wire, gates, ...)
├── src/                # C++ implementation
├── tests/              # Unit tests for the simulation engine
├── web/                # HTML, CSS, JavaScript frontend
├── docs/               # Additional documentation
├── .gitignore
└── README.md
```

> This is the planned layout and may change as development progresses.

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

### Run

Instructions for starting the application and opening the web interface will be added once the frontend and communication layer are implemented.

## Development Roadmap

Development is incremental. Each phase is completed and tested before the next begins.

**C++ simulation engine**

- [x] Phase 0: Repository setup and README
- [ ] Phase 1: C++ project and CMake setup
- [ ] Phase 2: Abstract `Gate` class
- [ ] Phase 3: AND, OR, NOT gates
- [ ] Phase 4: XOR, NAND, NOR gates
- [ ] Phase 5: Input, Output and Wire
- [ ] Phase 6: `Circuit` class
- [ ] Phase 7: Circuit evaluation and signal propagation
- [ ] Phase 8: Truth-table generation
- [ ] Phase 9: Independent testing of the engine

**Web application**

- [ ] Phase 10: Web application foundation
- [ ] Phase 11: Visual circuit canvas
- [ ] Phase 12: Gate placement and movement
- [ ] Phase 13: Visual wire connections
- [ ] Phase 14: Connect frontend to the C++ engine
- [ ] Phase 15: Interactive binary inputs and simulation
- [ ] Phase 16: Truth-table visualization

**Finishing**

- [ ] Phase 17: Circuit validation and error handling
- [ ] Phase 18: Save/load circuits (if time permits)
- [ ] Phase 19: Final testing, documentation and UI improvements

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
