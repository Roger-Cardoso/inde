#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace inde::project {

enum class DocumentTextStyle {
  Bold,
  Italic,
  Underline,
  Strikethrough,
  Heading,
  Subheading,
  Quote,
};

[[nodiscard]] std::string to_string(DocumentTextStyle value);
[[nodiscard]] DocumentTextStyle
document_text_style_from_string(const std::string &value);

// Formatação é uma camada sobre offsets Unicode do corpo. Ela enriquece a
// apresentação e nunca substitui os caracteres digitados.
struct DocumentFormatSpan {
  DocumentTextStyle style{DocumentTextStyle::Bold};
  std::size_t start_offset{};
  std::size_t end_offset{};
};

// Uma âncora nomeia uma posição ou um intervalo do Documento. A identidade
// permite que outras camadas a referenciem sem copiar o trecho textual.
struct DocumentAnchor {
  std::string id;
  std::string label;
  std::size_t start_offset{};
  std::size_t end_offset{};
};

// A entidade continua pertencendo ao Planejamento. O Documento apenas mantém
// uma referência explícita, opcionalmente situada numa âncora.
struct DocumentEntityReference {
  std::string id;
  std::string entity_id;
  std::optional<std::string> anchor_id;
  std::string notes;
};

enum class DocumentSearchMatch { None, Name, Content, NameAndContent };

// A finalidade organiza o trabalho do autor sem confundir Documento com
// unidade editorial. "Revisao" aqui significa revisao documental, não versão
// publicada da Obra nem alternativa narrativa.
enum class DocumentPurpose {
  MainText,
  Annotation,
  Revision,
  Outline,
  Research,
  Reference,
  Other,
};

[[nodiscard]] std::string to_string(DocumentPurpose value);
[[nodiscard]] std::string display_name(DocumentPurpose value);
[[nodiscard]] DocumentPurpose
document_purpose_from_string(const std::string &value);

// Grupo virtual da Biblioteca. Não representa uma pasta do sistema de
// arquivos e não altera a identidade nem a colocação editorial do Documento.
struct DocumentGroup {
  std::string id;
  std::string name;
  std::string description;
  std::string created_at;
  std::string updated_at;
};

struct RepeatedWord {
  std::string word;
  std::size_t count{};
};

struct LongParagraph {
  std::size_t paragraph_number{};
  std::size_t word_count{};
  std::size_t start_offset{};
};

struct DocumentTextStatistics {
  std::size_t words{};
  std::size_t characters_with_spaces{};
  std::size_t characters_without_spaces{};
  std::size_t paragraphs{};
  std::size_t sentences{};
  std::size_t reading_minutes{};
  std::vector<RepeatedWord> repeated_words;
  std::vector<LongParagraph> long_paragraphs;
};

[[nodiscard]] DocumentTextStatistics
analyze_document_text(const std::string &text,
                      std::size_t words_per_minute = 200,
                      std::size_t long_paragraph_words = 120);

// Documento textual possui identidade própria. A colocação editorial é um
// vínculo opcional e nunca contém ou substitui o texto.
struct Document {
  std::string id;
  std::optional<std::string> editorial_node_id;
  std::string title;
  std::string content;
  std::string created_at;
  std::string updated_at;
  std::optional<std::size_t> word_goal;
  std::vector<DocumentFormatSpan> formatting;
  std::vector<DocumentAnchor> anchors;
  std::vector<DocumentEntityReference> entity_references;
  std::optional<std::string> group_id;
  DocumentPurpose purpose{DocumentPurpose::MainText};
  std::optional<std::string> revision_of_id;
  std::string revision_label;
  std::string perspective;
};

// Projeção leve para Biblioteca. O corpo completo só é materializado quando
// um Documento é aberto no editor.
struct DocumentSummary {
  std::string id;
  std::optional<std::string> editorial_node_id;
  std::string title;
  std::size_t character_count{};
  std::string created_at;
  std::string updated_at;
  std::size_t entity_reference_count{};
  DocumentSearchMatch search_match{DocumentSearchMatch::None};
  std::optional<std::string> group_id;
  DocumentPurpose purpose{DocumentPurpose::MainText};
  std::optional<std::string> revision_of_id;
  std::string revision_label;
  std::string perspective;
};

void validate(const Document &value);
void validate(const DocumentGroup &value);
[[nodiscard]] std::size_t utf8_character_count(const std::string &text);

} // namespace inde::project
