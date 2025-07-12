#include "lang.hpp"

NAMESPACE_BEGIN(lang)

void Error::print(std::string_view source)
{
    auto line_str = get_containing_line(source, file_offset);

    log_error("{}", message);

    auto indent_amount = file_offset - (line_str.data() - source.data());
    std::print("\n      |" VT_move_prev_line);
    std::print( "{:5} | {}" VT_save_cursor "{}", line, line_str.substr(0, indent_amount), line_str.substr(indent_amount));
    std::println(VT_restore_cursor VT_move_down VT_color(93, "^~"));
    std::println("      | ");
}

NAMESPACE_END
