#include <print>
#include <fstream>
#include <filesystem>

#include <lang/core.hpp>
#include <lang/lang.hpp>
#include <lang/interpret/interpret.hpp>

#include <simdutf.h>

NAMESPACE_USE(lang)

std::string read_file(std::string_view path)
{
    std::ifstream in(std::filesystem::path(path), std::ios::in | std::ios::binary | std::ios::ate);
    auto len = in.tellg();
    in.seekg(0);
    std::string message;
    message.resize_and_overwrite(len, [&](char *data, usz size) {
        in.read(data, size);
        return size;
    });
    return message;
}

int main()
{
    // auto message = "Hello, world!\n";
    // auto message = read_file("src/lang/scan.cpp");
    // auto message = read_file("/media/home/Dev/Data/sf-city-lots-json-master/citylots.json");
    // auto message = "ｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃ";
    // auto message = read_file("test/test.lang");

    // auto message = "-x + 2 * -y++ / 4";

    // auto message = "((x, y, bar) {})(1, 2, .bar = 3)";
    // auto message = "(x: i32, y := 123, clamp := false){}(1, 2, .clamp = true)";
    // auto message = "(foo: (:i32 = 123) -> i32 = (x) { x ** 2 }){ foo(123) }()";
    // auto message = "a + b * c * d + e";
    // auto message = "a & b | (c ~| d)";
    // auto message = "a | b^() > c + d";
    // auto message = "(: ++list = 123) -> i32";
    // auto message = "-a++[].b && !++c()^ || &foo^";
    // auto message = "a & b > c && d";
    // auto message = "main :: () { print(\"Hello, world\") }";

//     auto message = R"(

    auto message = R"(

#[[[
foo :: 2;
1.5 * bar + 3 / math.pi - 5;

bar :: 3;
1.5 * foo + 3 / math.tau - 5;

math.tau :: math.pi * 2;
math.pi  :: 3.14;

x :: 2;
x + y;
y :: 5;

z :: 4;

{
    x :: 2;
    y :: 3;
    x * { foo :: y; foo + 1 } - z
}

x :: 0;
if x { 2 } else { 3 }

{
    x := 0;
    while x < 10 {
        print(x);
        x = x + 1;
    }
}

]]]

x :: 1;
x + 1;

bar :: enum {
    alpha,
    bravo,
    charlie,
};

foo :: struct {
    t: bar;
    x: i32;
    y :: 0;
};

bar_str :: (v) {
    match v {
        bar.alpha   -> { "alpha"   }
        bar.bravo   -> { "bravo"   }
        bar.charlie -> { "charlie" }
    }
}

    )";

#if 0
    auto message = R"(

std   :: @package("std");
mylib := @package("com.foobar.mylib");

foo : i32;
bar :: foo = 42;

mylib.pi : f64 : 3.1415;
mylib.tau :: mylib.pi * 2;

mylib.fib : (n) -> @typeof(n) = (n) {
    if n < 2 { n } else { mylib.fib(n - 1) + mylib.fib(n - 2) }
};

thing := for stuff -> (x) {
    if pred(x) { break Some(x); }
} else { None() };

bar :: {
    123
};

(a + b)[c](d);

std.min :: (x, y) { if x <= y { x } else { y } };

main :: () {
    div := 2;
    for 0..10 -> (x) {
        println(if x % div { "odd" } else { "even" });
    }

    while div > 0 {
        defer { --div; }
        println("{}", div);
    }

    println("fib({}) = {}", x, fib(x));
};

my_enum :: enum {
    alpha,
    bravo,
    charlie
};

gen_to_string_matches :: (e) {
    tokens := ${}
    for e -> (v) {
        tokens += ${
            \{v} -> { \{v.name()} }
        };
    }
    tokens
}

gen_to_string_match :: (e, token_seq) {
    ${
        match \{token_seq} {
            \{gen_to_string_matches(e)}
        }
    }
}

{
    \{gen_to_string_match(my_enum, ${e})}

    #[[
        Expands to
        match e {
            my_enum::alpha   -> { "alpha"   }
            my_enum::bravo   -> { "bravo"   }
            my_enum::charlie -> { "charlie" }
        }
    ]]
}

print(x);
x :: 123;

)";
#endif

    auto start = std::chrono::steady_clock::now();
    auto tokens = scan(message);
    auto end = std::chrono::steady_clock::now();

#if 0
    log_debug("token details");

    for (auto iter = tokens.begin(); !iter.is_at_end(); iter.advance()) {
        auto token = iter.peek();
        switch (token.type) {
            case lang::TokenType::Keyword:
                log_info("{:4} |     keyword |     | {}", iter.index(), token.keyword.value);
                break;
            case TokenType::Identifier:
                log_info("{:4} |  identifier |   {} | {}", iter.index(), token.identifier.is_at ? "@" : " ", token.identifier.value);
                break;
            case TokenType::String:
                if (token.string.is_raw) {
                    log_info("{:4} |      string | raw | {}", iter.index(), token.string.value);
                } else {
                    log_info("{:4} |      string |     | {}", iter.index(), token.string.value);
                }
                break;
            case TokenType::Number:
                log_info("{:4} |      number |  {:2} | {}", iter.index(), token.number.base, token.number.raw);
                break;
            case TokenType::Punctuation:
                // if (auto matched = iter.matching_pair_for(token)) {
                //     log_info("{:4} | punctuation |     | {}         (matching {} @ {})", iter.index(), token.punctuation.value,
                //         matched->punctuation.value, matched.index());
                // } else {
                log_info("{:4} | punctuation |     | {}", iter.index(), token.punctuation.value);
                // }
                break;
            default:
                raise_error("unknown token type: {}", std::to_underlying(token.type));
        }
    }
#endif

#if 0
    log_debug("matched view");

    auto indent_str = "  ";
    auto indent = 0;

    for (auto iter = tokens.begin(); !iter.is_at_end(); iter.advance()) {
        auto token = iter.peek();
        auto matched = tokens.matching_pair_for(token);
        if (!matched.is_at_end() && matched.index() < iter.index()) {
            indent--;
        }

        auto message = std::format("{:4} ", iter.index());
        for (auto i = 0; i < indent0; ++i) {
            message += indent_str;
        }
        message += token.raw;
        log_info("{}", message);

        if (!matched.is_at_end() && matched.index() > iter.index()) {
            indent++;
        }
    }
#endif

    log_debug("scanned in {:.4f}", std::chrono::duration_cast<std::chrono::duration<f64>>(end - start).count());

    start = std::chrono::steady_clock::now();
    auto fragments = parse(tokens);
    end = std::chrono::steady_clock::now();

    log_debug("parsed in {:.4f}", std::chrono::duration_cast<std::chrono::duration<f64>>(end - start).count());

    auto file_context = file_context_create();

    auto math = tag_value(new Namespace{});
    file_context->global->set("math", math);

    file_context->global->set("print", tag_value(new NativeFunction {
        .function = [](ExecutionContext* ctx, std::span<Value* const> args) -> Value* {
            std::string message;
            for (auto& arg : args) {
                message += value_to_string(arg);
            }
            log_warn("{}", message);
            return nullptr;
        },
    }));

    CompileJobSystem job_system;
    for (auto fragment = fragments.head; fragment; fragment = fragment->next) {
        // job_system.add_fragment(fragment, file_context);
        ast_debug_print(fragment);
    }

    log_info("");

    log_debug("bytecode generated");

    job_system.compile();

    log_debug("bytecode interpreted");
}
