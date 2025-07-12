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
            log_error("Test [{}] failed with exception", test.name);
            results += TestResult::Failed;
        }
    }

    log_info("Tests complete");
    log_info(" {:4} succeeded", results.successes);
    if (results.skipped) {
        log_info(" {:4} skipped", results.skipped);
    }
    if (results.failures) {
        log_error(" {:4} failed", results.failures);
    } else {
        log_info(" {:4} failed", results.failures);
    }
}

NAMESPACE_END

extern void samples();

int main()
{
    lang::test::run_tests();
    // samples();
}
