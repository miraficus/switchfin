#include "client/apache.hpp"
#include <curl/curl.h>

namespace remote {

Apache::Apache(const AppRemote &conf) {
    HTTP::set_option(this->c, HTTP::Timeout{});
    this->init(conf, this->c);

    this->root = conf.url;
    if (this->root.back() != '/') this->root.push_back('/');
    this->host = this->root.substr(0, this->root.find("/", this->root.find("://") + 3));
    brls::Logger::debug("remote::Apache host {} root {}", this->host, this->root);
}

std::vector<DirEntry> Apache::list(const std::string &path) {
    std::ostringstream ss;
    this->c._get(path, &ss);
    std::string resp = ss.str();

    std::vector<DirEntry> s = {{.type = EntryType::UP}};
    size_t index = 0;
    while (index < resp.size()) {
        std::string link;
        auto found = resp.find("<a href=\"", index);
        if (found == std::string::npos) break;
        index = found + 9;
        while (index < resp.size()) {
            if (resp[index] == '"') {
                if (link.empty()) break;
                if (link[0] == '?' || link[0] == '#' || link[0] == '.') break;

                DirEntry item;
                std::string name = link;
                if (resp[index - 1] == '/') {
                    item.type = EntryType::DIR;
                    name.pop_back();
                } else {
                    item.type = EntryType::FILE;
                }
                if (link[0] == '/') {
                    item.path = this->host + link;
                    auto pos = name.find_last_of('/');
                    if (pos == std::string::npos) break;
                    name = name.substr(pos + 1);
                } else {
                    item.path = path + link;
                }

                char *unescape = curl_unescape(name.c_str(), name.size());
                item.name = unescape;
                curl_free(unescape);
                s.push_back(item);
                break;
            }
            link += resp[index++];
        }
    }
    return s;
}

}  // namespace remote