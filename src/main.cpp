#include <print>
#include <fstream>
#include <filesystem>

#include <lang/core.hpp>
#include <lang/lang.hpp>

#include <simdutf.h>

NAMESPACE_USE(lang)

int main()
{
    // auto message = read_file("test/small.lang");
    // auto message = read_file("test/large.lang");

    // auto message = read_file("test/vulkan_test.lang");

    // std::string_view message = "x := 2; 1 + x * 2";

#if 1
    std::string_view message = R"(
x := 1;
while x < 10 {
    x += 1;
    if x % 2 == 0 {
        x *= math.pi;
        break foo;
    } else {
        continue;
    }
} else {
    123
}

for 0..10 -> (x) {
    print(x);
} else { 0 }

fib :: (n: f64) -> f64 {
    defer { print("Hello, world!"); }
    if n < 2 { n } else { fib(n - 1) + fib(n - 2) }
};

++foo^.bar(1, 2, "3");

foo :: @as(u8, 1234);
bar :: @typeOf(foo);
alpha: u32 = undefined;
if beta != null and x {}
)";
#endif

#if 0
    std::string_view message = R"(

#[[
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
    x * { foo :: x; foo = foo + 2; foo } - z
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
]]

x := 1;
while x < 10 {
    x += 1
}
    )";
#endif

#if 0
    std::string_view message = R"(

accumulate_tokens :: (input, fn) {
    tokens := ${};
    for input -> (i) {
        tokens += fn(i);
    }
    tokens
};

my_enum :: enum {
    alpha,
    bravo,
    charlie
};

gen_to_string_matches :: (e) {
    accumulate_tokens(e, (v) {
        ${\{v} -> { \{v.name()} }}
    })
};

gen_to_string_match :: (e, token_seq) {
    ${
        match \{token_seq} {
            \{gen_to_string_matches(e)}
        }
    }
};

{
    \{gen_to_string_match(my_enum, ${e})}

    match e {
        \{gen_to_string_matches(e)}
        _ -> { "unknown" }
    }

    #[[
        Expands to
        match e {
            my_enum::alpha   -> { "alpha"   }
            my_enum::bravo   -> { "bravo"   }
            my_enum::charlie -> { "charlie" }
            _                -> { "unknown" }
        }
    ]]
}

    )";
#endif

#if 0
    std::string_view message = R"(

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

print(x);
x :: 123;

)";
#endif

    log_debug("scanning...");

    TokenList tokens;
    tokens.tokens.reserve(message.size());
    Constants constants;

    auto start = std::chrono::steady_clock::now();
    scan(tokens, constants, message);
    auto end = std::chrono::steady_clock::now();

#if 1
    log_debug("tokens");

    auto indent_str = ". ";
    auto indent = 0;

    for (auto iter = tokens.begin(); !iter.is_at_end(); iter.advance()) {
        auto token = iter.peek();
        auto matched = tokens.matching_pair_for(token);
        if (!matched.is_at_end() && matched.index() < iter.index()) {
            indent--;
        }

        auto message = std::format("{:7} ", iter.index());
        for (auto i = 0; i < indent; ++i) {
            message += indent_str;
        }
        message += token_to_string(constants, token);
        std::println("{}", message);

        if (!matched.is_at_end() && matched.index() > iter.index()) {
            indent++;
        }
    }
#endif

    {
        auto seconds = std::chrono::duration_cast<std::chrono::duration<f64>>(end - start).count();
        auto size = tokens.tokens.size();
        log_debug("scanned in {} ({}/s)", duration_to_string(end - start), byte_size_to_string(message.size() / seconds));
        log_debug("tokenlist size = {} ({:.2f}x)", byte_size_to_string(size), f64(size) / message.size());
    }

#if 1
    start = std::chrono::steady_clock::now();
    auto[ast, offset] = parse(tokens, constants);
    end = std::chrono::steady_clock::now();

    {
        auto seconds = std::chrono::duration_cast<std::chrono::duration<f64>>(end - start).count();
        log_debug("parsed in {} ({}/s)", duration_to_string(end - start), byte_size_to_string(message.size() / seconds));
    }

    ast_print_details(constants, ast, offset, message);
#endif

#if 0
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
        job_system.add_fragment(fragment, file_context);
        // ast_debug_print(fragment);
    }

    log_info("");

    log_debug("bytecode generated");

    job_system.compile();

    log_debug("bytecode interpreted");
#endif
}
