#include "common\xml_value.hpp"
#include "common\database.hpp"

static const std::string& missing_string() {
    static const std::string value = MISSING_STRING;
    return value;
}

static const std::string& get_string_or_missing(const Database* db, uint32_t id) {
    if (id == XmlValue::INVALID_ID)
        return missing_string();
    return db->get_string(id);
}

const std::string& XmlValue::mdate() const{
    return get_string_or_missing(db_, mdate_id_);
}

const std::string &XmlValue::key() const
{
    return get_string_or_missing(db_, key_id_);
}

std::vector<std::string> XmlValue::authors() const
{
    std::vector<std::string> result;
    result.reserve(author_ids_.size());
    for (uint32_t id : author_ids_)
        result.push_back(get_string_or_missing(db_, id));
    return result;
}

const std::string &XmlValue::title() const
{
    return get_string_or_missing(db_, title_id_);
}

const std::string &XmlValue::journal() const
{
    return get_string_or_missing(db_, journal_id_);
}

const std::string &XmlValue::volume() const
{
    return get_string_or_missing(db_, volume_id_);
}

const std::string &XmlValue::month() const
{
    return get_string_or_missing(db_, month_id_);
}

const std::string &XmlValue::year() const
{
    return get_string_or_missing(db_, year_id_);
}

std::vector<std::string> XmlValue::cdroms() const
{
    std::vector<std::string> result;
    result.reserve(cdrom_ids_.size());
    for (uint32_t id : cdrom_ids_)
        result.push_back(get_string_or_missing(db_, id));
    return result;
}

std::vector<std::string> XmlValue::ees() const
{
    std::vector<std::string> result;
    result.reserve(ee_ids_.size());
    for (uint32_t id : ee_ids_)
        result.push_back(get_string_or_missing(db_, id));
    return result;
}

void XmlValue::setMdate(uint32_t id){
    mdate_id_=id;
}

void XmlValue::setKey(uint32_t id){
    key_id_ = id;
}

void XmlValue::addAuthor(uint32_t id){
    author_ids_.push_back(id);
}

void XmlValue::setTitle(uint32_t id){
    title_id_ = id;
}

void XmlValue::setJournal(uint32_t id){
    journal_id_ = id;
}

void XmlValue::setVolume(uint32_t id){
    volume_id_ = id;
}

void XmlValue::setMonth(uint32_t id){
    month_id_ = id;
}

void XmlValue::setYear(uint32_t id){
    year_id_ = id;
}

void XmlValue::addCdrom(uint32_t id){
    cdrom_ids_.push_back(id);
}

void XmlValue::addEe(uint32_t id){
    ee_ids_.push_back(id);
}

void XmlValue::print_val() const {
    std::cout << "mdate: " << mdate() << '\n'
	    << "key: " << key() << '\n';
    std::vector<std::string> author = authors();
    std::cout << "author: ";
    for (const std::string& item : author) {
        std::cout << item << "，";
    }
    std::cout << std::endl;
    std::cout << "title: " << title()   << '\n'
	    << "journal: "	   << journal() << '\n'
	    << "volume: "	   << volume()  << '\n'
	    << "month: "	   << month()   << '\n'
	    << "year: "		   << year()    << '\n';
    std::vector<std::string> cdrom = cdroms();
    std::cout << "cdrom: ";
    for (const std::string& item : cdrom) {
        std::cout << item << "，";
    }
    std::cout << std::endl;
    std::vector<std::string> ee = ees();
    std::cout << "ee: ";
    for (const std::string& item : ee) {
        std::cout << item << "，";
    }
    std::cout << std::endl;
    return;
}
