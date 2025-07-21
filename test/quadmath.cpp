#include "test.hpp"

// #include <boost/multiprecision/integer.hpp>
// #include <boost/multiprecision/float128.hpp>

#include "lang/core.hpp"

NAMESPACE_BEGIN()

NAMESPACE_USE(lang)
NAMESPACE_USE(lang::test)

TestResults quadmath_test()
{
    // u128 v1 = boost::multiprecision::uint128_t("128037140814701821871083712787").convert_to<u128>();
    // u128 v2 = boost::multiprecision::uint128_t("180471087401878052736236842356").convert_to<u128>();
    // auto v3 = v1 + v2;

    // std::println("trivially copyable = {}", std::is_trivially_copyable_v<u128>);
    // std::println("trivially destructible = {}", std::is_trivially_destructible_v<u128>);

    // std::println("value = {} (size = {})", boost::multiprecision::to_string(boost::multiprecision::uint128_t(v3)), sizeof(v3));

    // f128 a = str_to_f128("0.1", nullptr);
    // f128 b = str_to_f128("0.2", nullptr);

    // f128 a = str_to_f128("1.2345678901234567890123456789012345678", nullptr);
    // f128 b = str_to_f128("5.2837461976431972639759875938475394837", nullptr);
    // f128 v = a * b;

    // auto input = "0x0.3p+0";
    auto input = "5.28374619764319726397598759384753";
    char* ptr;
    f128 v = str_to_f128(input, &ptr);
    std::println("parsed [{}]", std::string_view(input, ptr));
    if (usz(ptr - input) != strlen(input)) {
        std::println("failed to parse");
        return TestResult::Failed;
    }
    std::println("value = {}", v);

#define TEST(_op, _a, _b, _t) \
    { \
        usz total = 0; \
        usz unsafe = 0; \
        for (i64 a = std::numeric_limits<_a>::lowest(); a <= std::numeric_limits<_a>::max(); ++a) { \
            for (i64 b = std::numeric_limits<_b>::lowest(); b <= std::numeric_limits<_b>::max(); ++b) { \
                if ("/"sv == #_op && b == 0) continue; \
                total++; \
                auto r = a _op b; \
                if (r < std::numeric_limits<_t>::lowest() || r > std::numeric_limits<_t>::max()) unsafe++; \
            } \
        } \
        std::println(#_a " " #_op " " #_b " -> " #_t " = {:5.2f}% ({})", 100.00 * unsafe / total, unsafe); \
    }

    TEST(+, i8, i8, i8)
    TEST(+, u8, u8, u8)
    TEST(+, u8, i8, u8)
    TEST(+, u8, i8, i8)

    std::println("---");

    TEST(-, u8, u8, u8)
    TEST(-, u8, i8, u8)
    TEST(-, u8, i8, i8)

    TEST(-, i8, i8, i8)
    TEST(-, i8, u8, u8)
    TEST(-, i8, u8, i8)

    std::println("---");

    TEST(*, i8, i8, i8)
    TEST(*, u8, u8, u8)
    TEST(*, u8, i8, u8)
    TEST(*, u8, i8, i8)

    std::println("---");

    TEST(/, u8, u8, u8)
    TEST(/, u8, i8, u8)
    TEST(/, u8, i8, i8)

    TEST(/, i8, i8, i8)
    TEST(/, i8, u8, u8)
    TEST(/, i8, u8, i8)

    #undef TEST

    // + mixed prefer unsigned
    // - prefer minuend
    // * mixed prefer unsigned
    // / prefer signed

    return TestResult::Success;
}

Registered _1 = register_test("quadmath", quadmath_test);

NAMESPACE_END
