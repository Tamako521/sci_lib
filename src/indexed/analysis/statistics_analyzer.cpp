#include "indexed/analysis/statistics_analyzer.hpp"

namespace indexed {

std::vector<AuthorStat> StatisticsAnalyzer::top_authors(const Database& db, std::size_t limit) const
{
    return db.top_authors(limit);
}

YearKeywordTop StatisticsAnalyzer::yearly_hot_keywords(const Database& db, std::size_t limit) const
{
    return db.yearly_hot_keywords(limit);
}

} // namespace indexed
