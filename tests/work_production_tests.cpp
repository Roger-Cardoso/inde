#include "../src/application/production_support.hpp"
#include "inde/application/editorial_review.hpp"
#include "inde/application/work_production.hpp"
#include "inde/persistence/json.hpp"
#include "inde/project/document_state.hpp"
#include "inde/project/manifest.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <source_location>
using namespace inde;
void require(bool condition,
             std::source_location where = std::source_location::current()) {
  if (!condition)
    throw std::runtime_error("Failure line " + std::to_string(where.line()));
}
template <class F>
void refuses(F f, std::source_location w = std::source_location::current()) {
  bool rejected{};
  try {
    f();
  } catch (const std::exception &) {
    rejected = true;
  }
  require(rejected, w);
}
std::string read(const std::filesystem::path &p) {
  std::ifstream in(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), {});
}
int main(int argc, char **argv) {
  try {
    namespace a = application;
    namespace pd = a::production_detail;
    project::Work work;
    work.id = "work";
    work.title = "Caderno de produção";
    work.subtitle = "Ensaio dos formatos e perfis";
    work.language = "pt-BR";
    project::StructuralNode chapter;
    chapter.id = "chapter";
    chapter.work_id = work.id;
    chapter.structure_id = "structure";
    chapter.position = 1000;
    chapter.title = "A memória da água";
    chapter.designator = "1";
    auto scene = chapter;
    scene.id = "scene";
    scene.parent_id = chapter.id;
    scene.position = 1000;
    scene.title = "O encontro";
    scene.designator = "";
    auto end = chapter;
    end.id = "end";
    end.position = 2000;
    end.title = "A volta";
    end.designator = "2";
    std::vector nodes{end, scene, chapter};
    auto order = a::production_order(nodes);
    require(order[0].id == chapter.id && order[1].id == scene.id &&
            order[2].id == end.id);
    auto bad = nodes;
    bad[0].position = chapter.position;
    refuses([&] { a::production_order(bad); });
    bad = nodes;
    bad[1].parent_id = "missing";
    refuses([&] { a::production_order(bad); });
    bad = nodes;
    bad[2].parent_id = scene.id;
    refuses([&] { a::production_order(bad); });
    bad = nodes;
    bad.push_back(chapter);
    refuses([&] { a::production_order(bad); });
    project::Document first;
    first.id = "first";
    first.title = "Cena da janela";
    first.editorial_node_id = scene.id;
    first.updated_at = "2026-10-08T12:00:00Z";
    first.content =
        "Água, memória e escrita. < & >\r\n\tO viajante abriu a janela.\n\n";
    first.formatting = {{project::DocumentTextStyle::Bold, 0, 4, {}},
                        {project::DocumentTextStyle::Italic, 6, 13, {}}};
    for (int i = 0; i < 36; ++i)
      first.content +=
          "A luz chegava ao quarto e demorava sobre os livros. A narradora "
          "conservou a dúvida em vez de inventar uma resposta. Linha " +
          std::to_string(i + 1) + ".\n";
    auto second = first;
    second.id = "second";
    second.title = "Bilhete de despedida";
    second.editorial_node_id = end.id;
    second.content = "Depois da chuva, a rua guardou silêncio.\nÚltima linha.";
    second.formatting.clear();
    auto alternate = second;
    alternate.id = "alternate";
    alternate.title = "Alternativa não escolhida";
    alternate.content = "ESTE TEXTO NÃO PODE ENTRAR AUTOMATICAMENTE";
    alternate.editorial_node_id = scene.id;
    const std::vector docs{first, second, alternate};
    a::ProductionChoices choices{{chapter.id, {true, {}}},
                                 {scene.id, {true, {first.id}}},
                                 {end.id, {true, {second.id}}}};
    a::ProductionOptions options;
    options.author = "Autor de teste";
    auto out = a::prepare_work_production("project", "structure", work, nodes,
                                          docs, choices, options);
    require(out.units.size() == 3 && out.units[1].depth == 1 &&
            out.preview.find(alternate.content) == std::string::npos &&
            out.omissions.size() == 1);
    require(project::same_document_state(docs[0], first));
    auto two = choices;
    two[scene.id].documents = {alternate.id, first.id};
    auto explicit_two = a::prepare_work_production("project", "structure", work,
                                                   nodes, docs, two, options);
    require(explicit_two.preview.find(alternate.content) <
            explicit_two.preview.find(first.content));
    auto wrong = choices;
    wrong[scene.id].documents = {first.id, first.id};
    refuses([&] {
      a::prepare_work_production("project", "structure", work, nodes, docs,
                                 wrong, options);
    });
    wrong = choices;
    wrong[scene.id].documents = {second.id};
    refuses([&] {
      a::prepare_work_production("project", "structure", work, nodes, docs,
                                 wrong, options);
    });
    wrong = choices;
    wrong["missing"] = {true, {}};
    refuses([&] {
      a::prepare_work_production("project", "structure", work, nodes, docs,
                                 wrong, options);
    });
    refuses([&] {
      a::prepare_work_production("project", "structure", work, nodes, docs, {},
                                 options);
    });
    bad = nodes;
    bad[0].structure_id = "alternative";
    refuses([&] {
      a::prepare_work_production("project", "structure", work, bad, docs,
                                 choices, options);
    });
    auto invalid = docs;
    invalid[0].content = std::string("x\0y", 3);
    refuses([&] {
      a::prepare_work_production("project", "structure", work, nodes, invalid,
                                 choices, options);
    });
    invalid = docs;
    invalid[0].content = "\xff";
    refuses([&] {
      a::prepare_work_production("project", "structure", work, nodes, invalid,
                                 choices, options);
    });
    invalid = docs;
    invalid[0].formatting.push_back(
        {project::DocumentTextStyle::Bold, 0, 9999999, {}});
    refuses([&] {
      a::prepare_work_production("project", "structure", work, nodes, invalid,
                                 choices, options);
    });
    const auto ps = pd::paragraphs(first);
    require(ps[0].runs[0].text == "Água" && ps[0].runs[0].bold);
    require(ps[1].runs[0].text.starts_with("\t"));
    require(ps[2].runs.empty());
    std::stop_source cancel;
    cancel.request_stop();
    refuses([&] { pd::make_docx(out, cancel.get_token()); });
    refuses([&] { pd::make_epub(out, cancel.get_token()); });
    refuses([&] { pd::make_pdf(out, cancel.get_token()); });
    const auto root = argc == 3 && std::string(argv[1]) == "--emit"
                          ? std::filesystem::absolute(argv[2])
                          : std::filesystem::temp_directory_path() /
                                ("inde-production-" + project::new_uuid());
    std::filesystem::create_directories(root);
    refuses([&] {
      a::write_work_production(out, root / "cancelled", cancel.get_token());
    });
    require(!std::filesystem::exists(root / "cancelled"));
    a::write_work_production(out, root / "manuscript");
    const auto report =
        persistence::json::parse(read(root / "manuscript/relatorio.json"));
    require(report.at("complete").as_boolean() &&
            report.at("readback_verified").as_boolean());
    for (const auto &file : report.at("outputs").as_array())
      require(
          pd::sha(read(root / "manuscript" / file.at("file").as_string())) ==
          file.at("sha256").as_string());
    const auto old = read(root / "manuscript/obra.docx");
    refuses([&] { a::write_work_production(out, root / "manuscript"); });
    require(read(root / "manuscript/obra.docx") == old);
    std::filesystem::create_directory_symlink(root / "manuscript",
                                              root / "link");
    refuses([&] { a::write_work_production(out, root / "link/never"); });
    require(!std::filesystem::exists(root / "manuscript/never"));
    options.profile = a::ProductionProfile::Reading;
    auto reading = a::prepare_work_production("project", "structure", work,
                                              nodes, docs, choices, options);
    a::write_work_production(reading, root / "reading");
    require(read(root / "reading/obra.pdf").starts_with("%PDF-"));
    require(read(root / "reading/obra.epub").starts_with("PK"));
    require(read(root / "manuscript/obra.txt") == out.preview &&
            out.preview.find(first.content) != std::string::npos);
    if (argc == 3 && std::string(argv[1]) == "--emit") {
      using J = persistence::json::Value;
      J::Array units;
      for (const auto &u : out.units) {
        J::Array texts;
        for (const auto &d : u.documents)
          texts.emplace_back(d.content);
        units.emplace_back(
            J::Object{{"heading", u.heading},
                      {"title", u.node.title},
                      {"designator", u.node.designator},
                      {"depth", static_cast<std::int64_t>(u.depth)},
                      {"texts", std::move(texts)}});
      }
      std::ofstream expected(root / "expected.json", std::ios::binary);
      expected << persistence::json::serialize(
          J::Object{{"title", work.title},
                    {"subtitle", work.subtitle},
                    {"author", options.author},
                    {"language", work.language},
                    {"preview", out.preview},
                    {"excluded", alternate.content},
                    {"units", std::move(units)}},
          true);
    }
    std::cout << "PASS order, explicit alternatives, fail closed, Unicode "
                 "formatting, omissions, cancel, exclusive output, symlink "
                 "refusal, hashes/readback and both profiles\nOUTPUT "
              << root << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << std::endl;
    return 1;
  }
}
