#include "inde/application/project_service.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/manifest.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using inde::persistence::SqliteDatabase;
using inde::persistence::SqliteStatement;
using inde::persistence::SqliteTransaction;

constexpr std::string_view character_type =
    "00000000-0000-4000-9000-000000000001";
constexpr std::string_view location_type =
    "00000000-0000-4000-9000-000000000002";
constexpr std::string_view event_type = "00000000-0000-4000-9000-000000000003";
constexpr std::string_view information_type =
    "00000000-0000-4000-9000-000000000004";
constexpr std::string_view objective_type =
    "00000000-0000-4000-9000-000000000005";
constexpr std::string_view saga_type = "10000000-0000-4000-9000-000000000002";
constexpr std::string_view chapter_type =
    "10000000-0000-4000-9000-000000000007";
constexpr std::string_view scene_type = "10000000-0000-4000-9000-000000000009";

struct WorkSpec {
  std::string id;
  std::string editorial_structure_id;
  std::string narrative_structure_id;
  int groups{};
  int chapters_per_group{};
  int scenes_per_chapter{};
  std::string label;
};

struct NodeRecord {
  std::string id;
  std::string work_id;
  int ordinal{};
  bool scene{};
};

struct DocumentRecord {
  std::string id;
  std::string anchor_id;
  std::int64_t content_characters{};
};

std::string uuid() { return inde::project::new_uuid(); }

std::string one_text(SqliteDatabase &database, std::string_view sql,
                     std::string_view value) {
  auto statement = database.prepare(sql);
  statement.bind(1, value);
  if (!statement.step())
    throw std::runtime_error("consulta da fixture não retornou uma linha");
  return statement.column_text(0);
}

void run(SqliteStatement &statement) {
  statement.run();
  statement.reset();
}

std::string padded(int value, int width = 4) {
  std::ostringstream out;
  out.width(width);
  out.fill('0');
  out << value;
  return out.str();
}

std::string prose(int seed, int paragraphs, std::string_view strand) {
  static constexpr std::array<std::string_view, 12> subjects{
      "A cartógrafa",  "O vigia",     "A pesquisadora", "O mensageiro",
      "A conselheira", "O navegante", "A arquivista",   "O artesão",
      "A testemunha",  "O mediador",  "A exploradora",  "O cronista"};
  static constexpr std::array<std::string_view, 12> actions{
      "comparou os mapas de maré",        "reuniu os relatos contraditórios",
      "atravessou o corredor de estufas", "registrou a mudança do vento",
      "protegeu a mensagem cifrada",      "questionou a versão oficial",
      "seguiu as lanternas do cais",      "reconstruiu o mecanismo antigo",
      "ouviu a assembleia em silêncio",   "marcou a rota no vidro",
      "negociou uma passagem segura",     "observou o horizonte violeta"};
  static constexpr std::array<std::string_view, 12> consequences{
      "antes que a ponte fechasse",
      "enquanto a cidade mudava de turno",
      "sem revelar o nome da fonte",
      "para preservar a memória do bairro",
      "quando os sinos anunciaram neblina",
      "sob a luz baixa do observatório",
      "até que outra hipótese surgisse",
      "porque o arquivo estava incompleto",
      "e deixou uma pergunta para o amanhecer",
      "mas recusou a resposta fácil",
      "quando a multidão ocupou a praça",
      "antes da próxima corrente"};
  std::ostringstream out;
  out << "# " << strand << " — registro " << padded(seed) << "\n\n";
  for (int index = 0; index < paragraphs; ++index) {
    const auto a = static_cast<std::size_t>(seed + index) % subjects.size();
    const auto b =
        static_cast<std::size_t>(seed * 3 + index * 5) % actions.size();
    const auto c =
        static_cast<std::size_t>(seed * 7 + index * 2) % consequences.size();
    out << subjects[a] << ' ' << actions[b] << ' ' << consequences[c]
        << ". O registro " << padded(seed) << '.' << padded(index + 1, 2)
        << " relaciona uma decisão, uma presença e uma consequência "
           "observável. "
        << "Nenhuma conclusão é tratada como verdade apenas por ter sido "
           "repetida; "
        << "a equipe conserva a origem, o contexto e a dúvida que ainda "
           "precisa ser "
        << "resolvida.\n\n";
  }
  return out.str();
}

std::int64_t utf8_characters(std::string_view text) {
  std::int64_t count = 0;
  for (const unsigned char byte : text)
    if ((byte & 0xc0U) != 0x80U)
      ++count;
  return count;
}

class FixtureBuilder {
public:
  explicit FixtureBuilder(std::filesystem::path output)
      : output_(std::move(output)) {}

  void build() {
    if (std::filesystem::exists(output_))
      throw std::runtime_error("o destino já existe: " + output_.string());

    const auto started = std::chrono::steady_clock::now();
    create_container();
    populate();
    audit();
    refresh_manifest();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);
    std::cout << "Fixture colossal criada em " << output_ << " ("
              << elapsed.count() << " ms).\n";
  }

private:
  void create_container() {
    inde::application::ProjectService service;
    const auto &project = service.create(
        output_, "Arquipélago de Íris — Corpus Colossal Sintético");
    project_id_ = project.manifest().project_id;
    const auto &ip = service.create_intellectual_property(
        "Arquipélago de Íris", "Corpus procedural para testes de escala",
        "Universo ficcional inteiramente sintético criado para exercitar o "
        "INDE "
        "em grande volume, sem adaptar ou reproduzir uma obra existente.",
        "assets/covers/arquipelago-iris.svg");
    const auto &main = service.create_work(
        ip.id, "Os Mapas da Aurora Fraturada", "Linha principal",
        "Uma expedição civil tenta manter rotas, memória pública e decisões "
        "coerentes durante uma sequência de mudanças ambientais.",
        "pt-BR", "Teste de escala");
    const auto main_id = main.id;
    const auto &side = service.create_work(
        ip.id, "Crônicas das Margens", "Histórias paralelas",
        "Relatos laterais que cruzam locais, acontecimentos e informações da "
        "linha principal sem repetir sua ordem de revelação.",
        "pt-BR", "Teste de escala");
    const auto side_id = side.id;
    const auto &pov = service.create_work(
        ip.id, "Cadernos de Ponto de Vista", "PoVs complementares",
        "Documentos de perspectiva que exercitam organização editorial, "
        "referências e leitura descontínua.",
        "pt-BR", "Teste de escala");
    const auto pov_id = pov.id;
    service.save();
    service.close();

    std::filesystem::create_directories(output_ / "assets" / "covers");
    std::ofstream cover(output_ / "assets" / "covers" / "arquipelago-iris.svg");
    cover
        << R"(<svg xmlns="http://www.w3.org/2000/svg" width="900" height="1200"><rect width="900" height="1200" fill="#142b3b"/><circle cx="650" cy="280" r="170" fill="#e7b7ff"/><path d="M0 800 Q180 620 360 800 T720 800 T1080 800 V1200 H0Z" fill="#24798f"/><text x="70" y="1030" fill="#fff" font-family="serif" font-size="63">ARQUIPÉLAGO DE ÍRIS</text><text x="74" y="1100" fill="#c8e7ed" font-family="sans-serif" font-size="30">CORPUS SINTÉTICO DE ESCALA</text></svg>)";

    SqliteDatabase database(output_ / "data" / "project.sqlite3");
    works_.push_back(make_work(database, main_id, 9, 54, 3, "Principal"));
    works_.push_back(make_work(database, side_id, 3, 40, 2, "Paralela"));
    works_.push_back(make_work(database, pov_id, 4, 60, 1, "PoV"));
  }

  WorkSpec make_work(SqliteDatabase &database, const std::string &work_id,
                     int groups, int chapters, int scenes, std::string label) {
    return {work_id,
            one_text(database,
                     "SELECT id FROM editorial_structures WHERE work_id=? AND "
                     "is_active=1",
                     work_id),
            one_text(database,
                     "SELECT id FROM narrative_structures WHERE work_id=? AND "
                     "is_active=1",
                     work_id),
            groups,
            chapters,
            scenes,
            std::move(label)};
  }

  void populate() {
    SqliteDatabase database(output_ / "data" / "project.sqlite3");
    database.execute("PRAGMA synchronous = NORMAL");
    SqliteTransaction transaction(database);
    create_custom_types(database);
    create_alternative_structures(database);
    create_entities(database);
    create_time(database);
    create_relations(database);
    create_structures(database);
    create_narrative(database);
    create_documents(database);
    create_scope_and_editorial_references(database);
    create_history(database);
    transaction.commit();
    database.execute("PRAGMA optimize");
  }

  void create_alternative_structures(SqliteDatabase &database) {
    const auto now = inde::project::utc_now();
    auto editorial = database.prepare(
        "INSERT INTO "
        "editorial_structures(id,project_id,work_id,name,description,"
        "model_id,derived_from_id,creation_kind,is_active,created_at,updated_"
        "at) "
        "VALUES(?,?,?,?,?,?,?,?,0,?,?)");
    auto narrative = database.prepare(
        "INSERT INTO "
        "narrative_structures(id,project_id,work_id,name,description,"
        "model_id,derived_from_id,creation_kind,is_active,created_at,updated_"
        "at) "
        "VALUES(?,?,?,?,?,?,?,?,0,?,?)");
    for (const auto &work : works_) {
      editorial.bind(1, uuid());
      editorial.bind(2, project_id_);
      editorial.bind(3, work.id);
      editorial.bind(4, "Alternativa compacta " + work.label);
      editorial.bind(5,
                     "Estrutura inativa para testar biblioteca e comparação.");
      editorial.bind(6, "20000000-0000-4000-9000-000000000002");
      editorial.bind(7, work.editorial_structure_id);
      editorial.bind(8, "derived");
      editorial.bind(9, now);
      editorial.bind(10, now);
      run(editorial);
      narrative.bind(1, uuid());
      narrative.bind(2, project_id_);
      narrative.bind(3, work.id);
      narrative.bind(4, "Narrativa alternativa " + work.label);
      narrative.bind(5, "Estrutura inativa para testar biblioteca narrativa.");
      narrative.bind(6, "20000000-0000-4000-9000-000000000005");
      narrative.bind(7, work.narrative_structure_id);
      narrative.bind(8, "derived");
      narrative.bind(9, now);
      narrative.bind(10, now);
      run(narrative);
    }
  }

  void create_custom_types(SqliteDatabase &database) {
    const auto now = inde::project::utc_now();
    artifact_type_id_ = uuid();
    auto entity_type = database.prepare(
        "INSERT INTO "
        "entity_types(id,project_id,key,name,description,is_builtin,"
        "created_at,updated_at) VALUES(?,?,?,?,?,0,?,?)");
    entity_type.bind(1, artifact_type_id_);
    entity_type.bind(2, project_id_);
    entity_type.bind(3, "artifact");
    entity_type.bind(4, "Artefato");
    entity_type.bind(5, "Tipo do usuário usado pela fixture de escala.");
    entity_type.bind(6, now);
    entity_type.bind(7, now);
    run(entity_type);

    relation_type_ids_.clear();
    auto relation_type = database.prepare(
        "INSERT INTO "
        "relation_types(id,project_id,key,name,inverse_name,description,"
        "directionality,is_builtin,created_at,updated_at) "
        "VALUES(?,?,?,?,?,?,?,0,?,?)");
    const std::array<std::array<std::string_view, 5>, 6> specs{{
        {{"cooperates", "coopera com", "coopera com", "Cooperação operacional",
          "symmetric"}},
        {{"protects", "protege", "é protegido por", "Proteção declarada",
          "directed"}},
        {{"investigates", "investiga", "é investigado por", "Investigação",
          "directed"}},
        {{"reveals", "revela", "é revelado por", "Revelação", "directed"}},
        {{"opposes", "se opõe a", "se opõe a", "Oposição", "symmetric"}},
        {{"depends", "depende de", "sustenta", "Dependência", "directed"}},
    }};
    for (const auto &spec : specs) {
      const auto id = uuid();
      relation_type_ids_.push_back(id);
      relation_type.bind(1, id);
      relation_type.bind(2, project_id_);
      relation_type.bind(3, spec[0]);
      relation_type.bind(4, spec[1]);
      relation_type.bind(5, spec[2]);
      relation_type.bind(6, spec[3]);
      relation_type.bind(7, spec[4]);
      relation_type.bind(8, now);
      relation_type.bind(9, now);
      run(relation_type);
    }
  }

  void create_entities(SqliteDatabase &database) {
    const auto now = inde::project::utc_now();
    auto insert = database.prepare(
        "INSERT INTO "
        "entities(id,project_id,entity_type_id,name,summary,created_at,"
        "updated_at) VALUES(?,?,?,?,?,?,?)");
    auto add = [&](std::vector<std::string> &target, std::string_view type,
                   std::string_view label, int count) {
      for (int index = 1; index <= count; ++index) {
        const auto id = uuid();
        target.push_back(id);
        all_entities_.push_back(id);
        insert.bind(1, id);
        insert.bind(2, project_id_);
        insert.bind(3, type);
        insert.bind(4, std::string(label) + " " + padded(index));
        insert.bind(5, "Registro sintético " + padded(index) +
                           " com contexto, dúvida, função narrativa e termos "
                           "pesquisáveis em português.");
        insert.bind(6, now);
        insert.bind(7, now);
        run(insert);
      }
    };
    add(characters_, character_type, "Pessoa", 3000);
    add(locations_, location_type, "Local", 800);
    add(events_, event_type, "Acontecimento", 2500);
    add(information_, information_type, "Informação", 1200);
    add(objectives_, objective_type, "Objetivo", 500);
    add(artifacts_, artifact_type_id_, "Artefato", 1000);
    non_events_.insert(non_events_.end(), characters_.begin(),
                       characters_.end());
    non_events_.insert(non_events_.end(), locations_.begin(), locations_.end());
    non_events_.insert(non_events_.end(), information_.begin(),
                       information_.end());
    non_events_.insert(non_events_.end(), objectives_.begin(),
                       objectives_.end());
    non_events_.insert(non_events_.end(), artifacts_.begin(), artifacts_.end());
  }

  void create_time(SqliteDatabase &database) {
    const auto now = inde::project::utc_now();
    time_axis_id_ = one_text(database,
                             "SELECT id FROM fictional_time_axes WHERE "
                             "project_id=? AND is_default=1",
                             project_id_);
    auto point = database.prepare(
        "INSERT INTO fictional_time_points(id,project_id,axis_id,ordinal,label,"
        "description,created_at,updated_at) VALUES(?,?,?,?,?,?,?,?)");
    for (int index = 1; index <= 3000; ++index) {
      const auto id = uuid();
      time_points_.push_back(id);
      point.bind(1, id);
      point.bind(2, project_id_);
      point.bind(3, time_axis_id_);
      point.bind(4, static_cast<std::int64_t>(index * 1000));
      point.bind(5, "Ciclo " + padded(index));
      point.bind(6, "Marco ficcional sintético no eixo principal.");
      point.bind(7, now);
      point.bind(8, now);
      run(point);
    }

    auto occurrence = database.prepare(
        "INSERT INTO "
        "event_occurrences(id,project_id,event_entity_id,time_point_id,"
        "description,created_at,updated_at) VALUES(?,?,?,?,?,?,?)");
    auto participation = database.prepare(
        "INSERT INTO event_participations(id,project_id,event_occurrence_id,"
        "participant_entity_id,role,notes,created_at,updated_at) "
        "VALUES(?,?,?,?,?,?,?,?)");
    for (std::size_t index = 0; index < events_.size(); ++index) {
      const auto occurrence_id = uuid();
      occurrences_.push_back(occurrence_id);
      occurrence.bind(1, occurrence_id);
      occurrence.bind(2, project_id_);
      occurrence.bind(3, events_[index]);
      occurrence.bind(4, time_points_[index % time_points_.size()]);
      occurrence.bind(5, "Ocorrência sintética " +
                             padded(static_cast<int>(index + 1)));
      occurrence.bind(6, now);
      occurrence.bind(7, now);
      run(occurrence);
      for (int member = 0; member < 4; ++member) {
        participation.bind(1, uuid());
        participation.bind(2, project_id_);
        participation.bind(3, occurrence_id);
        participation.bind(
            4, non_events_[(index * 7 + member * 97) % non_events_.size()]);
        participation.bind(5, member == 0   ? "Agente"
                              : member == 1 ? "Testemunha"
                                            : "Participante");
        participation.bind(6, "Participação procedural com papel explícito.");
        participation.bind(7, now);
        participation.bind(8, now);
        run(participation);
      }
    }

    auto presence = database.prepare(
        "INSERT INTO "
        "entity_presences(id,project_id,entity_id,location_entity_id,"
        "start_time_point_id,end_time_point_id,description,created_at,updated_"
        "at) "
        "VALUES(?,?,?,?,?,?,?,?,?)");
    for (int index = 0; index < 12000; ++index) {
      const auto start =
          static_cast<std::size_t>(index * 13) % (time_points_.size() - 4);
      presence.bind(1, uuid());
      presence.bind(2, project_id_);
      presence.bind(3, characters_[static_cast<std::size_t>(index * 17) %
                                   characters_.size()]);
      presence.bind(
          4,
          locations_[static_cast<std::size_t>(index * 19) % locations_.size()]);
      presence.bind(5, time_points_[start]);
      presence.bind(6, time_points_[start + 3]);
      presence.bind(7, "Presença sintética em intervalo ficcional ordenado.");
      presence.bind(8, now);
      presence.bind(9, now);
      run(presence);
    }
  }

  void create_relations(SqliteDatabase &database) {
    const auto now = inde::project::utc_now();
    auto relation = database.prepare(
        "INSERT INTO relations(id,project_id,relation_type_id,source_entity_id,"
        "target_entity_id,description,created_at,updated_at) "
        "VALUES(?,?,?,?,?,?,?,?)");
    auto context = database.prepare(
        "INSERT INTO relation_contexts(relation_id,project_id,"
        "fictional_time_point_id,location_entity_id,cause_entity_id) "
        "VALUES(?,?,?,?,?)");
    for (int index = 0; index < 30000; ++index) {
      const auto id = uuid();
      const auto type_index =
          static_cast<std::size_t>(index) % relation_type_ids_.size();
      const auto per_type_index =
          static_cast<std::size_t>(index) / relation_type_ids_.size();
      const auto source =
          (per_type_index * 7 + type_index) % all_entities_.size();
      auto target = (source + 101 + type_index * 13) % all_entities_.size();
      if (target == source)
        target = (target + 1) % all_entities_.size();
      auto source_id = all_entities_[source];
      auto target_id = all_entities_[target];
      if ((type_index == 0 || type_index == 4) && source_id > target_id)
        std::swap(source_id, target_id);
      relation.bind(1, id);
      relation.bind(2, project_id_);
      relation.bind(3, relation_type_ids_[type_index]);
      relation.bind(4, source_id);
      relation.bind(5, target_id);
      relation.bind(6, "Relação sintética qualificada " + padded(index + 1, 5));
      relation.bind(7, now);
      relation.bind(8, now);
      run(relation);
      context.bind(1, id);
      context.bind(2, project_id_);
      context.bind(3, time_points_[static_cast<std::size_t>(index * 11) %
                                   time_points_.size()]);
      context.bind(
          4,
          locations_[static_cast<std::size_t>(index * 23) % locations_.size()]);
      context.bind(5, information_[static_cast<std::size_t>(index * 29) %
                                   information_.size()]);
      run(context);
    }
  }

  void create_structures(SqliteDatabase &database) {
    const auto now = inde::project::utc_now();
    auto node = database.prepare(
        "INSERT INTO editorial_nodes(id,work_id,parent_id,type,title,subtitle,"
        "synopsis,custom_type_name,status,position,created_at,updated_at,"
        "structural_type_id,designator,structure_id) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    int global = 0;
    for (const auto &work : works_) {
      for (int group = 1; group <= work.groups; ++group) {
        const auto root = uuid();
        insert_node(node, root, work, std::nullopt, "saga", saga_type,
                    std::to_string(group),
                    work.label + " — ciclo " + padded(group, 2), group * 1000,
                    now);
        for (int chapter = 1; chapter <= work.chapters_per_group; ++chapter) {
          const auto chapter_id = uuid();
          const auto chapter_number =
              (group - 1) * work.chapters_per_group + chapter;
          insert_node(node, chapter_id, work, root, "chapter", chapter_type,
                      std::to_string(chapter_number),
                      "Convergência " + padded(chapter_number), chapter * 1000,
                      now);
          nodes_.push_back({chapter_id, work.id, ++global, false});
          for (int scene = 1; scene <= work.scenes_per_chapter; ++scene) {
            const auto scene_id = uuid();
            insert_node(node, scene_id, work, chapter_id, "scene", scene_type,
                        std::to_string(scene), "Movimento " + padded(scene, 2),
                        scene * 1000, now);
            nodes_.push_back({scene_id, work.id, ++global, true});
          }
        }
      }
    }
  }

  void insert_node(SqliteStatement &node, const std::string &id,
                   const WorkSpec &work,
                   const std::optional<std::string> &parent,
                   std::string_view legacy_type,
                   std::string_view structural_type,
                   const std::string &designator, const std::string &title,
                   int position, const std::string &now) {
    node.bind(1, id);
    node.bind(2, work.id);
    if (parent)
      node.bind(3, *parent);
    else
      node.bind_null(3);
    node.bind(4, legacy_type);
    node.bind(5, title);
    node.bind(6, "");
    node.bind(7, "Posição editorial sintética para teste de escala.");
    node.bind(8, "");
    node.bind(9, "Planejamento");
    node.bind(10, position);
    node.bind(11, now);
    node.bind(12, now);
    node.bind(13, structural_type);
    node.bind(14, designator);
    node.bind(15, work.editorial_structure_id);
    run(node);
  }

  void create_narrative(SqliteDatabase &database) {
    const auto now = inde::project::utc_now();
    auto line = database.prepare(
        "INSERT INTO "
        "narrative_lines(id,project_id,structure_id,name,description,"
        "position,created_at,updated_at) VALUES(?,?,?,?,?,?,?,?)");
    auto unit = database.prepare(
        "INSERT INTO "
        "narrative_units(id,project_id,structure_id,designator,title,"
        "summary,purpose,perspective,position,created_at,updated_at) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?)");
    auto membership = database.prepare(
        "INSERT INTO narrative_unit_lines(project_id,unit_id,line_id,position) "
        "VALUES(?,?,?,?)");
    auto unit_entity = database.prepare(
        "INSERT INTO "
        "narrative_unit_entities(project_id,unit_id,entity_id,role) "
        "VALUES(?,?,?,?)");
    auto link = database.prepare(
        "INSERT INTO narrative_links(id,project_id,structure_id,source_unit_id,"
        "target_unit_id,kind,label,created_at,updated_at) "
        "VALUES(?,?,?,?,?,?,?,?,?)");

    for (const auto &work : works_) {
      std::vector<std::string> lines;
      for (int index = 0; index < 8; ++index) {
        const auto id = uuid();
        lines.push_back(id);
        line.bind(1, id);
        line.bind(2, project_id_);
        line.bind(3, work.narrative_structure_id);
        line.bind(4, "Linha " + padded(index + 1, 2));
        line.bind(5, "Fluxo narrativo sintético para comparação.");
        line.bind(6, static_cast<std::int64_t>((index + 1) * 1000));
        line.bind(7, now);
        line.bind(8, now);
        run(line);
      }
      const int unit_count = work.groups * work.chapters_per_group * 2;
      std::string previous;
      for (int index = 0; index < unit_count; ++index) {
        const auto id = uuid();
        unit.bind(1, id);
        unit.bind(2, project_id_);
        unit.bind(3, work.narrative_structure_id);
        unit.bind(4, std::to_string(index + 1));
        unit.bind(5, work.label + " — unidade " + padded(index + 1));
        unit.bind(6, "Mudança de estado, consequência e informação revelada.");
        unit.bind(7, index % 3 == 0   ? "Preparar"
                     : index % 3 == 1 ? "Confrontar"
                                      : "Resolver");
        unit.bind(8, "Pessoa " + padded((index * 17) % 3000 + 1));
        unit.bind(9, static_cast<std::int64_t>((index + 1) * 1000));
        unit.bind(10, now);
        unit.bind(11, now);
        run(unit);
        for (int member = 0; member < 2; ++member) {
          membership.bind(1, project_id_);
          membership.bind(2, id);
          membership.bind(
              3,
              lines[static_cast<std::size_t>(index + member) % lines.size()]);
          membership.bind(
              4, static_cast<std::int64_t>((index + 1) * 1000 + member));
          run(membership);
        }
        for (int member = 0; member < 4; ++member) {
          unit_entity.bind(1, project_id_);
          unit_entity.bind(2, id);
          unit_entity.bind(3, all_entities_[static_cast<std::size_t>(
                                                index * 31 + member * 101) %
                                            all_entities_.size()]);
          unit_entity.bind(4, member == 0 ? "Ponto de vista" : "Participante");
          run(unit_entity);
        }
        if (!previous.empty()) {
          link.bind(1, uuid());
          link.bind(2, project_id_);
          link.bind(3, work.narrative_structure_id);
          link.bind(4, previous);
          link.bind(5, id);
          link.bind(6, index % 5 == 0 ? "causes" : "precedes");
          link.bind(7, "Encadeamento sintético");
          link.bind(8, now);
          link.bind(9, now);
          run(link);
        }
        previous = id;
      }
    }

    static constexpr std::array<std::string_view, 5> role_ids{
        "22000000-0000-4000-9000-000000000001",
        "22000000-0000-4000-9000-000000000002",
        "22000000-0000-4000-9000-000000000003",
        "22000000-0000-4000-9000-000000000004",
        "22000000-0000-4000-9000-000000000005"};
    auto assignment = database.prepare(
        "INSERT INTO "
        "narrative_role_assignments(id,project_id,role_id,entity_id,"
        "work_id,structure_id,unit_id,notes,created_at,updated_at) "
        "VALUES(?,?,?,?,?,?,NULL,?,?,?)");
    for (int index = 0; index < 600; ++index) {
      const auto &work =
          works_[static_cast<std::size_t>(index) % works_.size()];
      assignment.bind(1, uuid());
      assignment.bind(2, project_id_);
      assignment.bind(
          3, role_ids[static_cast<std::size_t>(index) % role_ids.size()]);
      assignment.bind(4, characters_[static_cast<std::size_t>(index * 13) %
                                     characters_.size()]);
      assignment.bind(5, work.id);
      assignment.bind(6, work.narrative_structure_id);
      assignment.bind(7,
                      "Atribuição contextual sintética para teste de escala.");
      assignment.bind(8, now);
      assignment.bind(9, now);
      run(assignment);
    }
  }

  void create_documents(SqliteDatabase &database) {
    const auto now = inde::project::utc_now();
    auto group = database.prepare(
        "INSERT INTO document_groups(id,project_id,name,description,created_at,"
        "updated_at) VALUES(?,?,?,?,?,?)");
    const std::array<std::string_view, 6> group_names{
        "Manuscrito principal", "Histórias paralelas", "Pontos de vista",
        "Planejamento",         "Pesquisa sintética",  "Revisões"};
    for (const auto name : group_names) {
      const auto id = uuid();
      document_groups_.push_back(id);
      group.bind(1, id);
      group.bind(2, project_id_);
      group.bind(3, name);
      group.bind(4, "Agrupamento documental da fixture colossal.");
      group.bind(5, now);
      group.bind(6, now);
      run(group);
    }
    auto document = database.prepare(
        "INSERT INTO documents(id,project_id,editorial_node_id,title,content,"
        "created_at,updated_at,word_goal,group_id,purpose,revision_of_id,"
        "revision_label,perspective) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)");
    auto format = database.prepare(
        "INSERT INTO document_format_spans(project_id,document_id,style,"
        "start_offset,end_offset) VALUES(?,?,?,?,?)");
    auto anchor = database.prepare(
        "INSERT INTO "
        "document_anchors(id,project_id,document_id,label,start_offset,"
        "end_offset) VALUES(?,?,?,?,?,?)");
    auto reference = database.prepare(
        "INSERT INTO "
        "document_entity_references(id,project_id,document_id,entity_id,"
        "anchor_id,notes) VALUES(?,?,?,?,?,?)");

    int seed = 0;
    for (const auto &node : nodes_) {
      const auto content = prose(++seed, node.scene ? 34 : 8,
                                 node.scene ? "Cena" : "Plano de capítulo");
      const auto id = uuid();
      const auto anchor_id = uuid();
      insert_document(document, id, node.id,
                      (node.scene ? "Texto " : "Plano ") + padded(node.ordinal),
                      content, document_groups_[node.scene ? 0 : 3],
                      node.scene ? "main-text" : "outline", std::nullopt, "",
                      "Pessoa " + padded((seed * 17) % 3000 + 1), now);
      const auto characters = utf8_characters(content);
      format.bind(1, project_id_);
      format.bind(2, id);
      format.bind(3, "heading");
      format.bind(4, 0);
      format.bind(5, std::min<std::int64_t>(40, characters));
      run(format);
      format.bind(1, project_id_);
      format.bind(2, id);
      format.bind(3, "italic");
      format.bind(4, std::min<std::int64_t>(60, characters - 2));
      format.bind(5, std::min<std::int64_t>(150, characters));
      run(format);
      anchor.bind(1, anchor_id);
      anchor.bind(2, project_id_);
      anchor.bind(3, id);
      anchor.bind(4, "Passagem-chave " + padded(seed));
      anchor.bind(5, std::min<std::int64_t>(60, characters));
      anchor.bind(6, std::min<std::int64_t>(240, characters));
      run(anchor);
      for (int entity = 0; entity < 4; ++entity) {
        reference.bind(1, uuid());
        reference.bind(2, project_id_);
        reference.bind(3, id);
        reference.bind(
            4,
            all_entities_[static_cast<std::size_t>(seed * 43 + entity * 211) %
                          all_entities_.size()]);
        if (entity == 0)
          reference.bind(5, anchor_id);
        else
          reference.bind_null(5);
        reference.bind(
            6, "Referência explícita gerada para busca e continuidade.");
        run(reference);
      }
      documents_.push_back({id, anchor_id, characters});
      if (node.scene && seed % 4 == 0) {
        const auto revision = uuid();
        const auto revised =
            content + "Nota de revisão: a consequência foi reavaliada sem "
                      "apagar a versão anterior.\n";
        insert_document(document, revision, node.id,
                        "Revisão " + padded(node.ordinal), revised,
                        document_groups_[5], "revision", id, "R2",
                        "Pessoa " + padded((seed * 17) % 3000 + 1), now);
        documents_.push_back({revision, "", utf8_characters(revised)});
      }
    }
    for (int index = 1; index <= 120; ++index) {
      const auto content = prose(++seed, 18, "Dossiê de pesquisa");
      const auto id = uuid();
      insert_document(
          document, id, std::nullopt, "Dossiê sintético " + padded(index),
          content, document_groups_[4], "research", std::nullopt, "", "", now);
      documents_.push_back({id, "", utf8_characters(content)});
    }
  }

  void insert_document(SqliteStatement &statement, const std::string &id,
                       const std::optional<std::string> &node,
                       const std::string &title, const std::string &content,
                       const std::string &group, std::string_view purpose,
                       const std::optional<std::string> &revision,
                       std::string_view revision_label,
                       const std::string &perspective, const std::string &now) {
    statement.bind(1, id);
    statement.bind(2, project_id_);
    if (node)
      statement.bind(3, *node);
    else
      statement.bind_null(3);
    statement.bind(4, title);
    statement.bind(5, content);
    statement.bind(6, now);
    statement.bind(7, now);
    statement.bind(8, 1800);
    statement.bind(9, group);
    statement.bind(10, purpose);
    if (revision)
      statement.bind(11, *revision);
    else
      statement.bind_null(11);
    statement.bind(12, revision_label);
    statement.bind(13, perspective);
    run(statement);
  }

  void create_scope_and_editorial_references(SqliteDatabase &database) {
    const auto now = inde::project::utc_now();
    auto scope = database.prepare(
        "INSERT INTO entity_work_scopes(id,project_id,entity_id,work_id,notes,"
        "created_at,updated_at) VALUES(?,?,?,?,?,?,?)");
    for (std::size_t index = 0; index < all_entities_.size(); ++index) {
      const auto &work = works_[index % works_.size()];
      scope.bind(1, uuid());
      scope.bind(2, project_id_);
      scope.bind(3, all_entities_[index]);
      scope.bind(4, work.id);
      scope.bind(5, "Escopo sintético explícito.");
      scope.bind(6, now);
      scope.bind(7, now);
      run(scope);
    }
    auto editorial = database.prepare(
        "INSERT INTO "
        "editorial_entity_references(id,project_id,entity_id,work_id,"
        "editorial_node_id,purpose,notes,created_at,updated_at) "
        "VALUES(?,?,?,?,?,?,?,?,?)");
    for (int index = 0; index < 6000; ++index) {
      const auto entity_index =
          static_cast<std::size_t>(index * 17) % all_entities_.size();
      const auto &work = works_[entity_index % works_.size()];
      const NodeRecord *chosen = nullptr;
      for (std::size_t attempt = 0; attempt < nodes_.size(); ++attempt) {
        const auto &candidate =
            nodes_[(static_cast<std::size_t>(index * 31) + attempt) %
                   nodes_.size()];
        if (candidate.work_id == work.id) {
          chosen = &candidate;
          break;
        }
      }
      if (!chosen)
        throw std::runtime_error("não foi possível localizar nó do escopo");
      editorial.bind(1, uuid());
      editorial.bind(2, project_id_);
      editorial.bind(3, all_entities_[entity_index]);
      editorial.bind(4, work.id);
      editorial.bind(5, chosen->id);
      editorial.bind(6, "Contexto " + std::to_string(index % 3 + 1));
      editorial.bind(7, "Apresentação editorial sintética.");
      editorial.bind(8, now);
      editorial.bind(9, now);
      run(editorial);
    }
  }

  void create_history(SqliteDatabase &database) {
    const auto now = inde::project::utc_now();
    auto history = database.prepare(
        "INSERT INTO change_log(id,project_id,command_name,system_created_at) "
        "VALUES(?,?,?,?)");
    for (int index = 1; index <= 1000; ++index) {
      history.bind(1, uuid());
      history.bind(2, project_id_);
      history.bind(3, "fixture.synthetic.batch." + padded(index));
      history.bind(4, now);
      run(history);
    }
  }

  void audit() {
    SqliteDatabase database(output_ / "data" / "project.sqlite3");
    const std::array<std::pair<std::string_view, std::int64_t>, 22> minimums{{
        {"entities", 9000},
        {"relations", 30000},
        {"relation_contexts", 30000},
        {"fictional_time_points", 3000},
        {"event_occurrences", 2500},
        {"event_participations", 10000},
        {"entity_presences", 12000},
        {"editorial_nodes", 2800},
        {"documents", 3200},
        {"document_format_spans", 5568},
        {"document_anchors", 2784},
        {"document_entity_references", 11136},
        {"entity_work_scopes", 9000},
        {"editorial_entity_references", 6000},
        {"narrative_lines", 24},
        {"narrative_units", 1692},
        {"narrative_unit_entities", 6768},
        {"narrative_unit_lines", 3384},
        {"narrative_links", 1689},
        {"narrative_role_assignments", 600},
        {"editorial_structures", 6},
        {"narrative_structures", 6},
    }};
    for (const auto &[table, minimum] : minimums) {
      const auto count =
          database.query_integer("SELECT count(*) FROM " + std::string(table));
      if (count < minimum)
        throw std::runtime_error("auditoria falhou em " + std::string(table));
      std::cout << table << ": " << count << '\n';
    }
    if (database.query_text("PRAGMA integrity_check") != "ok")
      throw std::runtime_error("PRAGMA integrity_check falhou");
    if (database.query_integer(
            "SELECT count(*) FROM pragma_foreign_key_check") != 0)
      throw std::runtime_error("foreign_key_check encontrou violações");
    if (database.query_integer(
            "SELECT count(*) FROM documents WHERE length(content)=0") != 0)
      throw std::runtime_error("há documentos vazios na fixture colossal");
    const auto text_bytes = database.query_integer(
        "SELECT COALESCE(sum(length(CAST(content AS BLOB))),0) FROM documents");
    if (text_bytes < 20 * 1024 * 1024)
      throw std::runtime_error("o corpus textual ficou abaixo de 20 MiB");
    std::cout << "texto_documental_bytes: " << text_bytes << '\n';
    std::cout << "integrity_check: ok\nforeign_key_check: 0\n";
  }

  void refresh_manifest() {
    inde::application::ProjectService service;
    service.open(output_);
    service.save();
    service.close();
  }

  std::filesystem::path output_;
  std::string project_id_;
  std::string artifact_type_id_;
  std::string time_axis_id_;
  std::vector<WorkSpec> works_;
  std::vector<std::string> relation_type_ids_;
  std::vector<std::string> characters_, locations_, events_, information_;
  std::vector<std::string> objectives_, artifacts_, all_entities_, non_events_;
  std::vector<std::string> time_points_, occurrences_, document_groups_;
  std::vector<NodeRecord> nodes_;
  std::vector<DocumentRecord> documents_;
};

} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 2) {
      std::cerr << "Uso: inde_colossal_fixture_generator DESTINO.inde\n";
      return 2;
    }
    FixtureBuilder(argv[1]).build();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Falha ao gerar fixture colossal: " << error.what() << '\n';
    return 1;
  }
}
