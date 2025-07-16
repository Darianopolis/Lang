#pragma once

#include "lang.hpp"

#include "strings.hpp"

#include <cmath>

NAMESPACE_BEGIN(lang)

struct Scanner
{
    struct StopScanFlag {};

    ErrorList* errors;
    const char* source;
    usz source_len;
    usz position;

    std::string string_scratch;
    Token token;
    u32 line = 0;

// -----------------------------------------------------------------------------

    std::string_view sub(usz offset, usz count)
    {
        return std::string_view(source).substr(offset, count);
    }

// -----------------------------------------------------------------------------

    LANG_forceinline
    void new_line(u32 pos)
    {
        (void)pos;

        line++;
    }

    template<typename ...Args>
    LANG_noinline
    void report_error_nothrow(u32 head, u32 tail, std::format_string<Args...> fmt, Args&& ... args)
    {
        auto message = std::format(fmt, std::forward<Args>(args)...);
        errors->errors.emplace_back(Error {
            // TODO: Are we always still on the same line?
            .line = line,
            // .file_offset = pos,
            .head_file_offset = head,
            .tail_file_offset = tail,
            .message = message,
        });
    }

    template<typename ...Args>
    [[noreturn]]
    LANG_noinline
    void report_error(u32 head, u32 tail, std::format_string<Args...> fmt, Args&& ... args)
    {
        report_error_nothrow(head, tail, fmt, std::forward<Args>(args)...);
        throw StopScanFlag{};
    }

// -----------------------------------------------------------------------------

    LANG_forceinline
    bool is_at_end()
    {
        return position >= source_len;
    }

    LANG_forceinline
    u8 advance()
    {
        // log_trace("c = {}", ascii_debug_string(source[position]));
        if (position > source_len) report_error(token.file_offset, position - 2, "unexpected eof while parsing");
        return source[position++];
    }

    LANG_forceinline
    u8 peek()
    {
        return source[position];
    }

    LANG_forceinline
    u8 peek_next()
    {
        return source[position + 1];
    }

// -----------------------------------------------------------------------------

    LANG_forceinline bool match(char c)
    {
        if (peek() == c) {
            position++;
            return true;
        }
        return false;
    }

    LANG_forceinline void consume(char c)
    {
        if (!match(c)) report_error(position, position, "expected: {} got {}", ascii_debug_string(c), ascii_debug_string(peek()));
    }

    LANG_forceinline bool match(std::string_view str)
    {
        auto start = position;
        for (auto c : str) {
            if (!match(c)) {
                position = start;
                return false;
            }
        }
        return true;
    }

    LANG_forceinline bool match_identifier(std::string_view str)
    {
        auto start = position;
        for (auto c : str) {
            if (!match(c)) {
                position = start;
                return false;
            }
        }
        switch (peek()) {
            case 'a' ... 'z':
            case 'A' ... 'Z':
            case '0' ... '9':
            case '_':
                // NOT end of identifier
                position = start;
                return false;
            default:
                return true;
        }
    }

    LANG_forceinline bool match(std::string_view str, char last)
    {
        auto start = position;
        for (auto c : str) {
            if (!match(c)) {
                position = start;
                return false;
            }
        }
        if (!match(last)) {
            position = start;
            return false;
        }
        return true;
    }

// -----------------------------------------------------------------------------

    void finish_comment()
    {
        auto comment_start = position - 1;

        u32 count = 0;
        while (peek() == '[') {
            advance();
            count++;
        }

        if (count) {
            // multi-line

            for (;;) {
                if (is_at_end()) {
                    report_error(comment_start, position, "reached end-of-file parsing block comment");
                }
                auto c = peek();
                if (c == '\n') new_line(position + 1);
                if (c == '#' && peek_next() == '[') {
                    advance();
                    finish_comment();
                } else if (c == ']') {
                    bool end = true;
                    for (u32 i = 1; i < count; ++i) {
                        advance();
                        if (peek() != ']') {
                            end = false;
                            break;
                        }
                    }
                    if (end) {
                        advance();
                        return;
                    }
                } else {
                    advance();
                }
            }
        } else {
            // single-line

            while (!is_at_end()) {
                if (advance() == '\n') {
                    new_line(position);
                    return;
                }
            }
        }

    }

// -----------------------------------------------------------------------------

#if 0
    std::string decoded;

    LANG_noinline
    void handle_escape(bool& any_escapes, auto& start)
    {
        if (!any_escapes) {
            decoded.clear();
            any_escapes = true;
        }
        if (auto copy_len = position - 1 - start) {
            decoded.append(source.data() + start, copy_len);
        }

        switch (advance()) {
            case '"': decoded += '"'; break;
            case '0': decoded += '\0'; break;
            case 'a': decoded += '\a'; break; // bell
            case 'b': decoded += '\b'; break; // backspace
            case 'e': decoded += '\e'; break; // escape
            case 'f': decoded += '\f'; break; // form feed
            case 'n': decoded += '\n'; break; // line feed
            case 'r': decoded += "\r"; break; // carriage return
            case 't': decoded += "\t"; break; // horizontal tab
            case 'v': decoded += "\v"; break; // vertical tab
            case 'u': {
                if (advance() != '{') report_error(position - 1, "expected '{{' to begin unicode escape sequence");
                u32 escape_start = position;
                char32_t value = 0;
                u32 digit_count = 0;
                for (;;) {
                    auto u = advance();
                    if (u == '}') break;
                    if (++digit_count > 6) report_error(position, "unicode sequence can be no longer than 6 hex digits");
                    auto d = maybe_hex_digit_to_int(u);
                    if (!d) report_error(position, "expected hex digit");
                    value = (value << 4) | *d;
                }

                char utf8[4];
                auto[error, count] = simdutf::convert_utf32_to_utf8_with_errors(&value, 1, utf8);
                if (error != simdutf::error_code::SUCCESS) {
                    report_error(escape_start, "invalid unicode value ({:#x}): {}", u32(value), simdutf_err_to_string(error));
                }

                decoded.append(utf8, count);
                break;
            }
            default:
                report_error(position - 1, "illegal escape character '{}'", source[position - 1]);
        }
    }
#endif

    LANG_forceinline
    void finish_string()
    {
        bool any_escapes = false;
        auto start = position;
        for (;;) {
            auto c = advance();
            if (c == '"') break;
            if (c == '\n') line++;
            if (c == '\\') {
                any_escapes = true;
                advance();
                // handle_escape(any_escapes, start);
            }
        }
        token.string = {
            .value = sub(start, position - start - 1),
            .has_escapes = any_escapes,
        };
    }

    void finish_rawstring()
    {
        auto start = position;
        while (advance() != '"');
        auto content_start = position;
        auto term = std::string_view(source + start, position - start - 1);
        if (term.empty()) {
            while (advance() != '"' || !match('\''));
        } else {
            while (advance() != '"' || !match(term, '\''));
        }
        token.string = {
            .value = sub(content_start, position - term.size() - content_start - 2),
        };
    }

// -----------------------------------------------------------------------------

    void finish_identifier()
    {
        for (;;) {
            switch (peek()) {
                case 'a' ... 'z':
                case 'A' ... 'Z':
                case '0' ... '9':
                case '_':
                case 128 ... 255:
                    position++;
                    break;
                default:
                    goto end;
            }
        }
    end:
        token.identifier = {
            .value = sub(token.file_offset, position - token.file_offset),
        };
    }

// -----------------------------------------------------------------------------

    LANG_forceinline
    bool match_binary_digit()
    {
        if (peek() == '0' || peek() == '1') {
            position++;
            return true;
        }
        return false;
    }

    LANG_forceinline
    bool match_binary_digit_continuation()
    {
        switch (peek()) {
            case '_':
                advance();
                if (!match_binary_digit()) {
                    report_error(position - 1, token.file_offset, "digit separator must be followed by digit");
                }
                return true;
            case '0' ... '1':
                position++;
                return true;
            default:
                return false;
        }
    }

    LANG_forceinline
    bool match_octal_digit()
    {
        switch (peek()) {
            case '0' ... '7':
                position++;
                return true;
            default:
                return false;
        }
    }

    LANG_forceinline
    bool match_octal_digit_continuation()
    {
        switch (peek()) {
            case '_':
                advance();
                if (!match_octal_digit()) {
                    report_error(position - 1, token.file_offset, "digit separator must be followed by digit");
                }
                return true;
            case '0' ... '7':
                position++;
                return true;
            default:
                return false;
        }
    }

    LANG_forceinline
    bool match_decimal_digit()
    {
        switch (peek()) {
            case '0' ... '9':
                position++;
                return true;
            default:
                return false;
        }
    }

    LANG_forceinline
    bool match_decimal_digit_continuation()
    {
        switch (peek()) {
            case '_':
                advance();
                if (!match_decimal_digit()) {
                    report_error(position - 1, token.file_offset, "digit separator must be followed by digit");
                }
                return true;
            case '0' ... '9':
                position++;
                return true;
            default:
                return false;
        }
    }

    LANG_forceinline
    bool match_hexadecimal_digit()
    {
        switch (peek()) {
            case '0' ... '9':
            case 'a' ... 'f':
            case 'A' ... 'F':
                position++;
                return true;
            default:
                return false;
        }
    }

    LANG_forceinline
    bool match_hexadecimal_digit_continuation()
    {
        switch (peek()) {
            case '_':
                advance();
                if (!match_hexadecimal_digit()) {
                    report_error(position - 1, token.file_offset, "digit separator must be followed by digit");
                }
                return true;
            case '0' ... '9':
            case 'a' ... 'f':
            case 'A' ... 'F':
                position++;
                return true;
            default:
                return false;
        }
    }

    LANG_forceinline
    void end_number(u8  base)
    {
        switch (peek()) {
            case 'a' ... 'z':
            case 'A' ... 'Z':
            case '_':
            case '$':
            case '@':
            case '0' ... '9':
                switch (base) {
                    case 2:  report_error(position, token.file_offset, "invalid character in binary integer literal");
                    case 8:  report_error(position, token.file_offset, "invalid character in octal integer literal");
                    case 10: report_error(position, token.file_offset, "invalid character in integer literal");
                    case 16: report_error(position, token.file_offset, "invalid character in hexadeicmal integer literal");
                    default: std::unreachable();
                }

        }
        token.number = {
            .value = sub(token.file_offset, position - token.file_offset),
            .base = base,
        };
        // log_debug("ending number, start = {}, end = {}, value = '{}'", token.file_offset, position, token.number.value);
    }

    void finish_binary()
    {
        token.file_offset = position;
        if (!match_octal_digit()) {
            report_error(position, position - 2, "expected digit after binary literal introducer");
        }
        while (match_binary_digit_continuation());
        end_number(2);
    }

    void finish_octal()
    {
        token.file_offset = position;
        if (!match_octal_digit()) {
            report_error(position, position - 2, "expected digit after octal literal introducer");
        }
        while (match_octal_digit_continuation());
        end_number(8);
    }

    void finish_hexadecimal()
    {
        token.file_offset = position;
        if (!match_hexadecimal_digit()) {
            report_error(position, position - 2, "expected digit after hexadecimal literal introducer");
        }
        while (match_hexadecimal_digit_continuation());
        auto pos = position;
        if (match('.')) {
            if (peek() == '.') {
                position = pos;
            } else {
                if (!match_hexadecimal_digit()) {
                    report_error(position, token.file_offset, "expected digit to begin fractional part");
                }
                while (match_hexadecimal_digit_continuation());
                if (match("p")) {
                    match('+') || match('-');
                    while (match_decimal_digit_continuation());
                }
            }
        } else {
            position = pos;
        }
        end_number(16);
    }

    LANG_forceinline
    void finish_decimal_fraction()
    {
        auto prev = position - 1;
        if (peek() == '.') {
            position = prev;
        } else {
            if (!match_decimal_digit()) {
                    report_error(position, token.file_offset, "expected digit to begin fractional part");
            }
            while (match_decimal_digit_continuation());
            if (match("e")) {
                match('+') || match('-');
                match_decimal_digit();
                while (match_decimal_digit_continuation());
            }
        }
        end_number(10);
    }

    LANG_forceinline
    void finish_decimal(char first)
    {
        token.number.base = 10;
        while (match_decimal_digit_continuation());
        if (match(".")) {
            finish_decimal_fraction();
        } else {
            end_number(10);
        }
    }

    LANG_forceinline
    void finish_zero_prefixed_number()
    {
        switch (auto c = advance()) {
            case 'b': finish_binary(); break;
            case 'o': finish_octal(); break;
            case 'x': finish_hexadecimal(); break;
            case '.': finish_decimal_fraction(); break;
            case '0' ... '9': finish_decimal(c); break;
            default:
                // '0'
                position--;
                end_number(10);
                break;
        }
    }

// -----------------------------------------------------------------------------

    LANG_forceinline
    Token make_token(TokenType type)
    {
        token.type = type;
        return token;
    }

    LANG_forceinline
    Token next() try
    {
        token = {
            .line = line,
            .file_offset = u32(position),
        };

        for (;;) {
            uint8_t c = advance();
            switch (c) {
                case '\0':
                    return make_token(TokenTypes::Eof);
                case '\n':
                    line++;
                    token.line = line;
                    token.file_offset++;
                    break;
                case ' ':
                    while (peek() == ' ') position++;
                    token.file_offset = position;
                    break;
                case '\t':
                    while (peek() == '\t') position++;
                    token.file_offset = position;
                    break;

                case '0':
                    finish_zero_prefixed_number();
                    return make_token(TokenTypes::Number);
                case '1' ... '9':
                    finish_decimal(c);
                    return make_token(TokenTypes::Number);

                case '^': return make_token(punc("^"));
                case '(': return make_token(punc("("));
                case ')': return make_token(punc(")"));
                case '{': return make_token(punc("{"));
                case '}': return make_token(punc("}"));
                case '[': return make_token(punc("["));
                case ']': return make_token(punc("]"));
                case ':': return make_token(punc(":"));
                case ';': return make_token(punc(";"));
                case ',': return make_token(punc(","));
                case '?': return make_token(punc("?"));

                case '/': return make_token(match('=') ? punc("/=") : punc("/"));
                case '%': return make_token(match('=') ? punc("%=") : punc("%"));
                case '!': return make_token(match('=') ? punc("!=") : punc("!"));
                case '=': return make_token(match('=') ? punc("==") : punc("="));
                case '&': return make_token(match('=') ? punc("&=") : punc("&"));
                case '|': return make_token(match('=') ? punc("|=") : punc("|"));

                case '+':
                    if (match('=')) return make_token(punc("+="));
                    if (match('+')) return make_token(punc("++"));
                    return make_token(punc("+"));
                case '-':
                    if (match('=')) return make_token(punc("-="));
                    if (match('-')) return make_token(punc("--"));
                    if (match(">")) return make_token(punc("->"));
                    return make_token(punc("-"));
                case '*': return make_token(match('*') ? (match('=') ? punc("**=") : punc("**")) : (match('=') ? punc("*=") : punc("*")));
                case '<': return make_token(match('<') ? (match('=') ? punc("<<=") : punc("<<")) : (match('=') ? punc("<=") : punc("<")));
                case '>': return make_token(match('>') ? (match('=') ? punc(">>=") : punc(">>")) : (match('=') ? punc(">=") : punc(">")));
                case '~': return make_token(match('|') ? (match('=') ? punc("~|=") : punc("~|")) : punc("~"));
                case '.': return make_token(match('.') ? (match('=') ? punc("..=") : punc("..")) : punc("."));
                case '$': consume('{'); return make_token(punc("${"));
                case '\\':
                    if (match('{')) return make_token(punc("\\{"));
                    if (match('i')) {
                        consume('d');
                        consume('{');
                        return make_token(punc("\\id{"));
                    }
                    report_error(position, position - 1, "expected {{ or id{{ to follow \\");

                case '#':
                    finish_comment();
                    token.file_offset = position;
                    break;

                case '@':
                    if (match_identifier("as"))     return make_token(keyword("@as"));
                    if (match_identifier("typeOf")) return make_token(keyword("@typeOf"));
                    finish_identifier();
                    report_error(token.file_offset, position - 1, "invalid intrinsic");

                case '"':
                    finish_string();
                    return make_token(TokenTypes::String);

                case '\'':
                    finish_rawstring();
                    return make_token(TokenTypes::String);

                case 'a':
                    if (match_identifier("nd")) return make_token(keyword("and"));
                    goto identifier;
                case 'b':
                    if (match_identifier("reak")) return make_token(keyword("break"));
                    goto identifier;
                case 'c':
                    if (match_identifier("ontinue")) return make_token(keyword("continue"));
                    goto identifier;
                case 'd':
                    if (match_identifier("efer")) return make_token(keyword("defer"));
                    goto identifier;
                case 'e':
                    if (match_identifier("lse")) return make_token(keyword("else"));
                    if (match_identifier("num")) return make_token(keyword("enum"));
                    goto identifier;
                case 'f':
                    if (match_identifier("n"))    return make_token(keyword("fn"));
                    if (match_identifier("or"))   return make_token(keyword("for"));
                    if (match_identifier("alse")) return make_token(keyword("false"));
                    goto identifier;
                case 'g':
                case 'h':
                    goto identifier;
                case 'i':
                    if (match_identifier("f")) return make_token(keyword("if"));
                    goto identifier;
                case 'j':
                case 'k':
                case 'l':
                    goto identifier;
                case 'm':
                    if (match_identifier("atch")) return make_token(keyword("match"));
                    goto identifier;
                case 'n':
                    if (match_identifier("ull")) return make_token(keyword("null"));
                    goto identifier;
                case 'o':
                    if (match_identifier("r")) return make_token(keyword("or"));
                    goto identifier;
                case 'p':
                case 'q':
                    goto identifier;
                case 'r':
                    if (match_identifier("eturn")) return make_token(keyword("return"));
                    goto identifier;
                case 's':
                    if (match_identifier("lse"))   return make_token(keyword("else"));
                    if (match_identifier("truct")) return make_token(keyword("struct"));
                    goto identifier;
                case 't':
                    if (match_identifier("rue")) return make_token(keyword("true"));
                    goto identifier;
                case 'u':
                    if (match_identifier("ndefined")) return make_token(keyword("undefined"));
                    goto identifier;
                case 'v':
                    goto identifier;
                case 'w':
                    if (match_identifier("hile")) return make_token(keyword("while"));
                    goto identifier;
                case 'x':
                case 'y':
                case 'z':
                    goto identifier;

                case 'A' ... 'Z':
                case '_':
                case 128 ... 255:
                    goto identifier;

                default:
                    report_error(position - 1, position - 1, "unknown character: [{}]", ascii_debug_string(c));

                identifier:
                    finish_identifier();
                    return make_token(TokenTypes::Identifier);
            }
        }

        std::unreachable();
    }
    catch (StopScanFlag) {
        return InvalidToken;
    }

    bool init(ErrorList& errors, const char* source, usz source_len)
    {
        *this = {
            .errors = &errors,
            .source = source,
            .source_len = source_len,
        };

        auto res = simdutf::validate_utf8_with_errors(std::string_view(source, source_len));
        if (res.is_err()) {
            errors.errors.emplace_back(Error {
                .line = 0,
                .head_file_offset = 0,
                .tail_file_offset = 0,
                .message = std::format("source not valid UTF-8: {}", simdutf_err_to_string(res.error)),
            });
            return false;
        }

        return true;
    }
};

NAMESPACE_END
