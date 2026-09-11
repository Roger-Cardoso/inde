#pragma once
#include "inde/persistence/catalog_store.hpp"

namespace inde::persistence {
class CatalogRepository final : public CatalogStore {
public:
    [[nodiscard]] project::Catalog load(const std::filesystem::path& project_path) const override;
    void save(const std::filesystem::path&, const project::IntellectualProperty&) const override;
    void save(const std::filesystem::path&, const project::Work&) const override;
    void remove_intellectual_property(const std::filesystem::path&, const std::string& id) const override;
    void remove_work(const std::filesystem::path&, const std::string& id) const override;
};
} // namespace inde::persistence
