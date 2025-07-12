#include "lang.hpp"

#include "strings.hpp"

#include <cmath>

NAMESPACE_USE(lang)

NAMESPACE_BEGIN()

// TODO: Carriage Return
#define LANG_WHITESPACE_CASES \
    case ' ': case '\n': case '\t'

#define LANG_DECIMAL_DIGIT_CASES \
    case '0': case '1': case '2': case '3': case '4': \
    case '5': case '6': case '7': case '8': case '9'

#define LANG_HEXADECIMAL_DIGIT_CASES \
    LANG_DECIMAL_DIGIT_CASES: \
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': \
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F'

bool is_binary_digit(u8 c)
{
    return c == '0' || c == '1';
}

bool is_octal_digit(u8 c)
{
    return c >= '0' && c <= '7';
}

bool is_decimal_digit(u8 c)
{
    return c >= '0' && c <= '9';
}

bool is_hexadecimal_digit(u8 c)
{
    switch (c) {
        LANG_HEXADECIMAL_DIGIT_CASES: return true;
        default:                      return false;
    }
}

std::optional<u8> maybe_hex_digit_to_int(u8 c)
{
    switch (c) {
        case '0'...'9': return c - '0';
        case 'a'...'f': return 10 + c - 'a';
        case 'A'...'F': return 10 + c - 'A';
        default:        return std::nullopt;
    }
}

#define LANG_PUNCTUATION_CASES \
    case '(': case ')': case '[': case ']': case '{': case '}': \
    case ',': case '.': case '+': case ':': case ';': case '*': \
    case '!': case '=': case '>': case '<': case '-': case '%': \
    case '&': case '|': case '^': case '~': case '?': case '/': \
    case '$': case '\\'

bool is_ascii_alpha(u8 c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_ascii_alphanumeric(u8 c)
{
    return is_ascii_alpha(c) || is_decimal_digit(c);
}

bool is_valid_identifier_start_element(u8 c)
{
    // NOTE: All unicode values are fair game
    //       all other ASCII values are reserved for future compatibility
    //       Identifier rules should be enforced by linters based on user policy

    return c >= 128
        || is_ascii_alpha(c)
        || c == '_';
}

bool is_valid_identifier_continue_element(u8 c)
{
    return is_valid_identifier_start_element(c) || is_decimal_digit(c);
}

NAMESPACE_END

struct StopScanFlag {};

struct Scanner::Impl : Scanner
{
    ErrorList* errors;
    std::string_view source;
    usz position;

    std::string string_scratch;
    Token token;
    u32 line = 0;

// -----------------------------------------------------------------------------

    void new_line(u32 pos)
    {
        (void)pos;

        line++;
    }

    void begin_token()
    {
        token = {
            .line = line,
            .file_offset = u32(position),
        };
    }

    template<typename ...Args>
    void report_error_nothrow(u32 pos, std::format_string<Args...> fmt, Args&& ... args)
    {
        auto message = std::format(fmt, std::forward<Args>(args)...);
        errors->errors.emplace_back(Error {
            // TODO: Are we always still on the same line?
            .line = line,
            .file_offset = pos,
            .message = message,
        });
    }

    template<typename ...Args>
    void report_error(u32 pos, std::format_string<Args...> fmt, Args&& ... args)
    {
        report_error_nothrow(pos, fmt, std::forward<Args>(args)...);
        throw StopScanFlag{};
    }

// -----------------------------------------------------------------------------

    LANG_FORCEINLINE
    bool is_at_end()
    {
        return position >= source.size();
    }

    LANG_FORCEINLINE
    u8 advance()
    {
        return is_at_end() ? '\0' : source[position++];
    }

    LANG_FORCEINLINE
    u8 peek()
    {
        return is_at_end() ? '\0' : source[position];
    }

    LANG_FORCEINLINE
    u8 peek_next()
    {
        auto next = position + 1;
        return (next >= source.size() ? '\0' : source[next]);
    }

// -----------------------------------------------------------------------------

    void skip_comment()
    {
        auto comment_start = position;

        // Consume #
        advance();

        u32 count = 0;
        while (peek() == '[') {
            advance();
            count++;
        }

        if (count) {
            // multi-line

            for (;;) {
                if (is_at_end()) {
                    report_error(comment_start, "reached end-of-file parsing block comment");
                }
                auto c = peek();
                if (c == '\n') new_line(position + 1);
                if (c == '#' && peek_next() == '[') {
                    skip_comment();
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

    void read_punctuation()
    {
        begin_token();

        auto type = TokenTypes::Invalid;

        switch (advance()) {
            case '^': type = punc("^"); break;
            case '(': type = punc("("); break;
            case ')': type = punc(")"); break;
            case '{': type = punc("{"); break;
            case '}': type = punc("}"); break;
            case '[': type = punc("["); break;
            case ']': type = punc("]"); break;
            case ':': type = punc(":"); break;
            case ';': type = punc(";"); break;
            case ',': type = punc(","); break;
            case '?': type = punc("?"); break;
                break;
            case '+':
                switch (peek()) {
                    case '=': position++; type = punc("+="); break;
                    case '+': position++; type = punc("++"); break;
                    default:              type = punc("+");  break;
                }
                break;
            case '-':
                switch (peek()) {
                    case '=': position++; type = punc("-="); break;
                    case '-': position++; type = punc("--"); break;
                    case '>': position++; type = punc("->"); break;
                    default:              type = punc("-");  break;
                }
                break;
            case '*':
                switch (peek()) {
                    case '=': position++; type = punc("*="); break;
                    case '*':
                        advance();
                        switch (peek()) {
                            case '=': position++; type = punc("**="); break;
                            default:              type = punc("**");  break;
                        }
                        break;
                    default: type = punc("*"); break;
                }
                break;
            case '/': if (peek() == '=') { position++; type = punc("/="); } else { type = punc("/"); } break;
            case '%': if (peek() == '=') { position++; type = punc("%="); } else { type = punc("%"); } break;
            case '!': if (peek() == '=') { position++; type = punc("!="); } else { type = punc("!"); } break;
            case '=': if (peek() == '=') { position++; type = punc("=="); } else { type = punc("="); } break;
            case '&': if (peek() == '=') { position++; type = punc("&="); } else { type = punc("&"); } break;
            case '|': if (peek() == '=') { position++; type = punc("|="); } else { type = punc("|"); } break;
            case '<':
                switch (peek()) {
                    case '=':         position++; type = punc("<="); break;
                    case '<':
                        advance();
                        switch (peek()) {
                            case '=': position++; type = punc("<<="); break;
                            default:              type = punc("<<");  break;
                        }
                        break;
                    default:                      type = punc("<");  break;
                }
                break;
            case '>':
                switch (peek()) {
                    case '=':         position++; type = punc(">="); break;
                    case '>':
                        advance();
                        switch (peek()) {
                            case '=': position++; type = punc(">>="); break;
                            default:              type = punc(">>");  break;
                        }
                        break;
                    default:                      type = punc(">");  break;
                }
                break;
            case '~':
                switch (peek()) {
                    case '|':
                        advance();
                        switch (peek()) {
                            case '=': position++; type = punc("~|="); break;
                            default:              type = punc("~|");  break;
                        }
                        break;
                    default:                      type = punc("~");   break;
                }
                break;
            case '$':
                if (advance() != '{') report_error(position - 1, "expected '{{' following '$'");
                type = punc("${");
                break;
            case '\\':
                switch (advance()) {
                    case '{':; type = punc("\\{"); break;
                    case 'i':
                        if (advance() != 'd') report_error(position - 2, "expected '{{' or 'id{{' following '\\'");
                        if (advance() != '{') report_error(position - 3, "expected '{{' or 'id{{' following '\\'");
                        type = punc("\\id{");
                        break;
                    default:
                        report_error(position - 1, "expected '{{' or 'id{{' following '\\'");
                }
                break;
            case '.':
                switch (peek()) {
                    case '.':
                        advance();
                        switch (peek()) {
                            case '=': position++; type = punc("..="); break;
                            default:              type = punc("..");  break;
                        }
                        break;
                    default:                      type = punc(".");   break;
                }
                break;
        }

        token.type = type;
    }

    void read_number()
    {
        begin_token();

        auto raw_start = position;

        bool (*digit_test_)(u8) = is_decimal_digit;
        u8 exponent_separator = 'e';

        u32 base = 10;
        auto num_start = position;

        // Identify hexadecimal or binary number
        if (peek() == '0') {
            advance();
            switch (peek()) {
                case 'x':
                    digit_test_ = is_hexadecimal_digit;
                    exponent_separator = 'p';
                    base = 16;
                    num_start += 2;
                    advance();
                    break;
                case 'b':
                    digit_test_ = is_binary_digit;
                    base = 2;
                    num_start += 2;
                    advance();
                    break;
                case 'o':
                    digit_test_ = is_octal_digit;
                    base = 8;
                    num_start += 2;
                    advance();
                    break;
            }
        }

        auto digit_test = [&](u8 c, bool(*digit_test_override)(u8) = nullptr) -> bool {
            if (!digit_test_override) digit_test_override = digit_test_;
            if (digit_test_override(c)) return true;
            if (c == '\'') {
                if (!digit_test_override(peek_next())) {
                    report_error(position, "digit separator cannot appear at end of digit sequence");
                }
                advance();
                return true;
            }
            return false;
        };

        // Consume integer part
        while (digit_test(peek())) advance();

        // Look for fractional part
        if (peek() == '.' && digit_test(peek_next())) {
            advance();

            if (base == 2 || base == 8) report_error(position, "Binary and octal numbers do not support fractional components");

            // Consume the fractional part
            while (digit_test(peek())) advance();

            // Look for exponent
            if (peek() == exponent_separator) {
                advance();

                // Consume [ + ] or [ - ]
                if (peek() == '+' || peek() == '-') advance();

                // Consume the exponent (exponent always considered as decimal)
                while (is_decimal_digit(peek())) advance();
            }
        }

        switch (peek()) {
            case 0:
            LANG_PUNCTUATION_CASES:
            LANG_WHITESPACE_CASES:
            case '#':
            case '"': // This will result in syntax errors but we postpone this to parsing for better context
                break;
            default:
                report_error(position, "number literal must be followed by operator, comment, or whitespace; got '{}'", char(peek()));
        }

        auto end = position;

        token.type = TokenTypes::Number;
        token.number = {
            .value = source.substr(num_start, end - num_start),
            .base = u8(base),
        };
    }

    void read_identifier()
    {
        begin_token();

        bool is_at = peek() == '@';

        auto start = position;
        if (is_at) start++;

        for (;;) {
            advance();
            u8 c = peek();
            if (!is_valid_identifier_continue_element(c)) {
                break;
            }
        }

        auto end = position;

        auto ident = source.substr(start, end - start);

        TokenType type = TokenTypes::Identifier;

        if (!ident.empty())  {
            switch (ident[0]) {
                case 'a':
                    if      (ident.substr(1) == "nd") type = keyword("and");
                    else if (ident.substr(1) == "s")  type = keyword("@as");
                    break;
                case 'b': if (ident.substr(1) == "reak")    type = keyword("break");    break;
                case 'c': if (ident.substr(1) == "ontinue") type = keyword("continue"); break;
                case 'd': if (ident.substr(1) == "efer")    type = keyword("defer");    break;
                case 'e':
                    if      (ident.substr(1) == "lse") type = keyword("else");
                    else if (ident.substr(1) == "num") type = keyword("enum");
                    break;
                case 'f':
                    if      (ident.substr(1) == "n")    type = keyword("fn");
                    else if (ident.substr(1) == "or")   type = keyword("for");
                    else if (ident.substr(1) == "alse") type = keyword("false");
                    break;
                // gh
                case 'i': if (ident.substr(1) == "f") type = keyword("if"); break;
                // jkl
                case 'm': if (ident.substr(1) == "atch") type = keyword("match"); break;
                case 'n': if (ident.substr(1) == "ull")  type = keyword("null");  break;
                case 'o': if (ident.substr(1) == "r")    type = keyword("or");    break;
                // pq
                case 'r': if (ident.substr(1) == "eturn") type = keyword("return"); break;
                case 's':
                    if      (ident.substr(1) == "lse")   type = keyword("else");
                    else if (ident.substr(1) == "truct") type = keyword("struct");
                    break;
                case 't':
                    if      (ident.substr(1) == "rue")   type = keyword("true");
                    else if (ident.substr(1) == "ypeOf") type = keyword("@typeOf");
                    break;
                case 'u': if (ident.substr(1) == "ndefined") type = keyword("undefined"); break;
                // v
                case 'w': if (ident.substr(1) == "hile") type = keyword("while"); break;
                // xyz
            }
        }

        if (type == TokenTypes::Identifier) {
            if (is_at) {
                report_error(start, "unknown intrinsic: {}", ident);
            } else {
                token.type = TokenTypes::Identifier;
                token.identifier = {
                    .value = ident,
                };
            }
        } else {
            token.type = type;
        }
    }

    void read_string()
    {
        begin_token();

        // Consume "
        advance();
        auto start = position;

        bool any_escapes = false;
        string_scratch.clear();

        for (;;) {
            auto c = advance();
            if (!c) report_error(start - 1, "eof while parsing string");
            if (c == '"') break;
            if (c == '\n') new_line(position);

            if (c == '\\') {
                any_escapes = true;

                switch (advance()) {
                    case '"': string_scratch += '"'; break;
                    case '0': string_scratch += '\0'; break;
                    case 'a': string_scratch += '\a'; break; // bell
                    case 'b': string_scratch += '\b'; break; // backspace
                    case 'e': string_scratch += '\e'; break; // escape
                    case 'f': string_scratch += '\f'; break; // form feed
                    case 'n': string_scratch += '\n'; break; // line feed
                    case 'r': string_scratch += "\r"; break; // carriage return
                    case 't': string_scratch += "\t"; break; // horizontal tab
                    case 'v': string_scratch += "\v"; break; // vertical tab
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

                        string_scratch.append(utf8, count);
                        break;
                    }
                    default:
                        report_error(position - 1, "illegal escape character '{}'", source[position - 1]);
                }
            } else {
                string_scratch += c;
            }
        }

        auto end = position - 1;

        token.type = TokenTypes::String;
        if (any_escapes) {
            token.string = {
                .value = string_scratch,
                .is_temporary = true,
            };
        } else {
            token.string = {
                .value = source.substr(start, end - start),
                .is_temporary = false,
            };
        }
    }

    void read_rawstring()
    {
        begin_token();

        [[maybe_unused]] auto raw_start = position;

        // Consume '
        advance();

        // Find end of variable terminator sequence
        auto term_seq_first = position;
        while (peek() != '"') {
            if (peek() == '\n') report_error(raw_start, "illegal newline while parsing rawstring terminator sequence");
            advance();
            if (is_at_end()) report_error(raw_start, "eof while parsing rawstring terminator sequence");
        }
        advance();

        auto term_seq_user = source.substr(term_seq_first, (position - 1) - term_seq_first);

        auto str_start = position;

        string_scratch.resize(term_seq_user.size() + 2);
        std::format_to(string_scratch.data(), "\"{}'", term_seq_user);

        auto end_seq_pos = source.find(string_scratch, position);
        if (end_seq_pos == std::string_view::npos) {
            report_error(raw_start, "eof while parsing rawstring");
        }

        [[maybe_unused]] auto str_end = end_seq_pos;

        for (usz p = str_start; p < str_end; ++p) {
            if (source[p] == '\n') new_line(p + 1);
        }

        position = end_seq_pos + string_scratch.size();

        token.type = TokenTypes::String;
        token.string = {
            .value = source.substr(str_start, str_end - str_start),
            .is_temporary = false,
        };
    }

    Token next() try
    {
        for (;;)
        {
            u8 c = peek();
            switch (c) {
                case '\0':                                      return EofToken;
                case '\n':                new_line(++position); break;
                case ' ': case '\t':      position++;           break;
                LANG_DECIMAL_DIGIT_CASES: read_number();        return token;
                LANG_PUNCTUATION_CASES:   read_punctuation();   return token;
                case '#':                 skip_comment();       break;
                case '@':                 read_identifier();    return token;
                case '"':                 read_string();        return token;
                case '\'':                read_rawstring();     return token;
                default: {
                    if (is_valid_identifier_start_element(c)) {
                        read_identifier();
                        return token;
                    } else {
                        report_error(position, "illegal source character: {} - ({})", u32(c), char(c));
                    }
                    break;
                }
            }
        }
    }
    catch (StopScanFlag) {
        return InvalidToken;
    }
};

Scanner* Scanner::create()
{
    return new Scanner::Impl{};
}

void Scanner::destroy(Scanner* _self)
{
    delete get_impl(_self);
}

bool Scanner::init(ErrorList& errors, std::string_view source)
{
    decl_self(this);

    *self = {
        .errors = &errors,
        .source = source,
    };

    auto res = simdutf::validate_utf8_with_errors(source);
    if (res.is_err()) {
        errors.errors.emplace_back(Error {
            .line = 0,
            .file_offset = 0,
            .message = std::format("source not valid UTF-8: {}", simdutf_err_to_string(res.error)),
        });
        return false;
    }

    return true;
}

Token Scanner::next()
{
    return get_impl(this)->next();
}
