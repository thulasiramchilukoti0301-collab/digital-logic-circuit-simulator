#include "ANDGate.h"
#include "NOTGate.h"
#include "ORGate.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using digital_logic::Signal;

int failures = 0;

void expect_signal(const std::string& label,
                   const digital_logic::Gate& gate,
                   const std::vector<Signal>& inputs,
                   Signal expected) {
    try {
        if (gate.compute(inputs) != expected) {
            std::cerr << "FAIL: " << label << " returned the wrong signal\n";
            ++failures;
        }
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << label << " threw: " << error.what() << '\n';
        ++failures;
    }
}

template <typename Function>
void expect_invalid_argument(const std::string& label, Function&& function) {
    try {
        function();
        std::cerr << "FAIL: " << label << " did not reject invalid input\n";
        ++failures;
    } catch (const std::invalid_argument&) {
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << label << " threw unexpected exception: "
                  << error.what() << '\n';
        ++failures;
    }
}

} // namespace

int main() {
    const digital_logic::ANDGate and_gate(1);
    expect_signal("0 AND 0", and_gate, {Signal::Low, Signal::Low}, Signal::Low);
    expect_signal("0 AND 1", and_gate, {Signal::Low, Signal::High}, Signal::Low);
    expect_signal("1 AND 0", and_gate, {Signal::High, Signal::Low}, Signal::Low);
    expect_signal("1 AND 1", and_gate, {Signal::High, Signal::High}, Signal::High);

    const digital_logic::ORGate or_gate(2);
    expect_signal("0 OR 0", or_gate, {Signal::Low, Signal::Low}, Signal::Low);
    expect_signal("0 OR 1", or_gate, {Signal::Low, Signal::High}, Signal::High);
    expect_signal("1 OR 0", or_gate, {Signal::High, Signal::Low}, Signal::High);
    expect_signal("1 OR 1", or_gate, {Signal::High, Signal::High}, Signal::High);

    const digital_logic::NOTGate not_gate(3);
    expect_signal("NOT 0", not_gate, {Signal::Low}, Signal::High);
    expect_signal("NOT 1", not_gate, {Signal::High}, Signal::Low);

    expect_invalid_argument("AND wrong input count", [&] {
        and_gate.compute({Signal::High});
    });
    expect_invalid_argument("OR wrong input count", [&] {
        or_gate.compute({Signal::Low, Signal::High, Signal::Low});
    });
    expect_invalid_argument("NOT missing input", [&] {
        not_gate.compute({});
    });
    expect_invalid_argument("NOT extra input", [&] {
        not_gate.compute({Signal::Low, Signal::High});
    });
    expect_invalid_argument("AND undefined input", [&] {
        and_gate.compute({Signal::Low, Signal::Undefined});
    });
    expect_invalid_argument("OR undefined input", [&] {
        or_gate.compute({Signal::Low, Signal::Undefined});
    });
    expect_invalid_argument("NOT undefined input", [&] {
        not_gate.compute({Signal::Undefined});
    });
    expect_invalid_argument("AND constructed with fewer than two inputs", [] {
        digital_logic::ANDGate invalid_gate(4, 1);
    });
    expect_invalid_argument("OR constructed with fewer than two inputs", [] {
        digital_logic::ORGate invalid_gate(5, 1);
    });

    if (failures != 0) {
        std::cerr << failures << " gate test(s) failed\n";
        return 1;
    }

    std::cout << "All gate tests passed\n";
    return 0;
}
