#include "core/Compiler.h"

#include "core/Lexer.h"
#include "core/Optimizer.h"
#include "core/Parser.h"
#include "core/Target.h"

#include <algorithm>

namespace minipas {

std::string tokenKindName(TokenKind kind) {
    switch (kind) {
    case TokenKind::Keyword: return "keyword";
    case TokenKind::Identifier: return "identifier";
    case TokenKind::Delimiter: return "delimiter";
    case TokenKind::Constant: return "constant";
    case TokenKind::End: return "end";
    case TokenKind::Invalid: return "invalid";
    }
    return "invalid";
}

std::string severityName(Severity severity) {
    switch (severity) {
    case Severity::Info: return "info";
    case Severity::Warning: return "warning";
    case Severity::Error: return "error";
    }
    return "error";
}

CompilationResult Compiler::compile(const std::string &source,
                                    const CompileOptions &options) const {
    CompilationResult result;
    Lexer lexer(source);
    LexResult lexed = lexer.scan();

    result.keywordTable = Lexer::keywordTable();
    result.delimiterTable = Lexer::delimiterTable();
    result.tokens = lexed.tokens;
    result.identifiers = lexed.identifiers;
    result.constants = lexed.constants;
    result.diagnostics = lexed.diagnostics;

    Parser parser(result.tokens, result.constants, result.identifiers);
    ParseResult parsed = parser.parse();
    result.symbols = parsed.symbols;
    result.astText = parsed.astText;
    result.quads = parsed.quads;
    result.diagnostics.insert(result.diagnostics.end(),
                              parsed.diagnostics.begin(),
                              parsed.diagnostics.end());

    const bool hasErrors = std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                                       [](const Diagnostic &d) {
                                           return d.severity == Severity::Error;
                                       });
    if (hasErrors) {
        result.optimizedQuads = result.quads;
        result.success = false;
        return result;
    }

    if (options.enableOptimization) {
        Optimizer optimizer(result.constants);
        OptimizeResult optimized = optimizer.optimize(result.quads);
        result.optimizedQuads = optimized.quads;
        result.diagnostics.insert(result.diagnostics.end(),
                                  optimized.diagnostics.begin(),
                                  optimized.diagnostics.end());
    } else {
        result.optimizedQuads = result.quads;
    }

    TargetGenerator target;
    TargetProgram program = target.generate(result.optimizedQuads,
                                            result.symbols,
                                            result.constants,
                                            options.runVm);
    result.targetCode = program.instructions;
    result.vmTrace = program.trace;
    result.vmOutput = program.output;
    result.finalMemory = program.memory;
    result.diagnostics.insert(result.diagnostics.end(),
                              program.diagnostics.begin(),
                              program.diagnostics.end());

    result.success = std::none_of(result.diagnostics.begin(), result.diagnostics.end(),
                                  [](const Diagnostic &d) {
                                      return d.severity == Severity::Error;
                                  });
    return result;
}

} // namespace minipas
