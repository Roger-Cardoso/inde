#include "inde/persistence/json.hpp"
#include "project_file.hpp"
#include "project_file_transport.hpp"
#include <charconv>
#include <fstream>
#include <glib.h>
#include <iostream>
using namespace inde::project_file;
using V = inde::persistence::json::Value;
namespace {
std::int64_t number(const char *s) {
  std::int64_t n{};
  const std::string value(s);
  const auto r = std::from_chars(value.data(), value.data() + value.size(), n);
  if (r.ec != std::errc{} || r.ptr != value.data() + value.size() || n < 0)
    throw Error("invalid_argument", "Número inteiro não negativo esperado.");
  return n;
}
void fields(const V &v, std::initializer_list<const char *> names) {
  if (v.as_object().size() != names.size())
    throw Error("request_fields", "Campos inesperados no pedido.");
  for (const auto *n : names)
    static_cast<void>(v.at(n));
}
V summary(const State &s, bool content = false) {
  V::Array records;
  for (const auto &[key, r] : s.records) {
    V::Array refs;
    for (const auto &ref : r.references)
      refs.emplace_back(ref);
    V::Object row{{"key", key},
                  {"kind", r.kind},
                  {"version", r.version},
                  {"blob", r.blob},
                  {"references", refs}};
    if (content)
      row["payload"] = parse_json(r.payload);
    records.emplace_back(row);
  }
  return V::Object{{"format", "INDE-P5-1"},  {"project_id", s.project_id},
                   {"store_id", s.store_id}, {"sequence", s.sequence},
                   {"hash", s.hash},         {"records", records}};
}
void print(const V &v) { std::cout << encode_json(v) << '\n'; }
void usage() {
  std::cerr
      << "INDE P5 — estados e transporte verificáveis (primeiro corte)\n"
      << "  inde_project_file verificar ARQUIVO.inde\n"
      << "  inde_project_file inspecionar ARQUIVO.inde\n"
      << "  inde_project_file historico ARQUIVO.inde\n"
      << "  inde_project_file criar ARQUIVO_NOVO.inde UUID_PROJETO\n"
      << "  inde_project_file aplicar ARQUIVO.inde PEDIDO.json\n"
      << "  inde_project_file restaurar ARQUIVO.inde ESTADO SEQUENCIA_ESPERADA "
         "HASH_ESPERADO\n"
      << "  inde_project_file converter PASTA.inde ARQUIVO_NOVO.inde\n"
      << "  inde_project_file reabrir ARQUIVO.inde PASTA_NOVA.inde [ESTADO]\n"
      << "  inde_project_file registrar ARQUIVO.inde PASTA.inde "
         "SEQUENCIA_ESPERADA HASH_ESPERADO ROTULO\n"
      << "  inde_project_file demonstrar ARQUIVO_NOVO.inde\n";
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc < 3) {
      usage();
      return 2;
    }
    const std::string command = argv[1];
    if (command == "criar" && argc == 4) {
      auto s = Store::create(argv[2], argv[3]);
      print(summary(s.head()));
      return 0;
    }
    if (command == "converter" && argc == 4) {
      print(summary(pack_directory(argv[2], argv[3])));
      return 0;
    }
    if (command == "demonstrar" && argc == 3) {
      auto *uuid = g_uuid_string_random();
      const std::string owner(uuid);
      g_free(uuid);
      auto s = Store::create(argv[2], owner);
      Record villain{"entidade/vilao",
                     "entity",
                     "{\"nome\":\"Personagem de demonstração\"}",
                     "",
                     {},
                     0};
      Record doc{
          "documento/cena",   "document", "{\"texto\":\"O vilão chegou.\"}", "",
          {"entidade/vilao"}, 0};
      const auto first =
          s.apply(s.head(), {{villain.key, 0, villain}, {doc.key, 0, doc}}, {},
                  "Criar estado de demonstração");
      auto changed = doc;
      changed.payload = "{\"texto\":\"O vilão partiu.\"}";
      const auto second = s.apply(
          first, {{doc.key, first.records.at(doc.key).version, changed}}, {},
          "Alterar somente o documento");
      const auto restored =
          s.restore(second, first.sequence,
                    "Retomar primeiro estado conservando o segundo");
      s.verify();
      print(V::Object{{"initial", summary(first, true)},
                      {"changed", summary(second, true)},
                      {"restored", summary(restored, true)},
                      {"later_state_preserved",
                       s.state(second.sequence).hash == second.hash}});
      return 0;
    }
    auto store = Store::open(argv[2]);
    if (command == "verificar" && argc == 3) {
      store.verify();
      print(V::Object{{"valid", true}, {"state", summary(store.head())}});
      return 0;
    }
    if (command == "inspecionar" && argc == 3) {
      print(summary(store.head(), true));
      return 0;
    }
    if (command == "historico" && argc == 3) {
      V::Array values;
      for (const auto &r : store.history()) {
        V::Object row;
        row.emplace("sequence", r.sequence);
        if (r.parent)
          row.emplace("parent", *r.parent);
        else
          row.emplace("parent", nullptr);
        if (r.restored_from)
          row.emplace("restored_from", *r.restored_from);
        else
          row.emplace("restored_from", nullptr);
        row.emplace("hash", r.hash);
        row.emplace("label", r.label);
        row.emplace("created_at", r.created_at);
        values.emplace_back(std::move(row));
      }
      print(values);
      return 0;
    }
    if (command == "reabrir" && (argc == 4 || argc == 5)) {
      const auto state = argc == 5 ? number(argv[4]) : store.head().sequence;
      unpack_directory(store, state, argv[3]);
      print(V::Object{
          {"materialized", true},
          {"sequence", state},
          {"destination", std::filesystem::absolute(argv[3]).string()}});
      return 0;
    }
    if (command == "restaurar" && argc == 6) {
      auto expected = store.head();
      expected.sequence = number(argv[4]);
      expected.hash = argv[5];
      print(summary(store.restore(expected, number(argv[3]),
                                  "Retomar estado reservado")));
      return 0;
    }
    if (command == "registrar" && argc == 7) {
      auto expected = store.head();
      expected.sequence = number(argv[4]);
      expected.hash = argv[5];
      print(summary(
          update_directory(store, expected, argv[3], argv[2], argv[6])));
      return 0;
    }
    if (command == "aplicar" && argc == 4) {
      const auto size = std::filesystem::file_size(argv[3]);
      if (size > 16 * 1024 * 1024)
        throw Error("request_limit", "Pedido maior que o limite.");
      std::ifstream f(argv[3], std::ios::binary);
      if (!f)
        throw Error("request_read", "Pedido não pôde ser aberto.");
      const std::string raw{std::istreambuf_iterator<char>(f), {}};
      const auto v =
          parse_json(raw, 16 * 1024 * 1024);
      fields(v, {"format", "project_id", "store_id", "expected_sequence",
                 "expected_hash", "label", "edits"});
      if (v.at("format").as_string() != "INDE-P5-REQUEST-1")
        throw Error("request_format", "Pedido de formato não reconhecido.");
      State expected;
      expected.project_id = v.at("project_id").as_string();
      expected.store_id = v.at("store_id").as_string();
      expected.sequence = v.at("expected_sequence").as_integer();
      expected.hash = v.at("expected_hash").as_string();
      std::vector<Edit> edits;
      for (const auto &item : v.at("edits").as_array()) {
        fields(item, {"key", "expected_version", "value"});
        Edit e;
        e.key = item.at("key").as_string();
        e.expected_version = item.at("expected_version").as_integer();
        if (!item.at("value").is_null()) {
          const auto &value = item.at("value");
          fields(value, {"kind", "payload", "blob", "references"});
          Record r;
          r.key = e.key;
          r.kind = value.at("kind").as_string();
          r.payload = encode_json(value.at("payload"));
          r.blob = value.at("blob").as_string();
          for (const auto &ref : value.at("references").as_array())
            r.references.push_back(ref.as_string());
          e.value = std::move(r);
        }
        edits.push_back(std::move(e));
      }
      print(
          summary(store.apply(expected, edits, {}, v.at("label").as_string())));
      return 0;
    }
    usage();
    return 2;
  } catch (const Error &e) {
    std::cerr << e.code << ": " << e.what() << '\n';
    return e.code == "commit_uncertain" ? 3 : 1;
  } catch (const std::exception &e) {
    std::cerr << "Não foi possível concluir: " << e.what() << '\n';
    return 1;
  }
}
