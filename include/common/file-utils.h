#pragma once

#include <vector>
#include <string>
#include <optional>

std::optional<std::vector<char>> read_file(std::string_view file_name, bool exit_on_error = true);
void write_file(std::string_view name, std::string_view code);
