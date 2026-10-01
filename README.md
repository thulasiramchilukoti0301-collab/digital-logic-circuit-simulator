# Digital Logic Circuit Simulator

A C++ command-line application that models digital logic gates and simulates how binary signals propagate through connected circuits, built around object-oriented design with a virtual base class and runtime polymorphism.

**Language:** C++17 | **Interface:** Command line | **Status:** In development

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Supported Gates](#supported-gates)
- [System Architecture](#system-architecture)
- [OOP Concepts Demonstrated](#oop-concepts-demonstrated)
- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
- [Usage](#usage)
- [Roadmap](#roadmap)
- [Author](#author)

---

## Overview

The Digital Logic Circuit Simulator lets users build logic circuits from basic gates, connect them with wires, supply binary inputs, and evaluate the resulting outputs.

The project is a practical application of core C++ object-oriented concepts: an abstract `Gate` base class defines a common interface, each specific gate overrides its own evaluation logic, and the circuit evaluates any gate through a base-class pointer using **dynamic binding**.

## Features

- Abstract `Gate` base class with a pure virtual `evaluate()` interface
- Derived gate classes with their own logic
- Wires that carry and propagate binary state between components
- Evaluation of circuit output from supplied inputs
- Truth-table generation
- Validation of circuit configuration and inputs
- Modular, extensible design: adding a new gate requires only a new derived class

## Supported Gates

| Gate | Inputs | Output                             |
|------|--------|------------------------------------|
| AND  | 2+     | `1` only when all inputs are `1`   |
| OR   | 2+     | `1` when at least one input is `1` |
| NOT  | 1      | Complement of the input            |
| XOR  | 2      | `1` when the inputs differ         |
| NAND | 2+     | Complement of AND                  |
| NOR  | 2+     | Complement of OR                   |

AND, OR and NOT form the core required set. XOR, NAND and NOR are extensions, and more gates can be added easily.

**Example: AND gate truth table**

| A | B | Output |
|---|---|--------|
| 0 | 0 | 0      |
| 0 | 1 | 0      |
| 1 | 0 | 0      |
| 1 | 1 | 1      |

## System Architecture

Every gate derives directly from the abstract `Gate` class. Each one is a peer implementation of the same interface, so the circuit never needs to know which concrete gate it is evaluating.

```text
                        Gate  (abstract)
                  virtual bool evaluate() = 0
                           |
     +----------+----------+----------+----------+----------+
     |          |          |          |          |          |
  ANDGate    ORGate     NOTGate    XORGate    NANDGate   NORGate
```

Signals flow through `Wire` objects connecting gate outputs to gate inputs:

```text
 Input A ──┐
           ├──► [ AND ] ──► Wire ──► [ NOT ] ──► Output
 Input B ──┘
```

When the circuit is evaluated, each gate reads the state of its input wires, computes its result via its overridden `evaluate()`, and drives its output wire, which in turn feeds the next gate.

## OOP Concepts Demonstrated

| Concept                         | Where it is used                                                   |
|---------------------------------|--------------------------------------------------------------------|
| Abstraction                     | `Gate` defines a pure virtual interface                            |
| Inheritance                     | Each specific gate derives from `Gate`                             |
| Polymorphism / dynamic binding  | Gates are evaluated through `Gate*` pointers, resolved at runtime  |
| Virtual destructor              | Safe cleanup of derived objects via base pointers                  |
| Encapsulation                   | Gate and wire state kept private behind a clean interface          |
| Dynamic memory / smart pointers | Gates stored and managed in the circuit                            |

## Project Structure

```text
.
├── include/
│   ├── Gate.h
│   ├── Wire.h
│   ├── Circuit.h
│   └── gates/
│       ├── ANDGate.h
│       ├── ORGate.h
│       └── NOTGate.h
├── src/
│   ├── main.cpp
│   ├── Wire.cpp
│   ├── Circuit.cpp
│   └── gates/
├── tests/
├── Makefile
├── .gitignore
└── README.md
```

> The layout above is the planned structure and may change as development progresses.

## Getting Started

### Prerequisites

- A C++17-compatible compiler (GCC 9+, Clang 10+, or MSVC 2019+)
- `make` (optional)

### Build

```bash
git clone https://github.com/<your-username>/<repo-name>.git
cd <repo-name>

# Using make
make

# Or compile directly
g++ -std=c++17 -Wall -Wextra -Iinclude src/*.cpp src/gates/*.cpp -o simulator
```

### Run

```bash
./simulator
```

## Usage

A sample session is shown below (planned interface; subject to change):

```text
$ ./simulator

Circuit: (A AND B) -> NOT
Enter A: 1
Enter B: 1
Output: 0

Truth table
 A | B | Out
---+---+----
 0 | 0 |  1
 0 | 1 |  1
 1 | 0 |  1
 1 | 1 |  0
```

## Roadmap

- [x] Project setup and documentation
- [ ] `Gate` abstract base class and `Wire`
- [ ] AND, OR, NOT gates
- [ ] Circuit class with signal propagation
- [ ] Truth-table generation
- [ ] XOR, NAND, NOR gates
- [ ] Input and circuit validation
- [ ] Unit tests
- [ ] Circuit definition from a text file


## Authors

|--------------------------------------------------|
|            Name	       |  Roll No.	|    Role    |
|------------------------|------------|------------|
|  Thulasi Ram Chilukoti |   251116	  | Developer  |
|  Durga Sai Nayak	     |  251134	  |  Developer |
|--------------------------------------------------|

Course: Object-Oriented Programming with C++ (Mini Project)
Branch: Computer Science and Engineering (CSE), 2nd Year
Institution: Indian Institute of Information Technology (IIIT) Tiruchirappalli
Guided by: Dr. Anoop Jacob

---

*This project was developed for academic purposes.*
