#pragma once
#include "inde/persistence/structural_store.hpp"

namespace inde::persistence {
class StructuralRepository final : public StructuralStore {
public:
    [[nodiscard]] std::vector<project::StructuralNode> load(const std::filesystem::path&) const override;
    [[nodiscard]] std::vector<project::StructuralElementType> load_types(const std::filesystem::path&) const override;
    void save_type(const std::filesystem::path&, const project::StructuralElementType&) const override;
    void remove_type(const std::filesystem::path&, const std::string& id) const override;
    void save(const std::filesystem::path&, const project::StructuralNode&) const override;
    void save_many(const std::filesystem::path&, const std::vector<project::StructuralNode>&) const override;
    void remove(const std::filesystem::path&, const std::string& id) const override;
    std::string move_to_trash(const std::filesystem::path&, const std::vector<std::string>& ids) const override;
    bool restore_latest_trash(const std::filesystem::path&) const override;
};
} // namespace inde::persistence
