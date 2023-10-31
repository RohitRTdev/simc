#pragma once

#include <vector>
#include <string>

std::vector<char> read_file(std::string_view file_name);
void write_file(std::string_view name, std::string_view code);
