#include "lang.hpp"

#include "unicode.hpp"

#include <cmath>

NAMESPACE_USE(lang)

NAMESPACE_BEGIN()

#define LANG_WHITESPACE_CASES \
    case ' ': case '\n': case '\r': case '\t'

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
    //       ASCII values are reserved for future compatibility
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

struct ScanState
{
    Constants* constants;
    std::string_view source;
    usz position;

    Token token;

    bool is_at_end()
    {
        return position >= source.size();
    }

    u8 advance()
    {
        return is_at_end() ? '\0' : source[position++];
    }

    u8 peek()
    {
        return is_at_end() ? '\0' : source[position];
    }

    u8 peek_next()
    {
        auto next = position + 1;
        return (next >= source.size() ? '\0' : source[next]);
    }

    void skip_comment()
    {
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
                    raise_error("reached end-of-file parsing block comment");
                }
                auto c = peek();
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
                if (advance() == '\n') return;
            }
        }

    }

    void read_punctuation()
    {
        auto type = TokenTypes::Invalid;

        switch (source[position++]) {
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
                switch (source[position]) {
                    case '=': position++; type = punc("+="); break;
                    case '+': position++; type = punc("++"); break;
                    default:              type = punc("+");  break;
                }
                break;
            case '-':
                switch (source[position]) {
                    case '=': position++; type = punc("-="); break;
                    case '-': position++; type = punc("--"); break;
                    case '>': position++; type = punc("->"); break;
                    default:              type = punc("-");  break;
                }
                break;
            case '*':
                switch (source[position]) {
                    case '=': position++; type = punc("*="); break;
                    case '*':
                        switch (source[++position]) {
                            case '=': position++; type = punc("**="); break;
                            default:              type = punc("**");  break;
                        }
                        break;
                    default: type = punc("*"); break;
                }
                break;
            case '/': if (source[position] == '=') { position++; type = punc("/="); } else { type = punc("/"); } break;
            case '%': if (source[position] == '=') { position++; type = punc("%="); } else { type = punc("%"); } break;
            case '!': if (source[position] == '=') { position++; type = punc("!="); } else { type = punc("!"); } break;
            case '=': if (source[position] == '=') { position++; type = punc("=="); } else { type = punc("="); } break;
            case '&': if (source[position] == '=') { position++; type = punc("&="); } else { type = punc("&"); } break;
            case '|': if (source[position] == '=') { position++; type = punc("|="); } else { type = punc("|"); } break;
            case '<':
                switch (source[position]) {
                    case '=':         position++; type = punc("<="); break;
                    case '<':
                        switch (source[++position]) {
                            case '=': position++; type = punc("<<="); break;
                            default:              type = punc("<<");  break;
                        }
                        break;
                    default:                      type = punc("<");  break;
                }
                break;
            case '>':
                switch (source[position]) {
                    case '=':         position++; type = punc(">="); break;
                    case '>':
                        switch (source[++position]) {
                            case '=': position++; type = punc(">="); break;
                            default:              type = punc(">");  break;
                        }
                        break;
                    default:                      type = punc(">");  break;
                }
                break;
            case '~':
                switch (source[position]) {
                    case '|':
                        switch (source[++position]) {
                            case '=': position++; type = punc("~|="); break;
                            default:              type = punc("~|");  break;
                        }
                        break;
                    default:                      type = punc("~");   break;
                }
                break;
            case '$':
                if (source[position++] != '{') raise_error("expected '{{' following $");
                type = punc("${");
                break;
            case '\\':
                switch (source[position++]) {
                    case '{':; type = punc("\\{"); break;
                    case 'i':
                        if (source[position++] != 'd') raise_error("expected '{{' or 'id{{' following '\\'");
                        if (source[position++] != '{') raise_error("expected '{{' or 'id{{' following '\\'");
                        type = punc("\\id{");
                        break;
                    default:
                        raise_error("expected '{{' or 'id{{' following '\\'");
                }
                break;
            case '.':
                switch (source[position]) {
                    case '.':
                        switch (source[++position]) {
                            case '=': position++; type = punc("..="); break;
                            default:              type = punc("..");  break;
                        }
                        break;
                    default:                      type = punc(".");   break;
                }
                break;
        }

        token = {
            .type = type,
        };
    }

    void read_number()
    {
        auto raw_start = position;

        bool (*digit_test_)(u8) = is_decimal_digit;
        u8 exponent_separator = 'e';

        u32 base = 10;
        // auto num_start = position;

        // Identify hexadecimal or binary number
        if (peek() == '0') {
            advance();
            switch (peek()) {
                case 'x':
                    digit_test_ = is_hexadecimal_digit;
                    exponent_separator = 'p';
                    base = 16;
                    // num_start += 2;
                    advance();
                    break;
                case 'b':
                    digit_test_ = is_binary_digit;
                    base = 2;
                    // num_start += 2;
                    advance();
                    break;
                case 'o':
                    digit_test_ = is_octal_digit;
                    base = 8;
                    // num_start += 2;
                    advance();
                    break;
            }
        }

        auto digit_test = [&](u8 c, bool(*digit_test_override)(u8) = nullptr) -> bool {
            if (!digit_test_override) digit_test_override = digit_test_;
            if (digit_test_override(c)) return true;
            if (c == '\'') {
                if (!digit_test_override(peek_next())) {
                    raise_error("digit separator cannot appear at end of digit sequence");
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

            if (base == 2 || base == 8) raise_error("Binary and octal numbers do not support fractional components");

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
                raise_error("number literal must be followed by operator, comment, or whitespace; got {}", peek());
        }

        auto end = position;

        token = {
            .type = TokenTypes::Number,
            .value = constants->numbers.get_id(source.substr(raw_start, end - raw_start)),
            // .value = u32(raw_start),
        };
    }

    void read_identifier()
    {
        bool is_at = peek() == '@';
        if (is_at) advance();

        auto start = position;

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
                if      (ident.substr(1) == "or")   type = keyword("for");
                else if (ident.substr(1) == "alse") type = keyword("false");
                break;
            // gh
            case 'i': if (ident.substr(1) == "f") type = keyword("if"); break;
            // jkl
            case 'm': if (ident.substr(1) == "atch") type = keyword("match"); break;
            case 'n': if (ident.substr(1) == "ull")  type = keyword("null");  break;
            case 'o': if (ident.substr(1) == "r")    type = keyword("or");    break;
            // pq
            case 'r': if (ident.substr(1) == "eturn")    type = keyword("return"); break;
            case 's': if (ident.substr(1) == "lse")      type = keyword("else");   break;
            case 't':
                if      (ident.substr(1) == "rue")   type = keyword("true");
                else if (ident.substr(1) == "ypeOf") type = keyword("@typeOf");
                break;
            case 'u': if (ident.substr(1) == "ndefined") type = keyword("undefined"); break;
            // v
            case 'w': if (ident.substr(1) == "hile") type = keyword("while"); break;
            // xyz
        }

        if (type == TokenTypes::Identifier) {
            if (is_at) {
                raise_error("unknown intrinsic: {}", ident);
            } else {
                token = {
                    .type = TokenTypes::Identifier,
                    .value = constants->identifiers.get_id(ident),
                };
            }
        } else {
            token = {
                .type = type,
            };
        }

        return;
    }

    void read_string()
    {
        // Consume "
        advance();
        auto start = position;

        for (;;) {
            auto c = advance();
            if (!c) raise_error("eof while parsing string");
            if (c == '"') break;

            if (c == '\\') {
                if (peek() == '"') advance();
            }

        }

        auto end = position - 1;

        token = {
            .type = TokenTypes::String,
            .value = constants->strings.get_id(source.substr(start, end - start)),
            // .value = u32(start),
        };
    }

    std::string scratch_raw_string_init_sequence;
    void read_rawstring()
    {
        [[maybe_unused]] auto raw_start = position;

        // Consume '
        advance();

        // Find end of variable terminator sequence
        auto term_seq_first = position;
        while (advance() != '"') {
            if (is_at_end()) raise_error("eof while parsing rawstring terminator sequence");
        }

        auto term_seq_user = source.substr(term_seq_first, (position - 1) - term_seq_first);

        auto str_start = position;

        scratch_raw_string_init_sequence.resize(term_seq_user.size() + 2);
        std::format_to(scratch_raw_string_init_sequence.data(), "\"{}'", term_seq_user);

        auto end_seq_pos = source.find(scratch_raw_string_init_sequence, position);
        if (end_seq_pos == std::string_view::npos) {
            raise_error("eof while parsing rawstring");
        }

        [[maybe_unused]] auto str_end = end_seq_pos;

        position = end_seq_pos + scratch_raw_string_init_sequence.size();

        token = {
            .type = TokenTypes::String,
            .value = constants->strings.get_id(source.substr(str_start, str_end - str_start)),
            // .value = u32(raw_start),
        };
    }

    bool scan()
    {
        token.type = TokenTypes::Invalid;

        while (!is_at_end()) {
            u8 c = peek();
            switch (c) {
                LANG_WHITESPACE_CASES:    position++;         break;
                LANG_DECIMAL_DIGIT_CASES: read_number();      break;
                LANG_PUNCTUATION_CASES:   read_punctuation(); break;
                case '#':                 skip_comment();     break;
                case '@':                 read_identifier();  break;
                case '"':                 read_string();      break;
                case '\'':                read_rawstring();   break;
                default: {
                    if (is_valid_identifier_start_element(c)) read_identifier();
                    else raise_error("illegal source character: {} - ({})", u32(c), char(c));
                    break;
                }
            }

            if (token.type != TokenTypes::Invalid) {
                return true;
            }
        }

        return false;
    }
};

void lang::scan(TokenList& out, Constants& constants, std::string_view source)
{
    auto res = simdutf::validate_utf8_with_errors(source);
    if (res.is_err()) raise_error("source not valid UTF-8: {}", simdutf_err_to_string(res.error));

    out.constants = &constants;
    out.tokens.clear();

    ScanState scanner{
        .constants = &constants,
        .source = source,
        .position = 0,
    };

    std::vector<u32> beginning_tokens;

    while (scanner.scan()) {
        u32 offset = out.tokens.size();

        switch (scanner.token.type) {
            case punc("{"):
            case punc("["):
            case punc("("):
            case punc("${"):
            case punc("\\{"):
            case punc("\\id{"):
            {
                beginning_tokens.emplace_back(offset);
                break;
            }
            case punc("}"):
            case punc("]"):
            case punc(")"):
            {
                if (beginning_tokens.empty()) {
                    raise_error("unexpected closing bracket, no open brackets");
                }
                auto begin_token_type = TokenType(out.tokens[beginning_tokens.back()]);

                switch (scanner.token.type) {
                    case punc("}"):
                        if (begin_token_type != punc("{")
                                && begin_token_type != punc("\\{")
                                && begin_token_type != punc("\\id{"))
                            raise_error("mismatched pair - found unexpected closing '}}'");
                        break;
                    case punc("]"):
                        if (begin_token_type != punc("["))
                            raise_error("mismatched pair - found unexpected closing ']'");
                        break;
                    case punc(")"):
                        if (begin_token_type != punc("("))
                            raise_error("mismatched pair - found unexpected closing ')'");
                }

                // begin_token.value = offset;
                std::memcpy(out.tokens.data() + beginning_tokens.back() + 1, &offset, 4);
                scanner.token.value = beginning_tokens.back();

                beginning_tokens.pop_back();
            }
        }

        out.append(scanner.token);
    }

    if (!beginning_tokens.empty()) {
        raise_error("unclosed brackets");
    }
}
