#include "client/local.hpp"
#include "client/webdav.hpp"
#include "client/apache.hpp"

namespace remote {

std::shared_ptr<Client> create(const AppRemote& c) {
    auto pos = c.url.find_first_of("://");
    if (pos == std::string::npos) {
        std::runtime_error("invalid url");
    }
    std::string scheme = c.url.substr(0, pos);
    if (scheme == "webdav") {
        std::string url = "http" + c.url.substr(pos);
        return std::make_shared<Webdav>(url, c);
    }
    if (scheme == "webdavs") {
        std::string url = "https" + c.url.substr(pos);
        return std::make_shared<Webdav>(url, c);
    }
    if (scheme == "file") {
        return std::make_shared<Local>(c.url.substr(pos + 3));
    }
    if (scheme == "http" || scheme == "https") {
        return std::make_shared<Apache>(c);
    }
    throw std::runtime_error("unsupport protocol");
}

}  // namespace remote