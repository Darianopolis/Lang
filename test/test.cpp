#include "test.hpp"

NAMESPACE_BEGIN(lang::test)

struct Test
{
    std::string_view name;
    TestResults(*function)();
};

std::vector<Test> tests;

Registered register_test(std::string_view name, TestResults(*function)())
{
    tests.emplace_back(name, function);
    return {};
}

void run_tests()
{
    log_info("Running {} tests..", tests.size());

    TestResults results;
    for (auto& test : tests) {
        try {
            results += test.function();
        } catch(...) {
            results += TestResult::Failed;
        }
    }

    log_info("Tests complete");
    log_info("  {} succeeded", results.successes);
    if (results.failures) {
        log_error("  {} failed", results.failures);
    } else {
        log_info("  {} failed", results.failures);
    }
}

NAMESPACE_END

int main()
{
    lang::test::run_tests();
}
