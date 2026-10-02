#pragma once

#include "Component.h"
#include "Signal.h"

#include <string>
#include <vector>

namespace digital_logic {

struct TruthTableInput {
    ComponentId id;
    std::string name;
};

struct TruthTableOutput {
    ComponentId id;
    std::string name;
};

struct TruthTableRow {
    std::vector<Signal> inputs;
    std::vector<Signal> outputs;
};

struct TruthTable {
    std::vector<TruthTableInput> input_columns;
    std::vector<TruthTableOutput> output_columns;
    std::vector<TruthTableRow> rows;
};

struct TruthTableResult {
    bool success = false;
    std::vector<std::string> errors;
    TruthTable table;
};

} // namespace digital_logic
