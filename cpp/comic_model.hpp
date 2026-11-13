#pragma once
#include "snapshot_model.hpp"
#include <map>
namespace inde::comic {
using publication::A;
using publication::O;
using publication::V;
struct Asset {
  std::string id, name, mime, data, role{"general"};
};
struct Element {
  std::string id, kind{"bubble"}, shape{"rounded"}, text, gloss, speaker;
  std::string lang, dir{"auto"}, align{"center"}, valign{"center"},
      font{"sans-serif"}, font_asset, animation{"none"};
  std::string ink{"#18252e"}, background{"#fffaf0"}, source_document,
      source_hash;
  bool translate{true};
  double x{8}, y{8}, w{48}, h{26}, size{24};
};
struct Panel {
  std::string id, title, alt, image, background{"#1c3040"};
  double ratio{1.6};
  std::vector<Element> elements;
};
struct Book {
  std::string id, work_id, title, slug, lang{"pt-BR"}, layout{"strip"};
  std::int64_t revision{};
  bool removed{};
  std::vector<Panel> panels;
  std::map<std::string, Asset> assets;
};
V encode(const Book &);
Book decode(const V &);
void validate(const Book &);
std::string escape(const std::string &);
std::string raw_asset(const Asset &);
Asset import_asset(const std::filesystem::path &, bool font);
Element copy_document(const project::Document &);
struct Position {
  double x, y;
  bool snap_x, snap_y;
};
Position snap(const Element &, double x, double y, bool enabled = true);
Book example(const std::string &work);
// Produces only public HTML/resources. Authoring data is not a public format.
V public_snapshot(const Book &, const publication::V &site,
                  const std::filesystem::path &resources);
} // namespace inde::comic
