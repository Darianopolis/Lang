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

    TestResults(TestResult res, u32 count = 1)
        : successes(res == TestResult::Success ? count : 0)
        , failures(res == TestResult::Failed ? count : 0)
        , skipped(res == TestResult::Skipped ? count : 0)
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

// -----------------------------------------------------------------------------

void report_speed(std::string_view label, u64 bytes, auto duration)
{
    auto seconds = std::chrono::duration_cast<std::chrono::duration<f64>>(duration).count();
    std::println("{} {} in {} ({}/s)",
        label,
        byte_size_to_string(bytes),
        duration_to_string(duration),
        byte_size_to_string(u64(f64(bytes) / seconds)));
}

NAMESPACE_END
