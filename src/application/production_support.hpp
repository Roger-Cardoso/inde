#pragma once
#include "inde/application/work_production.hpp"
#include <cstdint>
#include <stdexcept>
namespace inde::application::production_detail {
struct Run {
  std::string text;
  bool bold{}, italic{}, underline{}, strike{};
};
struct Paragraph {
  std::vector<Run> runs;
};
std::vector<Paragraph> paragraphs(const project::Document &,
                                  std::stop_token = {});
std::string xml(const std::string &);
std::string sha(const std::string &);
std::string zip(const std::vector<std::pair<std::string, std::string>> &);
std::string make_docx(const WorkProduction &, std::stop_token);
std::string make_epub(const WorkProduction &, std::stop_token);
std::string make_pdf(const WorkProduction &, std::stop_token);
inline void cancelled(std::stop_token stop) {
  if (stop.stop_requested())
    throw std::runtime_error("Produção cancelada; fonte preservada");
}
} // namespace inde::application::production_detail
