#include "inde/project/writing.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <uuid/uuid.h>

namespace inde::project {
namespace {

void validate_uuid(const std::string &value, const char *label) {
  uuid_t parsed{};
  if (uuid_parse(value.c_str(), parsed) != 0)
    throw std::runtime_error(std::string(label) + " possui UUID inválido");
}

} // namespace

std::string to_string(DocumentTextStyle value) {
  switch (value) {
  case DocumentTextStyle::Bold:
    return "bold";
  case DocumentTextStyle::Italic:
    return "italic";
  case DocumentTextStyle::Underline:
    return "underline";
  case DocumentTextStyle::Strikethrough:
    return "strikethrough";
  case DocumentTextStyle::Heading:
    return "heading";
  case DocumentTextStyle::Subheading:
    return "subheading";
  case DocumentTextStyle::Quote:
    return "quote";
  }
  throw std::runtime_error("Estilo textual desconhecido");
}

DocumentTextStyle document_text_style_from_string(const std::string &value) {
  if (value == "bold")
    return DocumentTextStyle::Bold;
  if (value == "italic")
    return DocumentTextStyle::Italic;
  if (value == "underline")
    return DocumentTextStyle::Underline;
  if (value == "strikethrough")
    return DocumentTextStyle::Strikethrough;
  if (value == "heading")
    return DocumentTextStyle::Heading;
  if (value == "subheading")
    return DocumentTextStyle::Subheading;
  if (value == "quote")
    return DocumentTextStyle::Quote;
  throw std::runtime_error("Estilo textual de Documento inválido");
}

std::string to_string(DocumentPurpose value) {
  switch (value) {
  case DocumentPurpose::MainText:
    return "main-text";
  case DocumentPurpose::Annotation:
    return "annotation";
  case DocumentPurpose::Revision:
    return "revision";
  case DocumentPurpose::Outline:
    return "outline";
  case DocumentPurpose::Research:
    return "research";
  case DocumentPurpose::Reference:
    return "reference";
  case DocumentPurpose::Other:
    return "other";
  }
  throw std::runtime_error("Finalidade documental desconhecida");
}

std::string display_name(DocumentPurpose value) {
  switch (value) {
  case DocumentPurpose::MainText:
    return "Texto do elemento";
  case DocumentPurpose::Annotation:
    return "Anotação";
  case DocumentPurpose::Revision:
    return "Revisão documental";
  case DocumentPurpose::Outline:
    return "Esquema";
  case DocumentPurpose::Research:
    return "Pesquisa";
  case DocumentPurpose::Reference:
    return "Referência";
  case DocumentPurpose::Other:
    return "Outro";
  }
  throw std::runtime_error("Finalidade documental desconhecida");
}

DocumentPurpose document_purpose_from_string(const std::string &value) {
  if (value == "main-text")
    return DocumentPurpose::MainText;
  if (value == "annotation")
    return DocumentPurpose::Annotation;
  if (value == "revision")
    return DocumentPurpose::Revision;
  if (value == "outline")
    return DocumentPurpose::Outline;
  if (value == "research")
    return DocumentPurpose::Research;
  if (value == "reference")
    return DocumentPurpose::Reference;
  if (value == "other")
    return DocumentPurpose::Other;
  throw std::runtime_error("Finalidade de Documento inválida");
}

std::size_t utf8_character_count(const std::string &text) {
  std::size_t result = 0;
  for (const unsigned char character : text) {
    if ((character & 0xC0U) != 0x80U)
      ++result;
  }
  return result;
}

DocumentTextStatistics analyze_document_text(const std::string &text,
                                             std::size_t words_per_minute,
                                             std::size_t long_paragraph_words) {
  if (words_per_minute == 0)
    throw std::runtime_error("A velocidade de leitura precisa ser positiva");
  if (long_paragraph_words == 0)
    throw std::runtime_error("O limite de parágrafo precisa ser positivo");

  DocumentTextStatistics result;
  result.characters_with_spaces = utf8_character_count(text);
  std::unordered_map<std::string, std::size_t> frequencies;
  const std::unordered_set<std::string> ignored{
      "a",  "as",  "o",    "os",  "e",   "de",  "da", "das", "do", "dos", "em",
      "um", "uma", "para", "por", "com", "que", "se", "no",  "na", "nos", "nas",
      "ao", "aos", "the",  "and", "of",  "to",  "in", "is",  "it", "for"};

  std::string token;
  bool paragraph_has_text = false;
  bool previous_was_sentence_end = false;
  std::size_t paragraph_words = 0;
  std::size_t paragraph_number = 1;
  std::size_t paragraph_start = 0;
  std::size_t character_offset = 0;

  const auto finish_word = [&] {
    if (token.empty())
      return;
    ++result.words;
    ++paragraph_words;
    paragraph_has_text = true;
    for (auto &character : token) {
      if (character >= 'A' && character <= 'Z')
        character = static_cast<char>(character - 'A' + 'a');
    }
    if (token.size() >= 3 && !ignored.contains(token))
      ++frequencies[token];
    token.clear();
  };
  const auto finish_paragraph = [&] {
    if (!paragraph_has_text)
      return;
    ++result.paragraphs;
    if (paragraph_words >= long_paragraph_words)
      result.long_paragraphs.push_back(
          {paragraph_number, paragraph_words, paragraph_start});
    ++paragraph_number;
    paragraph_words = 0;
    paragraph_has_text = false;
  };

  for (std::size_t index = 0; index < text.size();) {
    const unsigned char character = static_cast<unsigned char>(text[index]);
    std::size_t bytes = 1;
    if ((character & 0xE0U) == 0xC0U)
      bytes = 2;
    else if ((character & 0xF0U) == 0xE0U)
      bytes = 3;
    else if ((character & 0xF8U) == 0xF0U)
      bytes = 4;
    bytes = std::min(bytes, text.size() - index);

    const bool ascii = character < 0x80U;
    const bool whitespace = ascii && std::isspace(character) != 0;
    const bool word_character = !ascii || std::isalnum(character) != 0 ||
                                character == '\'' || character == '-';
    if (!whitespace)
      ++result.characters_without_spaces;
    if (word_character)
      token.append(text, index, bytes);
    else
      finish_word();
    const bool sentence_end =
        ascii && (character == '.' || character == '!' || character == '?');
    if (sentence_end && !previous_was_sentence_end)
      ++result.sentences;
    previous_was_sentence_end = sentence_end;

    if (character == '\n') {
      finish_word();
      finish_paragraph();
      paragraph_start = character_offset + 1;
    }
    index += bytes;
    ++character_offset;
  }
  finish_word();
  finish_paragraph();
  if (result.words > 0)
    result.reading_minutes =
        (result.words + words_per_minute - 1) / words_per_minute;

  for (const auto &[word, count] : frequencies) {
    if (count >= 3)
      result.repeated_words.push_back({word, count});
  }
  std::sort(result.repeated_words.begin(), result.repeated_words.end(),
            [](const auto &left, const auto &right) {
              if (left.count != right.count)
                return left.count > right.count;
              return left.word < right.word;
            });
  if (result.repeated_words.size() > 8)
    result.repeated_words.resize(8);
  return result;
}

void validate(const Document &value) {
  validate_uuid(value.id, "O Documento");
  if (value.editorial_node_id)
    validate_uuid(*value.editorial_node_id, "A colocação editorial");
  if (value.group_id)
    validate_uuid(*value.group_id, "O grupo documental");
  if (value.revision_of_id) {
    validate_uuid(*value.revision_of_id, "O Documento de origem");
    if (*value.revision_of_id == value.id)
      throw std::runtime_error("Um Documento não pode ser revisão de si mesmo");
  }
  static_cast<void>(to_string(value.purpose));
  if (value.title.empty())
    throw std::runtime_error("O título do Documento é obrigatório");
  if (value.title.size() > 512)
    throw std::runtime_error("O título do Documento excede 512 bytes");
  if (value.created_at.empty() || value.updated_at.empty())
    throw std::runtime_error("Datas do Documento ausentes");
  if (value.word_goal && *value.word_goal == 0)
    throw std::runtime_error("A meta de palavras precisa ser maior que zero");
  if (value.word_goal && *value.word_goal > 10000000)
    throw std::runtime_error("A meta de palavras excede o limite suportado");
  if (value.revision_label.size() > 128)
    throw std::runtime_error("O rótulo da revisão excede 128 bytes");
  if (value.perspective.size() > 256)
    throw std::runtime_error("A perspectiva excede 256 bytes");

  const auto character_count = utf8_character_count(value.content);
  if (value.formatting.size() > 10000)
    throw std::runtime_error("O Documento excede 10 mil intervalos formatados");
  for (const auto &span : value.formatting) {
    static_cast<void>(to_string(span.style));
    if (span.start_offset >= span.end_offset ||
        span.end_offset > character_count)
      throw std::runtime_error("Intervalo de formatação inválido");
  }

  if (value.anchors.size() > 2000)
    throw std::runtime_error("O Documento excede 2 mil âncoras");
  std::unordered_set<std::string> anchor_ids;
  for (const auto &anchor : value.anchors) {
    validate_uuid(anchor.id, "A âncora");
    if (anchor.label.empty() || anchor.label.size() > 256)
      throw std::runtime_error("O nome da âncora deve ter entre 1 e 256 bytes");
    if (anchor.start_offset > anchor.end_offset ||
        anchor.end_offset > character_count)
      throw std::runtime_error("Intervalo da âncora inválido");
    if (!anchor_ids.insert(anchor.id).second)
      throw std::runtime_error("Âncora duplicada no Documento");
  }

  if (value.entity_references.size() > 2000)
    throw std::runtime_error("O Documento excede 2 mil vínculos com entidades");
  std::unordered_set<std::string> reference_ids;
  std::unordered_set<std::string> reference_targets;
  for (const auto &reference : value.entity_references) {
    validate_uuid(reference.id, "A referência documental");
    validate_uuid(reference.entity_id, "A entidade referenciada");
    if (reference.notes.size() > 4096)
      throw std::runtime_error("As notas da referência excedem 4096 bytes");
    if (!reference_ids.insert(reference.id).second)
      throw std::runtime_error("Referência documental duplicada");
    if (reference.anchor_id && !anchor_ids.contains(*reference.anchor_id))
      throw std::runtime_error("A referência aponta para uma âncora ausente");
    const auto target = reference.entity_id + "\n" +
                        reference.anchor_id.value_or(std::string{});
    if (!reference_targets.insert(target).second)
      throw std::runtime_error(
          "A entidade já está vinculada ao mesmo contexto do Documento");
  }
}

void validate(const DocumentGroup &value) {
  validate_uuid(value.id, "O grupo documental");
  if (value.name.empty() || value.name.size() > 256)
    throw std::runtime_error("O nome do grupo deve ter entre 1 e 256 bytes");
  if (value.description.size() > 2048)
    throw std::runtime_error("A descrição do grupo excede 2048 bytes");
  if (value.created_at.empty() || value.updated_at.empty())
    throw std::runtime_error("Datas do grupo documental ausentes");
}

} // namespace inde::project
