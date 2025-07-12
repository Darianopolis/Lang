#pragma once

#include "core.hpp"

#include <simdutf.h>

NAMESPACE_BEGIN(lang)

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

inline
std::string_view get_containing_line(std::string_view str, usz index)
{
    auto start_of_line = str.substr(0, index).find_last_of('\n');
    if (start_of_line == std::string::npos) {
        start_of_line = 0;
    } else {
        start_of_line++;
    }

    auto end_of_line = str.find_first_of('\n', index);
    if (end_of_line == std::string::npos) end_of_line = str.size();

    return str.substr(start_of_line, end_of_line - start_of_line);
}

NAMESPACE_END
