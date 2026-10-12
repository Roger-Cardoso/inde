#pragma once
#include <string>
namespace inde::ui {
inline std::string continuity_kind_label(const std::string &value) {
  if (value == "order")
    return "ordem declarada";
  if (value == "cycle")
    return "ciclo de dependências";
  if (value == "chronology")
    return "cronologia";
  if (value == "presence")
    return "presença";
  if (value == "insufficient")
    return "informação insuficiente";
  if (value == "review")
    return "conferência necessária";
  if (value == "reconciliation")
    return "conferência entre texto e plano";
  if (value == "author-declared")
    return "declaração autoral";
  return "aviso";
}
inline std::string realization_status_label(const std::string &value) {
  if (value == "missing")
    return "texto associado não encontrado";
  if (value == "empty")
    return "trecho vazio";
  if (value == "unresolved")
    return "trecho não localizado";
  if (value == "changed")
    return "texto alterado — conferir";
  if (value == "hidden")
    return "fora da leitura atual";
  if (value == "reference-only")
    return "referência sem incluir o texto";
  return "texto disponível para consulta";
}
inline std::string realization_review_label(const std::string &value) {
  if (value == "confirmed")
    return "conferido pelo autor";
  if (value == "diverged")
    return "diferença intencional entre texto e plano";
  return "ainda não conferido";
}
} // namespace inde::ui
