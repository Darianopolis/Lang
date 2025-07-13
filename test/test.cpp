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

TestResults run_test(Test& test)
{
    log_info("Running {}..", test.name);
    // try {
        return test.function();
    // } catch(...) {
    //     log_error("Test [{}] failed with exception", test.name);
    //     return TestResult::Failed;
    // }
}

void run_tests(int argc, char* argv[])
{
    if (argc < 2) {
        log_info("Tests:");
        for (auto& test : tests) {
            log_info(" - {}", test.name);
        }
        return;
    }

    TestResults results;

    log_info("argv[1] = [{}]", argv[1]);

    if ("--all"sv == argv[1] || "-A"sv == argv[1]) {
        log_info("Running all tests..", tests.size());

        for (auto& test : tests) {
            results += run_test(test);
        }
    } else {
        for (i32 i = 1; i < argc; ++i) {
            auto test_name = argv[i];

            bool found = false;
            for (auto& test : tests) {
                if (test.name == test_name) {
                    results += run_test(test);
                    found = true;
                    break;
                }
            }
            if (!found) {
                log_error("Could not find test: {}", test_name);
            }
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

int main(int argc, char* argv[])
{
    lang::test::run_tests(argc, argv);
}
