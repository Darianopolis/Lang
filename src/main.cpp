#include <print>
#include <fstream>
#include <filesystem>

#include <lang/core.hpp>
#include <lang/lang.hpp>

#include <simdutf.h>

LANG_NAMESPACE_USE

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
    auto message = read_file("test/test.lang");

    auto start = std::chrono::steady_clock::now();
    auto tokens = scan(message);
    auto end = std::chrono::steady_clock::now();

    log_debug("token details");

    for (auto iter = tokens.begin(); iter; ++iter) {
        auto token = *iter;
        switch (token.type) {
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
            case TokenType::Puncuation:
                if (auto matched = iter.matching_pair_for(token)) {
                    log_info("{:4} | punctuation |     | {} matching {} @ {}", iter.index(), char(token.punctuation.value),
                        char(matched->punctuation.value), matched.index());
                } else {
                    log_info("{:4} | punctuation |     | {}", iter.index(), char(token.punctuation.value));
                }
                break;
            default:
                break;
        }
    }

    log_debug("matched view");

    auto indent_str = "  ";
    auto indent = 0;

    for (auto iter = tokens.begin(); iter; ++iter) {
        auto token = *iter;
        auto matched = iter.matching_pair_for(token);
        if (matched && matched.index() < iter.index()) {
            indent--;
        }

        auto message = std::format("{:4} ", iter.index());
        for (auto i = 0; i < indent; ++i) {
            message += indent_str;
        }
        message += token.raw;
        log_info("{}", message);

        if (matched && matched.index() > iter.index()) {
            indent++;
        }
    }

    log_debug("scanned in {:.4f}", std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count());
}
