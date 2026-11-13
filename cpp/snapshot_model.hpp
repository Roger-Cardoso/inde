#pragma once
#include "inde/application/work_production.hpp"
#include "inde/project/narrative.hpp"
#include "inde/persistence/json.hpp"
namespace inde::publication {
using V = persistence::json::Value;
using O = V::Object;
using A = V::Array;
std::string hash(const std::string &);
V strings(const std::vector<std::string> &);
V encode_document(const project::Document &);
V snapshot(const application::WorkProduction &,const std::vector<project::NarrativeEntity> &);
}
