#pragma once
#include "inde/project/narrative.hpp"
#include "inde/project/manifest.hpp"
#include "inde/project/text_semantics.hpp"
#include "inde/persistence/json.hpp"
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace inde::ui::author {
struct Term {
  std::string_view name, aliases, explanation;
};
inline constexpr std::array terms{
    Term{"Projeto", "arquivo .inde", "Seu conjunto de trabalho. Pode reunir obras, documentos livres e referências."},
    Term{"Obra", "trabalho autoral", "O trabalho que você escreve ou organiza: romance, conto, roteiro, memórias ou outra forma."},
    Term{"Universo", "propriedade intelectual; IP; conjunto de obras", "Agrupamento de obras. Não é registro jurídico nem precisa ser uma série. Em não ficção, pode ser chamado de conjunto de obras."},
    Term{"Personagens e mundo", "entidades; realidade ficcional", "Fichas dos elementos da história: personagens, lugares, acontecimentos, informações, objetivos e categorias pessoais."},
    Term{"Elemento da história", "entidade; entidade narrativa", "Um item cadastrado. Uma menção no texto não declara sua presença nem confirma um fato."},
    Term{"Relações", "relações narrativas; contraparte; direcionalidade", "Ligações com significado entre elementos: é mãe de, vive em, pertence a. O sentido importa."},
    Term{"Nome alternativo", "alias; nome canônico", "Apelido, título ou grafia alternativa. O nome principal e a identidade da ficha são preservados."},
    Term{"Presença em um lugar", "presença narrativa; sujeito de presença", "Declara quem ou o que está em um lugar e período. Não é inferida de uma menção ou lembrança."},
    Term{"Plano narrativo", "estrutura narrativa; montagem narrativa", "Uma alternativa de planejamento. Não reorganiza o manuscrito automaticamente."},
    Term{"Parte da narrativa", "unidade narrativa", "Uma divisão planejada, como cena, sequência ou passagem. Não precisa ter texto escrito."},
    Term{"Linha narrativa", "fio narrativo; trama; subtrama; arco", "Um fio que você acompanha. Pode ter ordem própria; não é obrigatoriamente um arco ou subtrama."},
    Term{"Ordem do plano narrativo", "montagem narrativa", "Organiza as partes no plano escolhido. Não altera datas, ordem de escrita ou manuscrito."},
    Term{"Cronologia da história", "tempo ficcional; linha temporal", "Quando os acontecimentos ocorrem. Não determina a ordem em que são contados."},
    Term{"Ordem de escrita", "fila de escrita", "Sequência opcional em que você quer trabalhar; não determina a ordem para o leitor."},
    Term{"Ordem do manuscrito", "apresentação editorial; ordem editorial", "Organização desta apresentação ao leitor. É independente do plano, da cronologia e da escrita."},
    Term{"Modelos de planejamento", "modelos reutilizáveis; template", "Métodos reutilizáveis para criar planos; não são a obra nem o plano atual."},
    Term{"Trecho marcado", "âncora; âncora textual; reancorar", "Intervalo identificado no texto. Se não for localizado com segurança, você escolhe novamente o trecho."},
    Term{"Marcações do texto", "camadas semânticas; anotação semântica", "Narração, descrição, diálogo, pensamento, ponto de vista, tempo, comentários e associações escolhidas por você."},
    Term{"Ponto de vista", "perspectiva tipada; perspectiva situada; PoV", "Documento e trecho podem ter declarações distintas. Pessoa da narração, focalização e percepção não são o mesmo campo."},
    Term{"Focalização", "focalizador; percepção", "Como a informação é percebida. O personagem cuja percepção acompanhamos não é necessariamente o narrador."},
    Term{"Ações no texto", "comandos inline; modelos inline", "Inserções e sugestões explícitas no contexto da escrita, sem interpretação automática."},
    Term{"Texto que desenvolve esta parte", "realização textual; alvo textual", "Documento, trecho ou uso de bloco associado ao plano. Associar não copia o texto."},
    Term{"Conferir texto e planejamento", "reconciliação textual", "Revise a associação quando o texto mudar. Uma diferença intencional pode ser registrada sem correção automática."},
    Term{"Continuidade da história", "exceção autoral; continuidade", "Avisos baseados no que você declarou. Não bloqueiam a escrita; informação insuficiente não é contradição."},
    Term{"Montagem de texto", "composição", "Reúne usos de blocos em uma leitura. Rascunhos salvos não entram nela antes de aplicar uma versão."},
    Term{"Uso do bloco na montagem", "ocorrência de composição; ocorrência de bloco", "Uma posição específica do bloco. Retirá-la não exclui o bloco de outros lugares."},
    Term{"Versão em uso na montagem", "versão aplicada; acompanhar bloco", "Versão que contribui para a leitura; usos que acompanham o bloco podem mudar quando você aplica outra versão."},
    Term{"Cópia independente", "fork; bifurcar; separar prosa", "Outro texto, sem sincronização posterior. Duplicar um plano não torna seus documentos independentes."},
    Term{"Linha do tempo", "eixo temporal", "Uma estrutura independente de marcos e datas. Não é uma linha narrativa."},
    Term{"Calendários e relógios", "sistema temporal; biblioteca de tempo", "Regras de datas e horários. Criar regras e aplicá-las a uma linha do tempo são decisões separadas."},
    Term{"Data ou período", "referência temporal; precisão temporal", "Registro com calendário, horário e precisão, reutilizável sem perder sua origem."},
    Term{"Lugares e mapas", "cartografia; LOD; raster", "Posições e mapas. Nível de detalhe e aparência em pixels ficam nas opções do mapa."},
    Term{"Organização do manuscrito", "estrutura editorial; unidade editorial; raiz; filho", "Elementos principais e internos para apresentação ao leitor. Não é a ordem de planejamento."},
    Term{"Preparar para exportação", "produção; saída; snapshot", "Prepara uma versão para gerar arquivos. Exportar não significa publicar."},
    Term{"Excluir, retirar ou desvincular", "remover; deletar; dependências", "Excluir apaga o item; retirar o remove de um conjunto; desvincular remove uma associação. Veja onde é usado antes de excluir."},
    Term{"Ponto de recuperação", "checkpoint; journal", "Conteúdo preservado para recuperação, não uma versão aprovada ou backup completo do projeto."},
    Term{"Sentido da palavra", "acepção; sense; synset", "Cada sentido mantém sua definição e fonte. A consulta não escolhe nem associa um sentido automaticamente."},
    Term{"Fonte e histórico", "proveniência; QA; gate; overlay", "Origem dos dados e suas revisões. Identificadores técnicos continuam disponíveis nos detalhes."},
    Term{"Espaços de trabalho", "workspace; guardar contexto", "Guardam onde você estava e filtros suportados. Não copiam textos nem são backup do projeto."},
    Term{"Registrado, planejado e conferido", "canônico; confirmado; aplicado", "Registrado não significa verdadeiro; planejado não significa escrito; conferido não significa publicado."},
};

inline std::string category_label(const project::EntityType &type) {
  return type.is_builtin && type.key == "location" ? "Lugar" : type.name;
}
inline std::string annotation_label(const project::AnnotationTypeDefinition &type) {
  if (!type.system) return type.label; // Never rewrite author-defined labels.
  if (type.key == "inde.semantic.perspective") return "Ponto de vista deste trecho";
  if (type.key == "inde.semantic.entity-mention") return "Menção a um elemento da história";
  if (type.key == "inde.semantic.materialization") return "Como este trecho se relaciona com a ficha";
  if (type.key == "inde.editor.review-state") return "Parecer sobre esta versão do documento";
  return type.label;
}
inline std::string field_label(const project::AnnotationTypeDefinition &type,
                              const project::SemanticFieldDefinition &field) {
  if (!type.system) return field.label;
  if (type.key == "inde.semantic.dialogue" && field.key == "speaker") return "Quem fala";
  if (type.key == "inde.semantic.dialogue" && field.key == "acts") return "O que a fala faz";
  if (type.key == "inde.semantic.thought" && field.key == "thinker") return "Quem pensa";
  if (type.key == "inde.semantic.description" && field.key == "targets") return "O que está sendo descrito";
  if (type.key == "inde.semantic.perspective" && field.key == "person") return "Pessoa da narração";
  if (type.key == "inde.semantic.perspective" && field.key == "viewpoint") return "Percepção de";
  if (type.key == "inde.semantic.temporal-context" && field.key == "participants") return "Quem ou o que está presente (opcional)";
  if (type.key == "inde.semantic.entity-mention" && field.key == "target") return "Elemento mencionado";
  if (type.key == "inde.semantic.materialization" && field.key == "target") return "Ficha associada";
  if (type.key == "inde.semantic.materialization" && field.key == "purpose") return "Como o trecho se relaciona com a ficha";
  return field.label;
}

// Stable opaque IDs are generated only for a new definition, never when a
// displayed name is edited. No accidental built-in capabilities via a name.
inline std::string new_category_key() { return "personal-" + project::new_uuid(); }
struct PersonalField {
  std::string name;
  project::SemanticFieldKind kind{project::SemanticFieldKind::Text};
  bool required{}, multiple{};
  std::vector<std::string> choices;
};
inline std::string personal_definition(const std::string &name,
                                       const std::string &description,
                                       const std::vector<PersonalField> &fields,
                                       const std::string &key) {
  using namespace persistence::json;
  project::AnnotationTypeDefinition type{key, name, description, 1, false, true, {}};
  if (name.find_first_not_of(" \t\r\n") == std::string::npos)
    throw std::runtime_error("Informe o nome da marcação.");
  if (!key.starts_with("project.")) throw std::runtime_error("Identificador reservado; use a área pessoal do projeto.");
  Value::Array encoded;
  for (std::size_t i = 0; i < fields.size(); ++i) {
    const auto &f = fields[i];
    auto field_key = "field-" + std::to_string(i + 1);
    type.fields.push_back({field_key, f.name, f.kind, f.required, f.multiple, f.choices, {}});
    Value::Array choices;
    for (const auto &choice : f.choices) choices.emplace_back(choice);
    encoded.emplace_back(Value::Object{{"key", field_key}, {"label", f.name},
        {"kind", project::to_string(f.kind)}, {"required", f.required},
        {"multiple", f.multiple}, {"values", std::move(choices)},
        {"entity_types", Value::Array{}}});
  }
  project::validate(type);
  return serialize(Value::Object{{"schema", 1}, {"key", key}, {"label", name},
      {"description", description}, {"version", 1}, {"system", false},
      {"overlap", true}, {"fields", std::move(encoded)}});
}
} // namespace inde::ui::author
