#pragma once
#include <array>
#include <string>
#include <string_view>
#include <utility>
namespace inde::ui::author {
// Translate known diagnostics at the presentation boundary only. Unknown
// diagnostics (possibly containing author names, paths or SQL) stay verbatim.
inline std::string diagnostic(std::string_view raw) {
  using Pair = std::pair<std::string_view, std::string_view>;
  static constexpr std::array messages{
    Pair{"Âncoras exigem UTF-8 válido", "O texto precisa estar em UTF-8 válido para marcar trechos."},
    Pair{"Evidência sem âncora", "A marcação perdeu a referência ao trecho. Localize novamente o trecho antes de usá-la."},
    Pair{"Âncora semântica fora do Documento", "O trecho da marcação está fora do documento. Localize-o novamente."},
    Pair{"Anotação ausente", "A marcação não foi encontrada. Atualize a lista."},
    Pair{"Âncora ausente", "O trecho marcado não foi encontrado. Atualize a lista."},
    Pair{"Revisão semântica excedida", "O limite de versões das marcações foi atingido. Nada foi aplicado."},
    Pair{"Chave semântica ausente ou excedida", "O identificador técnico deve ter de 1 a 128 caracteres."},
    Pair{"Chave semântica inválida", "O identificador técnico aceita letras minúsculas sem acento, números, ponto, hífen e sublinhado."},
    Pair{"Tipo semântico precisa de namespace", "O identificador técnico precisa indicar uma área, como project.minha-marcacao."},
    Pair{"Rótulo de tipo ausente", "Informe o nome do tipo de marcação."},
    Pair{"Rótulo de campo ausente", "Informe o nome de cada campo."},
    Pair{"Tipo excede 32 campos", "Uma marcação pode ter até 32 campos. Retire os campos excedentes."},
    Pair{"Campo duplicado no tipo", "Dois campos têm o mesmo identificador técnico. Use identificadores diferentes."},
    Pair{"Opção vazia ou duplicada", "Preencha todas as opções da lista e evite opções repetidas."},
    Pair{"Enumeração sem opções", "Adicione ao menos uma opção à lista."},
    Pair{"Opções apenas em campos de enumeração", "Somente campos de lista de opções podem declarar opções."},
    Pair{"Tipos de entidade apenas em referências", "A restrição de categorias só se aplica a campos que selecionam elementos da história."},
    Pair{"Campo semântico excedido ou UTF-8 inválido", "Um campo ultrapassa seu limite ou contém texto inválido. Confira o nome, a descrição e as opções."},
    Pair{"Limite de camadas semânticas excedido", "O documento atingiu o limite de marcações ou associações. Confira as marcações existentes antes de adicionar outras."},
    Pair{"Uma entidade do Documento foi removida", "Um elemento associado ao documento foi excluído. Confira as associações antes de continuar."},
    Pair{"Declare explicitamente o papel temporal da data no texto", "Escolha o que a data indica neste texto: acontecimento, menção, lembrança ou referência."},
    Pair{"Data/papel duplicado na anotação", "Esta data já está associada à marcação com a mesma finalidade."},
    Pair{"Estado de âncora inválido", "O estado do trecho marcado é inválido. Nada foi aplicado."},
    Pair{"Estado de âncora desconhecido", "O estado do trecho marcado não é reconhecido. Nada foi aplicado."}
  };
  for (const auto &[technical, friendly] : messages)
    if (technical == raw) return std::string(friendly);
  return std::string(raw);
}
} // namespace inde::ui::author
