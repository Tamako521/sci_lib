#include "graph/AuthorGraph.hpp"

namespace indexed {

void AuthorGraph::buildGraph(const Database& db)
{
    db_ = &db;
}

std::vector<std::pair<std::string, int>> AuthorGraph::queryCoauthors(const std::string& author) const
{
    return db_ == nullptr ? std::vector<std::pair<std::string, int>>{} : db_->coauthors(author);
}

std::vector<std::uint64_t> AuthorGraph::countCliquesByOrder() const
{
    return db_ == nullptr ? std::vector<std::uint64_t>{} : db_->count_cliques_by_order();
}

} // namespace indexed
