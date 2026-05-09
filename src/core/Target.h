#ifndef MINIPAS_TARGET_H
#define MINIPAS_TARGET_H

#include "core/Compiler.h"

#include <map>

namespace minipas {

struct TargetProgram {
    std::vector<TargetInstruction> instructions;
    std::vector<std::string> trace;
    std::vector<std::string> output;
    std::vector<MemoryCell> memory;
    std::vector<Diagnostic> diagnostics;
};

class TargetGenerator {
public:
    TargetProgram generate(const std::vector<Quad> &quads,
                           const std::vector<Symbol> &symbols,
                           const std::vector<Constant> &constants,
                           bool runVm) const;

private:
    TargetInstruction make(int index, const std::string &op,
                           const std::string &a = "",
                           const std::string &b = "",
                           const std::string &c = "") const;
};

} // namespace minipas

#endif // MINIPAS_TARGET_H
