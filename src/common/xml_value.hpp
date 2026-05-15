#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#define MISSING_STRING "<missing_string>"

namespace indexed {

class Database;
class IndexBuilder;

class XmlValue {
public:
    static constexpr std::uint32_t INVALID_ID = UINT32_MAX;

    const std::string& mdate() const;
    const std::string& key() const;
    std::vector<std::string> authors() const;
    std::size_t author_count() const;
    const std::string& author_at(std::size_t index) const;
    const std::string& title() const;
    const std::string& journal() const;
    const std::string& volume() const;
    const std::string& month() const;
    const std::string& year() const;
    std::vector<std::string> cdroms() const;
    std::vector<std::string> ees() const;

private:
    friend class Database;
    friend class IndexBuilder;

    std::uint32_t mdate_id_ = INVALID_ID;
    std::uint32_t key_id_ = INVALID_ID;
    std::vector<std::uint32_t> author_ids_;
    std::uint32_t title_id_ = INVALID_ID;
    std::uint32_t journal_id_ = INVALID_ID;
    std::uint32_t volume_id_ = INVALID_ID;
    std::uint32_t month_id_ = INVALID_ID;
    std::uint32_t year_id_ = INVALID_ID;
    std::vector<std::uint32_t> cdrom_ids_;
    std::vector<std::uint32_t> ee_ids_;
    const Database* db_ = nullptr;
};

} // namespace indexed
