#ifndef MINIPAS_OPTIMIZER_H
#define MINIPAS_OPTIMIZER_H

#include "core/Compiler.h"

#include <map>

namespace minipas {

struct OptimizeResult {
    std::vector<Quad> quads;
    std::vector<Diagnostic> diagnostics;
};

class Optimizer {
public:
    explicit Optimizer(std::vector<Constant> constants);

    OptimizeResult optimize(const std::vector<Quad> &quads) const;

private:
    bool isBinaryOp(const std::string &op) const;
    bool isCommutative(const std::string &op) const;
    bool isConstantOperand(const std::string &operand) const;
    bool constantValue(const std::string &operand,
                       const std::map<std::string, std::string> &known,
                       std::string *value) const;
    bool fold(const std::string &op, const std::string &left,
              const std::string &right, std::string *value) const;
    void assignBlocks(std::vector<Quad> *quads) const;

    std::vector<Constant> constants_;
};

} // namespace minipas

#endif // MINIPAS_OPTIMIZER_H
