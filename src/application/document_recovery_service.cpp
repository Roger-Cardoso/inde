#include "inde/application/document_recovery_service.hpp"
#include "inde/project/document_state.hpp"
namespace inde::application {
project::RecoveryRecord
DocumentRecoveryService::checked(const project::Project &owner,
                                 const std::string &session,
                                 const std::string &hash) const {
  auto r = store_.load(owner, session);
  if (!r || r->status != "pending" || (!hash.empty() && r->checksum != hash))
    throw std::runtime_error(
        "Registro mudou ou não está pendente; abra a prévia novamente");
  auto context = writing_.save_snapshot(r->state.document, r->baseline, 0, 0);
  if (context.owner.manifest().project_id != owner.manifest().project_id ||
      context.owner.path() != owner.path())
    throw std::runtime_error("Recuperação pertence a outro Projeto ativo");
  return *r;
}
RecoveryPreview
DocumentRecoveryService::inspect(const project::Project &owner,
                                 const std::string &session) const {
  persistence::RecoveryLease lease(owner, session, false);
  auto r = checked(owner, session, {});
  auto current = writing_.document(r.state.document.id);
  const bool same =
      current && project::same_document_state(*current, r.baseline);
  const bool already = current && project::same_document_state(
                                      *current, r.state.document, false);
  return {std::move(r), same, already,
          already ? "O texto/camadas já coincidem com o Projeto. Você pode "
                    "confirmar a seleção recuperada ou arquivar o registro."
          : same  ? "O original coincide com a base do checkpoint. Restaurar "
                    "exige confirmação e nova comparação atômica."
                  : "O original mudou ou foi removido. Não será sobrescrito; "
                   "crie uma cópia de resgate de texto e estilos."};
}
project::RecoveryState
DocumentRecoveryService::restore(const project::Project &owner,
                                 const std::string &session,
                                 const std::string &hash) {
  persistence::RecoveryLease lease(owner, session, false);
  auto r = checked(owner, session, hash);
  auto expected = r.baseline;
  const auto current = writing_.document(r.state.document.id);
  if (current &&
      project::same_document_state(*current, r.state.document, false))
    expected = *current;
  auto saved =
      writing_.update_document_if_unchanged(r.state.document, expected);
  // A second-database failure retains evidence and is reported, never a silent
  // merge.
  try {
    store_.resolve(owner, session, hash, "restored");
  } catch (const std::exception &e) {
    throw std::runtime_error("Original já persistido; não foi possível "
                             "arquivar o journal. Evidência mantida: " +
                             std::string(e.what()));
  }
  r.state.document = std::move(saved);
  return r.state;
}
project::Document
DocumentRecoveryService::create_copy(const project::Project &owner,
                                     const std::string &session,
                                     const std::string &hash) {
  persistence::RecoveryLease lease(owner, session, false);
  auto r = checked(owner, session, hash);
  auto copy = writing_.create_recovery_document(r.state.document);
  try {
    store_.resolve(owner, session, hash, "copy-created");
  } catch (const std::exception &e) {
    throw std::runtime_error("Cópia de resgate já criada na Biblioteca (" +
                             copy.id + "); journal mantido: " + e.what());
  }
  return copy;
}
void DocumentRecoveryService::discard(const project::Project &owner,
                                      const std::string &session,
                                      const std::string &hash) {
  persistence::RecoveryLease lease(owner, session, false);
  static_cast<void>(checked(owner, session, hash));
  store_.resolve(owner, session, hash, "discarded");
}
} // namespace inde::application
