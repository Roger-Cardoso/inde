#include "comic_service.hpp"
#include "inde/project/manifest.hpp"
#include <fstream>
#include <iostream>
#include <limits>
namespace fs = std::filesystem;
using namespace inde;
std::string read(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
int count = 0;
void check(bool ok, const char *label) {
  if (!ok)
    throw std::runtime_error(label);
  ++count;
}
template <class F> void rejects(F fn, const char *label) {
  bool failed = false;
  try {
    fn();
  } catch (const std::exception &) {
    failed = true;
  }
  check(failed, label);
}
std::string decoded(const publication::V &v, const std::string &name) {
  comic::Asset a;
  a.data = v.at("files").at(name).as_string();
  return comic::raw_asset(a);
}
int main(int argc, char **argv) {
  try {
    check(argc == 3, "Uso: inde_comic_tests PASTA_NOVA RECURSOS");
    const fs::path root = argv[1], resources = argv[2];
    check(!fs::exists(root), "Use pasta nova");
    fs::create_directories(root);
    setenv("XDG_CONFIG_HOME", (root / "perfil").c_str(), 1);
    application::ProjectService source;
    source.create(root / "Comic.inde", "Laboratório comic sintético");
    auto work = source.start_work("A passagem", "", "Universo de demonstração");
    source.save();
    publication::Service pub(source);
    comic::Service service(pub);
    check(service.books().empty() && !fs::exists(service.database_path()),
          "Consultar criou banco comic");
    const auto original =
        publication::hash(read(root / "Comic.inde/data/project.sqlite3"));
    publication::Site site;
    site.name = "Biblioteca das passagens";
    site.author = "Autoria de demonstração";
    site.description = "Um Site, diferentes mídias";
    site.base_path = "/biblioteca";
    site = pub.create_site(site);
    auto b = comic::example(work.id);
    const auto before = comic::encode(b);
    b = comic::decode(before);
    check(persistence::json::serialize(comic::encode(b), false) ==
              persistence::json::serialize(before, false),
          "Modelo perdeu dados");
    auto bad = b;
    bad.lang = "\"><script>";
    rejects([&] { comic::validate(bad); }, "Idioma injetável aceito");
    bad = b;
    bad.panels[0].elements[0].x = std::numeric_limits<double>::quiet_NaN();
    rejects([&] { comic::validate(bad); }, "NaN aceito");
    bad = b;
    bad.panels[0].elements[0].w = 100;
    rejects([&] { comic::validate(bad); }, "Elemento fora do quadro aceito");
    bad = b;
    bad.panels[1].id = bad.panels[0].id;
    rejects([&] { comic::validate(bad); }, "IDs repetidos aceitos");
    bad = b;
    bad.panels[0].image = "ausente";
    rejects([&] { comic::validate(bad); }, "Recurso ausente aceito");
    bad = b;
    bad.work_id = "outra-obra";
    rejects([&] { service.save(bad, 0); }, "Obra externa aceita");
    bad = b;
    bad.slug = "../home";
    rejects([&] { comic::validate(bad); }, "Slug inseguro aceito");
    auto e = b.panels[0].elements[0];
    auto position = comic::snap(e, 50 - e.w / 2 + 1, 50 - e.h / 2 - 1);
    check(position.snap_x && position.snap_y && position.x == 50 - e.w / 2,
          "Snap central incorreto");
    position = comic::snap(e, -10, 200);
    check(position.x == 0 && position.y == 100 - e.h,
          "Arraste escapou do quadro");
    auto node = source.create_structural_node(
        work.id, {}, project::StructuralNodeType::Chapter, "Origem");
    auto doc = source.writing().create_document("Texto selecionado", node.id);
    doc.content = "Uma fala copiada conscientemente.";
    doc = source.writing().update_document(doc);
    source.save();
    const auto source_after_document =
        publication::hash(read(root / "Comic.inde/data/project.sqlite3"));
    auto copied = comic::copy_document(doc);
    check(copied.text == doc.content && copied.source_document == doc.id &&
              copied.source_hash == publication::hash(doc.content),
          "Cópia perdeu proveniência");
    b.panels[0].elements.push_back(copied);
    b.panels[0].elements[0].text = "<script>alert('x')</script> & fala";
    // A real raster and a real local font exercise binary resources without
    // network.
    comic::Asset pixel;
    pixel.id = project::new_uuid();
    pixel.name = "pixel.png";
    pixel.mime = "image/png";
    pixel.data = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAAC0lEQVR4nGNgA"
                 "AIAAAUAAXpeqz8AAAAASUVORK5CYII=";
    b.assets[pixel.id] = pixel;
    b.panels[0].image = pixel.id;
    const fs::path font =
        "/usr/share/fonts/truetype/noto/NotoNaskhArabic-Regular.ttf";
    if (fs::exists(font)) {
      auto asset = comic::import_asset(font, true);
      asset.role = "ar";
      b.assets[asset.id] = asset;
      b.panels[1].elements[0].font_asset = asset.id;
    }
    b = service.save(b, 0);
    check(b.revision == 1, "Revisão inicial incorreta");
    rejects([&] { service.save(b, 0); },
            "Comic existente sobrescrito como novo");
    auto stale = b;
    b.title = "A passagem · comic nativo";
    b = service.save(b, b.revision);
    rejects([&] { service.save(stale, stale.revision); },
            "Revisão obsoleta aceita");
    auto prepared =
        service.prepare(b.id, site.id, b.revision, site.revision, resources);
    check(service.is_current(prepared), "Preparação nova obsoleta");
    auto payload = persistence::json::parse(prepared.payload);
    const auto page = decoded(payload, b.slug + "/index.html"),
               plain = decoded(payload, b.slug + "/texto.html");
    check(page.find("&lt;script&gt;") != std::string::npos &&
              page.find("<script>alert") == std::string::npos,
          "Texto ativo vazou no HTML");
    check(page.find("lang=\"ar\" dir=\"auto\" translate=\"yes\"") !=
              std::string::npos,
          "Metadados de idioma ausentes");
    check(page.find("Som grave do portal.") != std::string::npos,
          "Legenda não exportada");
    check(plain.find("<script") == std::string::npos &&
              plain.find("تستمر القصة") != std::string::npos,
          "Texto contínuo incompleto");
    check(prepared.payload.find("source_document") == std::string::npos &&
              prepared.payload.find("source_hash") == std::string::npos &&
              prepared.payload.find("data/project.sqlite3") ==
                  std::string::npos,
          "Proveniência privada vazou na saída");
    auto material = service.materialize(prepared);
    check(read(material) == prepared.payload, "Materialização divergente");
    rejects([&] { service.materialize(prepared); },
            "Materialização sobrescrita");
    fs::copy_file(material, root / "comic-preparado.json");
    auto tampered = prepared;
    tampered.payload += " ";
    tampered.payload_hash = publication::hash(tampered.payload);
    check(!service.is_current(tampered), "Preparação adulterada aceita");
    b.panels[0].elements[0].text = "Uma revisão posterior.";
    b = service.save(b, b.revision);
    check(!service.is_current(prepared),
          "Texto alterado não invalidou preparação");
    prepared =
        service.prepare(b.id, site.id, b.revision, site.revision, resources);
    site.name = "Biblioteca revisada";
    site = pub.update_site(site, site.revision);
    check(!service.is_current(prepared),
          "Site alterado não invalidou preparação");
    const auto id = b.id;
    b = service.retire(id, b.revision, true);
    check(service.books().empty() && service.books(true).size() == 1,
          "Retirada perdeu recuperação");
    rejects([&] { service.save(b, b.revision); }, "Comic retirado editado");
    b = service.retire(id, b.revision, false);
    check(b.id == id && b.panels.size() == 3,
          "Restauração perdeu identidade ou conteúdo");
    check(publication::hash(read(root / "Comic.inde/data/project.sqlite3")) ==
              source_after_document,
          "Comic alterou banco autoral");
    check(original != source_after_document,
          "Fixture de Documento não foi criada");
    source.close();
    source.open(root / "Comic.inde");
    check(service.book(id).revision == b.revision, "Reabertura perdeu comic");
    const auto previous = source.current()->manifest().project_id;
    source.save_as(root / "Copia.inde");
    rejects([&] { service.books(); }, "Cópia adotada implicitamente");
    pub.adopt_project_copy(previous);
    service.adopt_project_copy(previous);
    check(service.book(id).id == id, "Salvar cópia perdeu comic");
    check(!service.is_current(prepared), "Cópia manteve preparação atual");
    source.close();
    source.open(root / "Comic.inde");
    check(service.book(id).revision == b.revision, "Cópia alterou origem");
    persistence::SqliteDatabase db(service.database_path());
    check(db.query_text("PRAGMA integrity_check") == "ok" &&
              db.query_integer("SELECT count(*) FROM history") >= 5,
          "Histórico ou integridade ausentes");
    // A final public snapshot is independent of both SQLite files and source
    // paths.
    prepared =
        service.prepare(b.id, site.id, b.revision, site.revision, resources);
    std::ofstream(root / "comic-atual.json") << prepared.payload;
    std::cout << "COMIC-01: " << count << " verificações nativas aprovadas\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
