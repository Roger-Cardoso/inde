#pragma once
#include "inde/project/writing.hpp"
namespace inde::persistence {
std::string encode_inline_commands(const std::vector<project::DocumentInlineCommand> &);
std::vector<project::DocumentInlineCommand> decode_inline_commands(const std::string &);
}
