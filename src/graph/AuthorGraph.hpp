#pragma once

#include "common/database.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace indexed {

class AuthorGraph {
public:
    void buildGraph(const Database& db);
    std::vector<std::pair<std::string, int>> queryCoauthors(const std::string& author) const;
    std::vector<std::string> countCliquesByOrder() const;

private:
    const Database* db_ = nullptr;
};

} // namespace indexed
