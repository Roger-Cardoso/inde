#pragma once
#include "inde/project/text_semantics.hpp"
namespace inde::persistence {
std::string encode_semantics(const project::DocumentSemantics &);
project::DocumentSemantics decode_semantics(const std::string &);
std::string encode_semantic_type(const project::AnnotationTypeDefinition &);
project::AnnotationTypeDefinition decode_semantic_type(const std::string &);
} // namespace inde::persistence
