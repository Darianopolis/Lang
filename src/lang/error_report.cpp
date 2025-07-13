#include "lang.hpp"

#include <unistd.h>
#include <termios.h>

NAMESPACE_BEGIN(lang)

namespace {

    termios set_input_mode()
    {
        struct termios new_termios;
        struct termios original_termios;
        tcgetattr(STDIN_FILENO, &original_termios);
        new_termios = original_termios;

        new_termios.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

        return original_termios;
    }

    void restore_input_mode(const termios* original_termios)
    {
        tcsetattr(STDIN_FILENO, TCSANOW, original_termios);
    }

    std::pair<int, int> get_cursor_position()
    {
        std::cout << std::flush;

        std::string response;
        {
            auto original_termios = set_input_mode();
            defer { restore_input_mode(&original_termios); };

            std::print("\033[6n");
            fflush(stdout);

            // Read the response: ESC [ row ; col R
            char ch;
            while (read(STDIN_FILENO, &ch, 1) == 1) {
                response += ch;
                if (ch == 'R') break;
            }
        }

        int row, col;
        if (sscanf(response.c_str(), "\033[%d;%dR", &row, &col) == 2) {
            return {row, col};
        } else {
            raise_error("failed to parse cursor position request response");
        }
    }
}

void Error::print(std::string_view source)
{
    auto line_str = get_containing_line(source, head_file_offset);

    log_error("{}", message);

    usz head_str_index = head_file_offset - (line_str.data() - source.data());
    usz tail_str_index = tail_file_offset - (line_str.data() - source.data());

    usz first_str_index = std::min(head_str_index, tail_str_index);
    usz last_str_index = std::max(head_str_index, tail_str_index);
    bool tail_first = tail_str_index < head_str_index;

    last_str_index = std::min(last_str_index, line_str.size());

    // print to beginning of caret
    std::print( "{:5} | {}", line, line_str.substr(0, first_str_index));
    auto[_, start] = get_cursor_position();

    // print to end of caret
    std::print("{}", line_str.substr(first_str_index, last_str_index - first_str_index));
    auto[_, end] = get_cursor_position();

    // print rest of line
    std::println("{}", line_str.substr(last_str_index));

    // print caret
    static constexpr auto prefix = "      | "sv;
    std::print(prefix);

    for (u32 i = prefix.size() + 1; i < start; ++i) {
        std::print(" ");
    }
    if (tail_first) {
        std::print(VT_color_begin(93));
        for (u32 i = start; i < end; ++i) {
            std::print("~");
        }
        std::print("^");
    } else {
        std::print(VT_color_begin(93) "^");
        for (u32 i = start; i < end; ++i) {
            std::print("~");
        }
    }
    std::println(VT_color_reset);

    std::println("      | ");
}

NAMESPACE_END
