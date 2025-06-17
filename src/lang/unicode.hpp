#include "core.hpp"

#include <simdutf.h>

#include <vector>

LANG_NAMESPACE_BEGIN

inline
const char* simdutf_err_to_string(simdutf::error_code code)
{
    switch (code) {
        case simdutf::error_code::SUCCESS:
            return "success";
        case simdutf::error_code::HEADER_BITS:
            // Any byte must have fewer than 5 header bits.
            return "too many header bits";
        case simdutf::error_code::TOO_SHORT:
            // The leading byte must be followed by N-1 continuation bytes,
            // where N is the UTF-8 character length This is also the error
            // when the input is truncated.
            return "too short";
        case simdutf::error_code::TOO_LONG:
            // We either have too many consecutive continuation bytes or the
            // string starts with a continuation byte.
            return "too long";
        case simdutf::error_code::OVERLONG:
            // The decoded character must be above U+7F for two-byte characters,
            // U+7FF for three-byte characters, and U+FFFF for four-byte
            // characters.
            return "overlong";
        case simdutf::error_code::TOO_LARGE:
            // The decoded character must be less than or equal to
            // U+10FFFF,less than or equal than U+7F for ASCII OR less than
            // equal than U+FF for Latin1
            return "too large";
        case simdutf::error_code::SURROGATE:
            // The decoded character must be not be in U+D800...DFFF (UTF-8 or
            // UTF-32) OR a high surrogate must be followed by a low surrogate
            // and a low surrogate must be preceded by a high surrogate
            // (UTF-16) OR there must be no surrogate at all (Latin1)
            return "surrogate";
        case simdutf::error_code::INVALID_BASE64_CHARACTER:
            // Found a character that cannot be part of a valid
            // base64 string. This may include a misplaced
            // padding character ('=').
            return "invalid base64 character";
        case simdutf::error_code::BASE64_INPUT_REMAINDER:
            // The base64 input terminates with a single
            // character, excluding padding (=).
            return "base64 input remainder";
        case simdutf::error_code::BASE64_EXTRA_BITS:
            // The base64 input terminates with non-zero
            // padding bits.
            return "base64 extra bits";
        case simdutf::error_code::OUTPUT_BUFFER_TOO_SMALL:
            // The provided buffer is too small.
            return "output buffer too small";
        case simdutf::error_code::OTHER:
            // Not related to validation/transcoding.
            return "other";
    }

    std::unreachable();
}

#if 0
struct Utf8IteratorEntry
{
    c32 value = 0;
    u64 loc = {};

    operator bool() const noexcept { return value; };
};

struct Utf8Iterator
{
    std::string_view source;
    u64 position = 0;

    Utf8Iterator(std::string_view source, std::vector<c32>& buf)
        : source(source)
    {}
    Utf8IteratorEntry next()
    {
        constexpr static auto fetch_add = [](auto& v, auto s) {
            auto r = v;
            v += s;
            return r;
        };

        constexpr static auto get_trailing = [](u8 value) {
            if ((value & 0b1100'0000) != 0b1000'0000) {
                raise_error("expected trailing byte");
            }
            return value & 0b0011'1111;
        };

        if (position >= source.size()) {
            // eof
            return {0};
        }

        u8 leading = source[position];

        if (leading < 0b1000'0000) {
            // 1 byte
            return {c32(leading), fetch_add(position, 1)};
        }

        if (leading < 0b1100'0000) {
            raise_error("unexpected trailing byte");
        }

        if (leading < 0b1110'0000) {
            // 2 byte

            if (position + 1 >= source.size()) raise_error("incomplete codepoint");
            u32 v0 = leading & 0b0001'1111;
            u32 v1 = get_trailing(source[position + 1]);
            auto c = v1 | (v0 << 6);
            if (c < (c << 7)) raise_error("overlong sequence");
            return {c, fetch_add(position, 2)};
        }

        if (leading < 0b1111'0000) {
            // 3 byte

            if (position + 2 >= source.size()) raise_error("incomplete codepoint");
            u32 v0 = leading & 0b0000'1111;
            u32 v1 = get_trailing(source[position + 1]);
            u32 v2 = get_trailing(source[position + 2]);
            auto c = v2 | (v1 << 6) | (v0 << 12);
            if (c < (1 << 11)) raise_error("overlong sequence");
            return {c, fetch_add(position, 3)};
        }

        if (leading < 0b1111'1000) {
            // 4 byte

            if (position + 3 >= source.size()) raise_error("incomplete codepoint");
            u32 v0 = leading & 0b0000'0111;
            u32 v1 = get_trailing(source[position + 1]);
            u32 v2 = get_trailing(source[position + 2]);
            u32 v3 = get_trailing(source[position + 3]);
            auto c = v3 | (v2 << 6) | (v1 << 12) | (v0 << 18);
            if (c < (1 << 16)) raise_error("overlong sequence");
            return {c, fetch_add(position, 4)};
        }
        raise_error("too many header bits");
    }
};
#endif

LANG_NAMESPACE_END
