#include "core/Optimizer.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

namespace minipas {
namespace {

bool startsWith(const std::string &text, const std::string &prefix) {
    return text.rfind(prefix, 0) == 0;
}

std::string trimNumber(double value) {
    if (std::fabs(value - std::round(value)) < 1e-9) {
        return std::to_string(static_cast<long long>(std::llround(value)));
    }
    std::ostringstream out;
    out << value;
    return out.str();
}

bool toDouble(const std::string &text, double *value) {
    try {
        size_t consumed = 0;
        *value = std::stod(text, &consumed);
        return consumed == text.size();
    } catch (...) {
        return false;
    }
}

} // namespace

Optimizer::Optimizer(std::vector<Constant> constants)
    : constants_(std::move(constants)) {}

OptimizeResult Optimizer::optimize(const std::vector<Quad> &quads) const {
    OptimizeResult result;
    std::map<std::string, std::string> knownTemps;
    std::map<std::string, std::string> expressions;

    for (Quad quad : quads) {
        if (isBinaryOp(quad.op)) {
            std::string left = quad.arg1;
            std::string right = quad.arg2;
            std::string leftValue;
            std::string rightValue;
            if (constantValue(left, knownTemps, &leftValue)) {
                left = leftValue;
            }
            if (constantValue(right, knownTemps, &rightValue)) {
                right = rightValue;
            }

            std::string folded;
            if (fold(quad.op, left, right, &folded)) {
                quad.op = ":=";
                quad.arg1 = folded;
                quad.arg2 = "_";
                knownTemps[quad.result] = folded;
            } else {
                if (isCommutative(quad.op) && right < left) {
                    std::swap(left, right);
                }
                const std::string key = quad.op + "|" + left + "|" + right;
                const auto found = expressions.find(key);
                if (found != expressions.end() && startsWith(quad.result, "t")) {
                    quad.op = ":=";
                    quad.arg1 = found->second;
                    quad.arg2 = "_";
                    knownTemps.erase(quad.result);
                } else {
                    expressions[key] = quad.result;
                    quad.arg1 = left;
                    quad.arg2 = right;
                    knownTemps.erase(quad.result);
                }
            }
        } else if (quad.op == ":=") {
            std::string value;
            if (constantValue(quad.arg1, knownTemps, &value)) {
                quad.arg1 = value;
                if (startsWith(quad.result, "t")) {
                    knownTemps[quad.result] = value;
                }
            } else {
                knownTemps.erase(quad.result);
            }
        } else if (quad.op == "=[]" || quad.op == "read" || quad.op == "readarr") {
            knownTemps.erase(quad.result);
        }
        result.quads.push_back(quad);
    }

    assignBlocks(&result.quads);
    result.diagnostics.push_back({Severity::Info, "optimizer",
                                  "applied constant folding, temporary propagation, common subexpression reuse, and basic-block marking",
                                  1, 1});
    return result;
}

bool Optimizer::isBinaryOp(const std::string &op) const {
    static const std::set<std::string> ops = {
        "+", "-", "*", "/", "=", "<>", "<", "<=", ">", ">=", "and", "or"
    };
    return ops.count(op) > 0;
}

bool Optimizer::isCommutative(const std::string &op) const {
    return op == "+" || op == "*" || op == "=" || op == "<>" || op == "and" || op == "or";
}

bool Optimizer::isConstantOperand(const std::string &operand) const {
    if (operand.empty() || operand == "_") {
        return false;
    }
    if (startsWith(operand, "C")) {
        return true;
    }
    if (operand == "true" || operand == "false") {
        return true;
    }
    double value = 0.0;
    return toDouble(operand, &value);
}

bool Optimizer::constantValue(const std::string &operand,
                              const std::map<std::string, std::string> &known,
                              std::string *value) const {
    const auto knownFound = known.find(operand);
    if (knownFound != known.end()) {
        *value = knownFound->second;
        return true;
    }
    if (startsWith(operand, "C")) {
        try {
            const int index = std::stoi(operand.substr(1));
            for (const auto &constant : constants_) {
                if (constant.index == index) {
                    *value = constant.value;
                    return true;
                }
            }
        } catch (...) {
            return false;
        }
    }
    if (isConstantOperand(operand)) {
        *value = operand;
        return true;
    }
    return false;
}

bool Optimizer::fold(const std::string &op, const std::string &left,
                     const std::string &right, std::string *value) const {
    if (op == "and" || op == "or") {
        if ((left != "true" && left != "false") || (right != "true" && right != "false")) {
            return false;
        }
        const bool l = left == "true";
        const bool r = right == "true";
        *value = (op == "and" ? (l && r) : (l || r)) ? "true" : "false";
        return true;
    }

    double l = 0.0;
    double r = 0.0;
    if (!toDouble(left, &l) || !toDouble(right, &r)) {
        return false;
    }

    if (op == "+") {
        *value = trimNumber(l + r);
    } else if (op == "-") {
        *value = trimNumber(l - r);
    } else if (op == "*") {
        *value = trimNumber(l * r);
    } else if (op == "/") {
        if (std::fabs(r) < 1e-12) {
            return false;
        }
        *value = trimNumber(l / r);
    } else if (op == "=") {
        *value = std::fabs(l - r) < 1e-9 ? "true" : "false";
    } else if (op == "<>") {
        *value = std::fabs(l - r) >= 1e-9 ? "true" : "false";
    } else if (op == "<") {
        *value = l < r ? "true" : "false";
    } else if (op == "<=") {
        *value = l <= r ? "true" : "false";
    } else if (op == ">") {
        *value = l > r ? "true" : "false";
    } else if (op == ">=") {
        *value = l >= r ? "true" : "false";
    } else {
        return false;
    }
    return true;
}

void Optimizer::assignBlocks(std::vector<Quad> *quads) const {
    if (!quads || quads->empty()) {
        return;
    }
    std::set<int> leaders = {1};
    for (const auto &quad : *quads) {
        if ((quad.op == "j" || quad.op == "jfalse") && !quad.result.empty()) {
            try {
                const int target = std::stoi(quad.result);
                if (target >= 1 && target <= static_cast<int>(quads->size())) {
                    leaders.insert(target);
                }
                if (quad.index + 1 <= static_cast<int>(quads->size())) {
                    leaders.insert(quad.index + 1);
                }
            } catch (...) {
            }
        }
    }

    int block = 0;
    for (auto &quad : *quads) {
        if (leaders.count(quad.index) > 0) {
            ++block;
        }
        quad.block = block;
    }
}

} // namespace minipas
