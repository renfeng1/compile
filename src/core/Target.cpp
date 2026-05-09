#include "core/Target.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
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
    out << std::setprecision(10) << value;
    return out.str();
}

bool parseNumber(const std::string &text, double *value) {
    try {
        size_t consumed = 0;
        *value = std::stod(text, &consumed);
        return consumed == text.size();
    } catch (...) {
        return false;
    }
}

struct Runtime {
    std::map<std::string, std::string> memory;
    std::map<std::string, std::string> types;
    std::map<int, Constant> constants;
    std::map<int, Symbol> symbolsByIndex;
    std::map<std::string, Symbol> symbolsByPlace;
};

std::string baseType(const Symbol &symbol) {
    if (symbol.arrayLength == 0) {
        return symbol.type;
    }
    const std::string marker = " of ";
    const size_t pos = symbol.type.find(marker);
    return pos == std::string::npos ? symbol.type : symbol.type.substr(pos + marker.size());
}

std::string defaultValue(const std::string &type) {
    if (type == "bool") {
        return "false";
    }
    if (type == "char" || type == "string") {
        return "";
    }
    return "0";
}

std::string symbolPlace(const Symbol &symbol) {
    if (symbol.category == "temp") {
        return symbol.name;
    }
    return "I" + std::to_string(symbol.index);
}

std::string placeName(const std::string &place, const Runtime &rt) {
    const auto found = rt.symbolsByPlace.find(place);
    if (found != rt.symbolsByPlace.end()) {
        return found->second.name;
    }
    return place;
}

std::string valueOf(const std::string &operand, const Runtime &rt) {
    if (operand.empty() || operand == "_") {
        return "0";
    }
    if (startsWith(operand, "C")) {
        try {
            const int index = std::stoi(operand.substr(1));
            const auto found = rt.constants.find(index);
            if (found != rt.constants.end()) {
                return found->second.value;
            }
        } catch (...) {
        }
    }
    if (rt.memory.count(operand) > 0) {
        return rt.memory.at(operand);
    }
    return operand;
}

double numericValue(const std::string &operand, const Runtime &rt) {
    double value = 0.0;
    parseNumber(valueOf(operand, rt), &value);
    return value;
}

bool boolValue(const std::string &operand, const Runtime &rt) {
    const std::string value = valueOf(operand, rt);
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }
    double number = 0.0;
    return parseNumber(value, &number) && std::fabs(number) > 1e-9;
}

void writeValue(const std::string &place, const std::string &value, Runtime *rt) {
    rt->memory[place] = value;
}

std::string arrayCell(const std::string &arrayPlace, const std::string &indexOperand, const Runtime &rt) {
    const int index = static_cast<int>(numericValue(indexOperand, rt));
    return arrayPlace + "[" + std::to_string(index) + "]";
}

void executeInstruction(const TargetInstruction &ins, int *pc, Runtime *rt,
                        std::vector<std::string> *trace,
                        std::vector<std::string> *output) {
    std::ostringstream event;
    event << "#" << ins.index << " " << ins.op;
    if (!ins.a.empty()) event << " " << ins.a;
    if (!ins.b.empty()) event << " " << ins.b;
    if (!ins.c.empty()) event << " " << ins.c;

    if (ins.op == "DATA" || ins.op == "PROC" || ins.op == "NOP") {
        ++(*pc);
    } else if (ins.op == "HALT") {
        *pc = -1;
    } else if (ins.op == "MOV") {
        writeValue(ins.a, valueOf(ins.b, *rt), rt);
        event << " => " << placeName(ins.a, *rt) << "=" << valueOf(ins.a, *rt);
        ++(*pc);
    } else if (ins.op == "NEG") {
        writeValue(ins.a, trimNumber(-numericValue(ins.b, *rt)), rt);
        ++(*pc);
    } else if (ins.op == "NOT") {
        writeValue(ins.a, boolValue(ins.b, *rt) ? "false" : "true", rt);
        ++(*pc);
    } else if (ins.op == "READ") {
        writeValue(ins.a, defaultValue(rt->types[ins.a]), rt);
        ++(*pc);
    } else if (ins.op == "LOADARR") {
        writeValue(ins.a, valueOf(arrayCell(ins.b, ins.c, *rt), *rt), rt);
        ++(*pc);
    } else if (ins.op == "STOREARR") {
        writeValue(arrayCell(ins.a, ins.b, *rt), valueOf(ins.c, *rt), rt);
        ++(*pc);
    } else if (ins.op == "WRITE") {
        output->push_back(valueOf(ins.a, *rt));
        event << " => output " << output->back();
        ++(*pc);
    } else if (ins.op == "JMP") {
        *pc = std::stoi(ins.a) - 1;
    } else if (ins.op == "JZ") {
        *pc = boolValue(ins.a, *rt) ? *pc + 1 : std::stoi(ins.b) - 1;
    } else {
        const double left = numericValue(ins.b, *rt);
        const double right = numericValue(ins.c, *rt);
        std::string value;
        if (ins.op == "ADD") value = trimNumber(left + right);
        else if (ins.op == "SUB") value = trimNumber(left - right);
        else if (ins.op == "MUL") value = trimNumber(left * right);
        else if (ins.op == "DIV") value = std::fabs(right) < 1e-12 ? "0" : trimNumber(left / right);
        else if (ins.op == "EQ") value = std::fabs(left - right) < 1e-9 ? "true" : "false";
        else if (ins.op == "NE") value = std::fabs(left - right) >= 1e-9 ? "true" : "false";
        else if (ins.op == "LT") value = left < right ? "true" : "false";
        else if (ins.op == "LE") value = left <= right ? "true" : "false";
        else if (ins.op == "GT") value = left > right ? "true" : "false";
        else if (ins.op == "GE") value = left >= right ? "true" : "false";
        else if (ins.op == "AND") value = (boolValue(ins.b, *rt) && boolValue(ins.c, *rt)) ? "true" : "false";
        else if (ins.op == "OR") value = (boolValue(ins.b, *rt) || boolValue(ins.c, *rt)) ? "true" : "false";
        else value = "0";
        writeValue(ins.a, value, rt);
        event << " => " << placeName(ins.a, *rt) << "=" << value;
        ++(*pc);
    }
    trace->push_back(event.str());
}

std::string targetOp(const std::string &quadOp) {
    if (quadOp == "+") return "ADD";
    if (quadOp == "-") return "SUB";
    if (quadOp == "*") return "MUL";
    if (quadOp == "/") return "DIV";
    if (quadOp == "=") return "EQ";
    if (quadOp == "<>") return "NE";
    if (quadOp == "<") return "LT";
    if (quadOp == "<=") return "LE";
    if (quadOp == ">") return "GT";
    if (quadOp == ">=") return "GE";
    if (quadOp == "and") return "AND";
    if (quadOp == "or") return "OR";
    return "NOP";
}

} // namespace

TargetInstruction TargetGenerator::make(int index, const std::string &op,
                                        const std::string &a,
                                        const std::string &b,
                                        const std::string &c) const {
    return {index, op, a, b, c};
}

TargetProgram TargetGenerator::generate(const std::vector<Quad> &quads,
                                        const std::vector<Symbol> &symbols,
                                        const std::vector<Constant> &constants,
                                        bool runVm) const {
    TargetProgram program;
    Runtime rt;
    for (const auto &constant : constants) {
        rt.constants[constant.index] = constant;
    }
    for (const auto &symbol : symbols) {
        rt.symbolsByIndex[symbol.index] = symbol;
        const std::string place = symbolPlace(symbol);
        rt.symbolsByPlace[place] = symbol;
        rt.types[place] = baseType(symbol);
        if (symbol.category == "program") {
            continue;
        }
        if (symbol.arrayLength > 0) {
            for (int i = 0; i < symbol.arrayLength; ++i) {
                const std::string cell = place + "[" + std::to_string(i) + "]";
                rt.memory[cell] = defaultValue(baseType(symbol));
                rt.types[cell] = baseType(symbol);
            }
        } else {
            rt.memory[place] = defaultValue(baseType(symbol));
        }
    }

    int instructionIndex = 1;
    for (const auto &symbol : symbols) {
        if (symbol.category == "program") {
            continue;
        }
        program.instructions.push_back(make(instructionIndex++, "DATA", symbol.name,
                                            symbol.type, std::to_string(symbol.length)));
    }

    std::map<int, int> quadToInstruction;
    for (const auto &quad : quads) {
        quadToInstruction[quad.index] = instructionIndex;
        if (quad.op == "program") {
            program.instructions.push_back(make(instructionIndex++, "PROC", quad.arg1));
        } else if (quad.op == "end") {
            program.instructions.push_back(make(instructionIndex++, "HALT", quad.arg1));
        } else if (quad.op == ":=") {
            program.instructions.push_back(make(instructionIndex++, "MOV", quad.result, quad.arg1));
        } else if (quad.op == "uminus") {
            program.instructions.push_back(make(instructionIndex++, "NEG", quad.result, quad.arg1));
        } else if (quad.op == "not") {
            program.instructions.push_back(make(instructionIndex++, "NOT", quad.result, quad.arg1));
        } else if (quad.op == "j") {
            program.instructions.push_back(make(instructionIndex++, "JMP", quad.result));
        } else if (quad.op == "jfalse") {
            program.instructions.push_back(make(instructionIndex++, "JZ", quad.arg1, quad.result));
        } else if (quad.op == "=[]") {
            program.instructions.push_back(make(instructionIndex++, "LOADARR", quad.result, quad.arg1, quad.arg2));
        } else if (quad.op == "[]=") {
            program.instructions.push_back(make(instructionIndex++, "STOREARR", quad.result, quad.arg2, quad.arg1));
        } else if (quad.op == "read") {
            program.instructions.push_back(make(instructionIndex++, "READ", quad.result));
        } else if (quad.op == "readarr") {
            program.instructions.push_back(make(instructionIndex++, "STOREARR", quad.result, quad.arg2, "0"));
        } else if (quad.op == "write") {
            program.instructions.push_back(make(instructionIndex++, "WRITE", quad.arg1));
        } else {
            program.instructions.push_back(make(instructionIndex++, targetOp(quad.op),
                                                quad.result, quad.arg1, quad.arg2));
        }
    }

    for (auto &instruction : program.instructions) {
        if (instruction.op == "JMP") {
            const int targetQuad = std::stoi(instruction.a);
            instruction.a = std::to_string(quadToInstruction.count(targetQuad)
                                               ? quadToInstruction[targetQuad]
                                               : instructionIndex);
        } else if (instruction.op == "JZ") {
            const int targetQuad = std::stoi(instruction.b);
            instruction.b = std::to_string(quadToInstruction.count(targetQuad)
                                               ? quadToInstruction[targetQuad]
                                               : instructionIndex);
        }
    }

    if (runVm) {
        int pc = 0;
        int guard = 0;
        while (pc >= 0 && pc < static_cast<int>(program.instructions.size()) && guard++ < 10000) {
            executeInstruction(program.instructions[static_cast<size_t>(pc)], &pc, &rt,
                               &program.trace, &program.output);
        }
        if (guard >= 10000) {
            program.diagnostics.push_back({Severity::Error, "vm",
                                           "execution stopped after 10000 steps; possible infinite loop",
                                           1, 1});
        }
    }

    for (const auto &entry : rt.memory) {
        program.memory.push_back({placeName(entry.first, rt), rt.types[entry.first], entry.second});
    }
    std::sort(program.memory.begin(), program.memory.end(),
              [](const MemoryCell &a, const MemoryCell &b) { return a.name < b.name; });
    return program;
}

} // namespace minipas
