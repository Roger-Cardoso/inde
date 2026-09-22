#include "inde/project/linguistic.hpp"

namespace inde::project {

std::string display_name(LinguisticEntityKind kind) {
  switch (kind) {
  case LinguisticEntityKind::Construction:
    return "Construção";
  case LinguisticEntityKind::Frame:
    return "Frame semântico";
  case LinguisticEntityKind::SyntacticFrame:
    return "Quadro sintático";
  case LinguisticEntityKind::MultiwordExpression:
    return "Expressão multipalavra";
  }
  return "Entidade linguística";
}

} // namespace inde::project
