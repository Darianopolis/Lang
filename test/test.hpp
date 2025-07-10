#pragma once

#include "lang/core.hpp"

NAMESPACE_BEGIN(lang::test)

enum class TestResult
{
    Skipped,
    Failed,
    Success,
};

struct TestResults
{
    usz successes = 0;
    usz failures = 0;
    usz skipped = 0;

    TestResults() = default;

    TestResults(TestResult res)
        : successes(res == TestResult::Success)
        , failures(res == TestResult::Failed)
        , skipped(res == TestResult::Skipped)
    {}

    TestResults& operator+=(const TestResults& res)
    {
        successes = successes + res.successes;
        failures = failures + res.failures;
        skipped = skipped + res.skipped;
        return *this;
    }

    TestResults operator+(const TestResults& other) const
    {
        TestResults combined{*this};
        combined += other;
        return combined;
    }

    TestResult collapse() const
    {
        if (failures) return TestResult::Failed;
        if (!successes) return TestResult::Skipped;
        return TestResult::Success;
    }
};

struct Registered {};

Registered register_test(std::string_view name, TestResults(*)());

NAMESPACE_END
