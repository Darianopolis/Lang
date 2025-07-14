#include "test.hpp"
#include "lang/scan.hpp"

#include "lang/compile.hpp"
#include "lang/generate.hpp"

NAMESPACE_BEGIN()

NAMESPACE_USE(lang::test)
NAMESPACE_USE(lang)

Registered _1 = register_test("compile", [] -> TestResults {

    auto input = R"(

fib :: fn(n: i32) -> i32 {
  if n < 2 { n } else { fib(n - 1) + fib(n - 2) }
};

main :: fn() -> i32 {
  fib(40)
};

    )"sv;
    std::string program = std::string(input);
    program.resize(program.size() + 10, '\0');

    ErrorList errors;
    Scanner scanner;
    scanner.init(errors, program);

    Compiler compiler;
    compiler.init();

    Ast ast;
    auto offset = parse(ast, &scanner, compiler.constants);

    ast_print_flat(compiler.constants, ast);
    ast_print_tree(compiler.constants, ast, offset);

    compiler.add({&ast, offset});
    compiler.run();

    Annotator annotator;
    annotator.compiler = &compiler;

    auto i32_type = compiler.globals.find(compiler.constants.identifiers.get_id("i32")).value;
    auto fib      = compiler.globals.find(compiler.constants.identifiers.get_id("fib")).value;
    auto main     = compiler.globals.find(compiler.constants.identifiers.get_id("main")).value;

    log_debug("i32  = {}", annotator.to_string(i32_type));
    log_debug("fib  = {}", annotator.to_string(fib));
    log_debug("main = {}", annotator.to_string(main));

    auto res = std::dynamic_pointer_cast<Function>(main)->reify(annotator, {});
    log_debug("res = {}", annotator.to_string(res.value));

    for (auto& annotated_fragment : compiler.annotated_fragments) {
        log_info("---- Annotated Fragment ----");
        print_annotations(&compiler, annotated_fragment);

        CodeGenerator generator;
        generator.init(compiler, annotated_fragment);
        generator.generate_function();
    }

    return TestResult::Success;
});

NAMESPACE_END
