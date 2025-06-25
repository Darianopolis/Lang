#include "lang.hpp"

#include "unicode.hpp"

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
    case '&': case '|': case '^': case '~': case '?': case '/'

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
        || c == '_'
        || c == '$';
}

bool is_valid_identifier_continue_element(u8 c)
{
    return is_valid_identifier_start_element(c) || is_decimal_digit(c);
}

NAMESPACE_END

struct ScanState
{
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

            while (!is_at_end()) {
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
        auto raw_start = position;

        static constexpr auto is_valid_punc = [](std::string_view str)
        {
            for (auto& punc : PunctuationSequences) {
                if (punc == str) return true;
            }
            return false;
        };

        while (advance()) {
            // Try to consume the largest valid punctuation sequence
            if (!is_valid_punc(source.substr(raw_start, position + 1 - raw_start))) {
                break;
            }
        }

        auto sub = source.substr(raw_start, position - raw_start);

        token = {
            .type = TokenType::Punctuation,
            .raw = sub,
            .punctuation = { .value = sub },
        };
    }

    void read_number()
    {
        bool (*digit_test_)(u8) = is_decimal_digit;
        u8 exponent_separator = 'e';

        u32 base = 10;
        auto num_start = position;
        auto raw_start = position;

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
            .type = TokenType::Number,
            .raw = source.substr(raw_start, end - raw_start),
            .number = {
                .base = base,
                .raw = source.substr(num_start, end - num_start),
            },
        };
    }

    void read_identifier()
    {
        auto raw_start = position;

        bool is_at = peek() == '@';
        if (is_at) advance();

        auto start = position;

        for (;;) {
            advance();
            u8 c = peek();
            if (!is_valid_identifier_continue_element(c)) {
                auto end = position;

                auto raw = source.substr(raw_start, end - raw_start);

                for (auto& k : Keywords) {
                    if (k == raw) {
                        token = {
                            .type = TokenType::Keyword,
                            .raw = raw,
                            .keyword = {
                                .value = source.substr(start, end - start),
                            },
                        };

                        return;
                    }
                }

                token = {
                    .type = TokenType::Identifier,
                    .raw = raw,
                    .identifier = {
                        .is_at = is_at,
                        .value = source.substr(start, end - start),
                    },
                };

                return;
            }
        }
    }

    void read_string()
    {
        auto raw_start = position;

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
            .type = TokenType::String,
            .raw = source.substr(raw_start, position - raw_start),
            .string = {
                .is_raw = false,
                .value = source.substr(start, end - start),
            },
        };
    }

    std::string scratch_raw_string_init_sequence;
    void read_rawstring()
    {
        auto raw_start = position;

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

        auto str_end = end_seq_pos;

        position = end_seq_pos + scratch_raw_string_init_sequence.size();

        token = {
            .type = TokenType::String,
            .raw = source.substr(raw_start, position - raw_start),
            .string = {
                .is_raw = true,
                .value = source.substr(str_start, str_end - str_start),
            },
        };
    }

    bool scan()
    {
        token.type = TokenType::Invalid;

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
                    else raise_error("illegal source character: {} - ({})", int(c), char(c));
                    break;
                }
            }

            if (token.type != TokenType::Invalid) {
                return true;
            }
        }

        return false;
    }
};

TokenList lang::scan(std::string_view source)
{
    auto res = simdutf::validate_utf8_with_errors(source);
    if (res.is_err()) raise_error("source not valid UTF-8: {}", simdutf_err_to_string(res.error));

    ScanState scanner{
        .source = source,
        .position = 0,
    };

    TokenList list;

    std::vector<u32> beginning_tokens;

    while (scanner.scan()) {
        auto i = list.tokens.size();
        if (scanner.token.type == TokenType::Punctuation) {
            auto b = scanner.token.punctuation.value;
            if (b == "{" || b == "[" || b == "(") {
                beginning_tokens.emplace_back(i);
            }
            else if (b == "}" || b == "]" || b == ")") {
                if (beginning_tokens.empty()) {
                    raise_error("unexpected closing '{}' - no matching open", b);
                }
                auto& begin_token = list.tokens[beginning_tokens.back()];
                auto a = begin_token.punctuation.value;
                u8 expected = '\0';
                switch (b[0]) {
                    case '}': expected = '{'; break;
                    case ']': expected = '['; break;
                    case ')': expected = '('; break;
                }
                if (a[0] != expected) {
                    raise_error("mismatched pair - found closing '{}' but last open was '{}'", b, a);
                }

                begin_token.punctuation.matching = i;
                scanner.token.punctuation.matching = beginning_tokens.back();

                beginning_tokens.pop_back();
            }
        }
        list.tokens.emplace_back(scanner.token);
    }

    if (!beginning_tokens.empty()) {
        raise_error("unclosed '{}'", list.tokens[beginning_tokens.back()].punctuation.value);
    }

    return list;
}
