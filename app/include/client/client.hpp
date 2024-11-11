#pragma once

#include <utils/config.hpp>
#include <api/http.hpp>

namespace remote {

enum class EntryType {
    FILE,
    DIR,
    VIDEO,
    AUDIO,
    IMAGE,
    PLAYLIST,
    SUBTITLE,
    TEXT,
    UP,
};

struct DirEntry {
    std::string name;
    std::string path;
    uint64_t fileSize;
    EntryType type;
    std::tm modified;

    const std::string& url() const { return this->path; }
};

class Client {
public:
    virtual ~Client() = default;
    virtual std::vector<DirEntry> list(const std::string& path) = 0;
    virtual void auth(const std::string& user, const std::string& passwd) {}
    const std::string& rootPath() { return this->root; }
    const std::string& extraOption() { return this->extra; }

protected:
    std::string root;
    std::string extra;

    void init(const AppRemote &conf, HTTP& cilent);
};

std::shared_ptr<Client> create(const AppRemote& c);

}  // namespace remote