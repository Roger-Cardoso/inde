#include "publication_service.hpp"
#include "inde/application/contextual_planning.hpp"
#include "inde/application/document_alternatives.hpp"
#include "inde/application/document_visibility.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/project/document_state.hpp"
#include "inde/ui/text_coordinates.hpp"
#include <fstream>
#include <iostream>
using namespace inde;
using namespace inde::publication;
namespace fs = std::filesystem;
namespace {
void check(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
}
}
int main(int argc, char **argv) {
  try {
    check(argc == 2, "Informe uma cópia sintética de fixture schema28");
    check(ui::gtk_text_offset(-1) == -1, "Sentinela GTK alterada");
    bool overflow_refused = false;
    try { (void)ui::gtk_text_offset(std::size_t(std::numeric_limits<int>::max()) + 1); }
    catch (const std::exception &) { overflow_refused = true; }
    check(overflow_refused, "Posição textual truncada para GTK");
    check(ui::gtk_pixel(1e100) == std::numeric_limits<int>::max(), "Coordenada não limitada");
    const fs::path root = argv[1];
    application::ProjectService source;
    source.open(root / "Publicacao.inde");
    check(persistence::current_database_schema_version == 37, "Schema integrado");
    const auto work = source.catalog().works.front().id;
    auto nodes = application::production_order(source.structural_nodes_for_work(work));
    check(!nodes.empty(), "Fixture sem capítulos");
    auto document = source.writing().create_document("Integração — redação escolhida", nodes.front().id);
    const std::string current = "PUBLICO CORRENTE\nCORPO INTEGRAL\nFIM";
    const std::string secret = "ALTERNATIVA PRIVADA INT01";
    document.content = current;
    document.paragraphs = project::identify_paragraphs(current);
    auto state = application::create_alternative({document, {0, 0}}, 0, 16);
    const auto rid = state.document.resmas.front().id;
    const auto original = state.document.resmas.front().leaves.front().id;
    state = application::replace_selection(state, 0, 0, secret, rid);
    state = application::select_document_leaf(state, rid, original);
    check(state.document.content == current, "Redação escolhida mudou");
    project::VisibilityProfile profile{project::new_uuid(), "Primeiro parágrafo", false, {}, {}};
    profile.include.push_back({project::VisibilityRuleKind::Paragraph,
                              state.document.paragraphs.rows.front().id});
    state.document.visibility_profiles.push_back(profile);
    check(application::project_visibility(state.document, profile.id).text.find("CORPO INTEGRAL") == std::string::npos,
          "Perfil de trabalho não omitiu o trecho");
    document = source.writing().update_document(state.document);
    application::ContextualPlanningService planning(source);
    const auto plan = planning.create_plan(work, "Plano integrado");
    const auto part = planning.create_moment(planning.snapshot(plan), "Descoberta", "Uma situação explícita");
    const auto observation = planning.text_document(document.id);
    (void)planning.associate_text(planning.snapshot(plan), part, observation);
    check(planning.snapshot(plan).text_links.size() == 1, "Vínculo contextual perdido");
    auto second = planning.create_plan(work, "Outra composição");
    const auto moment = planning.moment(planning.snapshot(plan).participations.front().moment_id);
    (void)planning.reuse_moment(planning.snapshot(second), moment);
    check(planning.moment(moment.id).uses.size() == 2, "Reuso duplicou identidade");
    // A usable synthetic composition also exercises explicit local choices.
    application::ContextualVisualItem placement;
    placement.x = 40; placement.y = 200;
    const auto arrival = planning.create_moment(planning.snapshot(plan),
        "Chegada ao farol", "Mara encontra a luz interrompida.", &placement);
    placement.x = 380;
    const auto passage = planning.create_moment(planning.snapshot(plan),
        "Investigar a passagem", "Uma possibilidade: atravessar antes da maré.", &placement);
    placement.y = 420;
    const auto shore = planning.create_moment(planning.snapshot(plan),
        "Esperar na margem", "Outra possibilidade: observar o movimento da água.", &placement);
    const auto slot = planning.create_slot(planning.snapshot(plan), "Como continuar");
    const auto active = planning.create_option(planning.snapshot(plan), slot, "Atravessar");
    const auto alternate = planning.create_option(planning.snapshot(plan), slot, "Observar");
    planning.assign_option(planning.snapshot(plan), passage, active);
    planning.assign_option(planning.snapshot(plan), shore, alternate);
    planning.choose_option(planning.snapshot(plan), slot, active);
    planning.sequence_selection(planning.snapshot(plan), {arrival, part, passage}, true, false);
    check(planning.snapshot(plan).options.size() == 2, "Escolhas contextuais perdidas");
    Service service(source);
    Site site;
    site.name = "Biblioteca integrada";
    site.author = "Demonstração";
    site.description = "Planejamento, escrita e publicação integrados";
    site = service.create_site(site);
    Edition edition;
    edition.site_id = site.id;
    edition.work_id = work;
    edition.structure_id = nodes.front().structure_id;
    edition.title = "Edição integral corrente";
    edition.slug = "integracao";
    edition.choices[nodes.front().id].documents.push_back(document.id);
    edition = service.create_edition(edition);
    site.catalog = {edition.id};
    site = service.update_site(site, site.revision);
    auto prepared = service.prepare(edition.id, site.revision, edition.revision);
    check(service.is_current(prepared), "Preparação nova obsoleta");
    auto payload = persistence::json::parse(prepared.payload);
    const auto &snapshot = payload.at("snapshot");
    const auto &projected = snapshot.at("units").as_array().front().at("documents").as_array().front();
    check(snapshot.at("source_schema").as_integer() == 37, "Schema do retrato incorreto");
    check(projected.at("text").as_string() == current, "Perfil de trabalho amputou a publicação");
    check(prepared.payload.find(secret) == std::string::npos, "Alternativa privada vazou");
    check(projected.as_object().find("resmas") == projected.as_object().end(), "Modelo privado exportado");
    check(snapshot.at("source_production_notes").as_array().size() >= 2, "Limites não explicados");
    std::ofstream(root / "integracao-para-gerar.json") << prepared.payload;
    source.close();
    source.open(root / "Publicacao.inde");
    check(project::same_document_state(*source.writing().document(document.id), document), "Reabertura perdeu alternativas/perfis");
    check(planning.snapshot(plan).text_links.size() == 1, "Reabertura perdeu planejamento");
    const auto previous_owner = source.current()->manifest().project_id;
    source.save_as(root / "Copia-integrada.inde");
    service.adopt_project_copy(previous_owner);
    check(source.writing().document(document.id)->resmas.size() == 1, "Salvar cópia perdeu alternativas");
    check(source.writing().document(document.id)->visibility_profiles.size() == 1, "Salvar cópia perdeu perfis");
    std::cout << "INT01: migração, alternativas, perfil, reuso contextual, publicação integral, privacidade, reabertura e cópia OK\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
