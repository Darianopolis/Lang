#include "tokens.hpp"

NAMESPACE_BEGIN(lang)

void ErrorList::print_errors(Constants& constants, struct TokenList& token_list, std::string_view source)
{
    // log_debug("token_list.lines.size = {}", token_list.lines.size());
    // for (u32 l = 0; l < token_list.lines.size(); ++l) {
    //     auto line = token_list.lines[l];

    //     auto end_of_line = source.find_first_of('\n', line.file_offset);
    //     if (end_of_line == std::string::npos) end_of_line = source.size();
    //     auto line_str = source.substr(line.file_offset, end_of_line - line.file_offset);

    //     log_debug("[{:5}] {}", l, line_str);
    // }

    // TODO: Convert tabs to spaces and adjust visual columns accordingly

    for (auto& error : errors) {
        // log_warn("error.line = {}", error.line);
        // log_warn("error.file_offset = {}", error.file_offset);

        auto line_info = token_list.lines[error.line];

        // log_warn("line_info.file_offset = {}", line_info.file_offset);
        // log_warn("line_info.first_token_file_offset = {}", line_info.first_token_file_offset);
        // log_warn("line_info.first_token = {}", line_info.first_token);
        // log_warn("column = {}", error.file_offset - line_info.file_offset);

        // auto end_of_line = source.find_first_of('\n', line_info.file_offset);
        // if (end_of_line == std::string::npos) end_of_line = source.size();
        // auto line_str = source.substr(line_info.file_offset, end_of_line - line_info.file_offset);
        // log_error("{:5} | {}", error.line, line_str);

        // std::string indent(error.file_offset - line_info.file_offset, ' ');
        // log_error("      | {}" text_ansi_color(93, "^~") , indent);

        // log_error("      | {}", error.message);

        auto start_of_line = source.substr(0, line_info.first_token_file_offset).find_last_of('\n');
        if (start_of_line == std::string::npos) {
            // log_warn("  found no earlier newline");
            start_of_line = 0;
        } else {
            // log_warn("  found earlier newline");
            start_of_line++;
        }

        // log_warn("start_of_line = {}", start_of_line);

        auto end_of_line = source.find_first_of('\n', line_info.first_token_file_offset);
        if (end_of_line == std::string::npos) end_of_line = source.size();
        auto line_str = source.substr(start_of_line, end_of_line - start_of_line);
        log_error("{:5} | {}", error.line, line_str);

        std::string indent(error.file_offset - start_of_line, ' ');
        log_error("      | {}" text_ansi_color(93, "^~") , indent);

        log_error("      | {}", error.message);
    }
}

NAMESPACE_END
