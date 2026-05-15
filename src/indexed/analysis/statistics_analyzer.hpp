#pragma once

#include "indexed/common/database.hpp"

namespace indexed {

class StatisticsAnalyzer {
public:
    std::vector<AuthorStat> top_authors(const Database& db, std::size_t limit = 100) const;
    YearKeywordTop yearly_hot_keywords(const Database& db, std::size_t limit = 10) const;
};

} // namespace indexed
