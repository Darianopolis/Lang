#include "test.hpp"

#include "lang/lang.hpp"
#include "lang/scan.hpp"
#include "lang/strings.hpp"

#include <random>

#include <simdutf.h>

NAMESPACE_BEGIN()

NAMESPACE_USE(lang)
NAMESPACE_USE(lang::test)

TestResults test_scan_parse(Constants& constants, std::string_view input, std::span<const Token> expected, std::span<const u32> expected_ast = {})
{
    // log_trace("testing: {}", input);

    ErrorList errors;

    Scanner scanner;

    scanner.init(errors, input);

    u32 i = 0;
    while (auto token = scanner.next()) {
        if (i >= expected.size()) {
            log_error("test_scan :: expected {} tokens, got at least {}", expected.size(), i + 1);
            return TestResult::Failed;
        }
        bool not_equal = token.type != expected[i].type;
        if (!not_equal) {
            switch (token.type) {
                case TokenTypes::Identifier: not_equal = token.identifier   != expected[i].identifier;   break;
                case TokenTypes::String:     not_equal = token.string.value != expected[i].string.value; break;
                case TokenTypes::Number:     not_equal = token.number       != expected[i].number;       break;
            }
        }
        if (not_equal) {
            log_error("test_scan :: [{}] expected {} got {}", i, token_to_string(expected[i]), token_to_string(token));
            return TestResult::Failed;
        }
        i++;
    }

    if (errors.any_errors()) {
        for (auto err : errors) err.print(input);
        return TestResult::Failed;
    }

    if (!expected_ast.empty()) {
        Ast ast;
        scanner.init(errors, input);
        auto offset = parse(ast, &scanner, constants);
        Ast compacted;
        ast_compact(constants, ast, offset, compacted);
        u32 max = std::min(compacted.code.size(), expected_ast.size());
        for (u32 i = 0; i < max; ++i) {
            if (compacted.code[i] != expected_ast[i]) {
                auto to_str = [&](u32 v) { return v >= AstHeader_Start ? std::string(get_ast_header_name(AstHeader(v))) : std::to_string(v); };
                log_error("test_scan :: AST[{}] expected {} got {}", i, to_str(expected_ast[i]), to_str(compacted.code[i]));
                return TestResult::Failed;
            }
        }
    }

    return TestResult::Success;
}

TestResults test_negatives()
{
    /*

    error_reporting
        how to test that caret position for lines containing tabs and unicode characters is correct?

    strings
        escapes
            invalid
        unicode escapes
            too long
            too large
            surrogate

    */

    return TestResult::Skipped;
}

TestResults test_positives()
{
    using enum AstHeader;

    Constants constants;

    auto sym = [&](std::string_view type) consteval -> Token { return {punc(type)};                                            };
    auto kyw = [&](std::string_view type)           -> Token { return {keyword(type)};                                         };
    auto num = [&](std::string_view value)          -> Token { return {TokenTypes::Number, 0, 0, {.number = {value, 10}}};     };
    auto bin = [&](std::string_view value)          -> Token { return {TokenTypes::Number, 0, 0, {.number = {value, 2}}};      };
    auto oct = [&](std::string_view value)          -> Token { return {TokenTypes::Number, 0, 0, {.number = {value, 8}}};      };
    auto hex = [&](std::string_view value)          -> Token { return {TokenTypes::Number, 0, 0, {.number = {value, 16}}};     };
    auto str = [&](std::string_view value)          -> Token { return {TokenTypes::String, 0, 0, {.string = {value}}};         };
    auto id  = [&](std::string_view value)          -> Token { return {TokenTypes::Identifier, 0, 0, {.identifier = {value}}}; };

    auto _num =  [&](std::string_view val) { return constants.numbers.get_id(val);     };
    auto _id =   [&](std::string_view val) { return constants.identifiers.get_id(val); };
    auto ast  =  [&](AstHeader header)     { return std::to_underlying(header);        };

    TestResults res;

    // operator precedence
    res += test_scan_parse(constants, "1 + ++a^ * 3",
        { num("1"), sym("+"), sym("++"), id("a"), sym("^"), sym("*"), num("3") },
        { ast(File),
            ast(Add),
                ast(Literal_Number), _num("1"),
                ast(Multiply),
                    ast(Dereference),
                        ast(Preincrement),
                            ast(Identifier), _id("a"),
                    ast(Literal_Number), _num("3") });

    // keywords
    res += test_scan_parse(constants, "for 0..12 -> (x) {}",
        { kyw("for"), num("0"), sym(".."), num("12"), sym("->"), sym("("), id("x"), sym(")"), sym("{"), sym("}")});

    // strings
    res += test_scan_parse(constants, R"( 'raw"'""'"raw' )",                 { str("'\"\"'")                            });

#if 0
    res += test_scan_parse(constants, R"( "foo bar\" " )",                   { str("foo bar\" ")                        });
    res += test_scan_parse(constants, R"( "before" "🔥\u{1f525}" "after" )", { str("before"), str("🔥🔥"), str("after") });
#else
    res += TestResults(TestResult::Skipped, 2);
#endif

    // numbers
    res += test_scan_parse(constants, "0.1e+2",        { num("0.1e+2")        });
    res += test_scan_parse(constants, "0x4f.f12p-4",   { hex("4f.f12p-4")     });
    res += test_scan_parse(constants, "0o777#foo ba",  { oct("777")           });
    res += test_scan_parse(constants, "0123",          { num("0123")          });
    res += test_scan_parse(constants, "0b01101",       { bin("01101")         });
    res += test_scan_parse(constants, "10_000_000.00", { num("10_000_000.00") });

    // comments
    res += test_scan_parse(constants, "# 0123",             {            });
    res += test_scan_parse(constants, "#[\n,pmqg79\n] 123", { num("123") });

    // keywords
    {
        TestResults tmp;
        for (auto& k : Keywords) {
            tmp += test_scan_parse(constants, k, { {keyword_runtime(k)} });
        }
        res += tmp.collapse();
    }

    // punctuation
    {
        TestResults tmp;
        for (auto& _p : PunctuationSequences) {
            tmp += test_scan_parse(constants, _p, { {punc_runtime(_p)} });
        }
        res += tmp.collapse();
    }

    return res;
}

TestResults scan_fuzz()
{
    Constants constants;
    ErrorList errors;

    auto seed = std::random_device{}();

    log_trace("Fuzzing (seed = {})", seed);

    std::mt19937 rng{seed};
    std::uniform_int_distribution<u8> dist{0, 255};
    std::uniform_int_distribution<u8> spaces{0, 5};
    std::uniform_int_distribution<u32> ascii_dist{1, 127};
    std::uniform_int_distribution<u32> unicode_dist{128, 1114111};

    constexpr auto InputSize = 100;
    constexpr auto Iterations = 100;

    std::vector<char32_t> generated_utf32;
    std::string input;

    Scanner scanner;

    for (u32 iteration = 0; iteration < Iterations; ++iteration) {

        generated_utf32.clear();
        input.clear();
        for (u32 i = 0; i < InputSize; i++) {
            // auto value = (rng() & 1) ? codepoint_dist(rng) : ascii_dist(rng);
            auto value = spaces(rng) == 0 ? ' ' : ascii_dist(rng);

            if (value > 0xD7FF && value < 0xE000) continue; // surrogates
            if (value < 32) {
                // control characters
                if (value != '\n' && value != '\t') continue;
            }
            if (value == 127) continue; // delete

            generated_utf32.emplace_back(char32_t(value));
        }

        auto len = simdutf::utf8_length_from_utf32(generated_utf32);
        input.resize(len);

        auto res = simdutf::convert_utf32_to_utf8_with_errors(generated_utf32, input);
        if (res.is_err()) {
            raise_error("{}", simdutf_err_to_string(res.error));
        }

        // log_trace("source:\n{}", input);

        constants.clear();
        errors.clear();
        {
            // LogLevelGuard _{LogLevel::Fatal};
            scanner.init(errors, input);
            while (scanner.next());
        }
        // for (auto err : errors) err.print(input);
    }

    return TestResult::Success;
}

Registered _1 = register_test("scan.positive", test_positives);
Registered _2 = register_test("scan.negative", test_negatives);
Registered _3 = register_test("scan.fuzz", scan_fuzz);

NAMESPACE_END
