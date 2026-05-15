#include "indexed/common/xml_value.hpp"

#include "indexed/common/database.hpp"

namespace indexed {

namespace {
const std::string& value_or_missing(const Database* db, std::uint32_t id)
{
    static const std::string missing = MISSING_STRING;
    if (db == nullptr || id == XmlValue::INVALID_ID) {
        return missing;
    }
    return db->get_string(id);
}
}

const std::string& XmlValue::mdate() const { return value_or_missing(db_, mdate_id_); }
const std::string& XmlValue::key() const { return value_or_missing(db_, key_id_); }
const std::string& XmlValue::title() const { return value_or_missing(db_, title_id_); }
const std::string& XmlValue::journal() const { return value_or_missing(db_, journal_id_); }
const std::string& XmlValue::volume() const { return value_or_missing(db_, volume_id_); }
const std::string& XmlValue::month() const { return value_or_missing(db_, month_id_); }
const std::string& XmlValue::year() const { return value_or_missing(db_, year_id_); }

std::vector<std::string> XmlValue::authors() const
{
    std::vector<std::string> result;
    result.reserve(author_ids_.size());
    for (std::uint32_t id : author_ids_) {
        result.push_back(value_or_missing(db_, id));
    }
    return result;
}

std::size_t XmlValue::author_count() const
{
    return author_ids_.size();
}

const std::string& XmlValue::author_at(std::size_t index) const
{
    return value_or_missing(db_, author_ids_.at(index));
}

std::vector<std::string> XmlValue::cdroms() const
{
    std::vector<std::string> result;
    for (std::uint32_t id : cdrom_ids_) {
        result.push_back(value_or_missing(db_, id));
    }
    return result;
}

std::vector<std::string> XmlValue::ees() const
{
    std::vector<std::string> result;
    for (std::uint32_t id : ee_ids_) {
        result.push_back(value_or_missing(db_, id));
    }
    return result;
}

} // namespace indexed
