#include "client/local.hpp"
#include "client/webdav.hpp"
#include "client/apache.hpp"
#include <utils/misc.hpp>

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

void Client::init(const AppRemote &conf, HTTP& cilent) {
    std::stringstream ssextra;
    ssextra << fmt::format("network-timeout={}", HTTP::TIMEOUT / 100);
    if (HTTP::PROXY_STATUS) ssextra << ",http-proxy=\"" << HTTP::PROXY << "\"";

    if (conf.user.size() > 0 || conf.passwd.size() > 0) {
        std::string auth = base64::encode(fmt::format("{}:{}", conf.user, conf.passwd));
        ssextra << fmt::format(",http-header-fields=\"Authorization: Basic {}\"", auth);
        cilent.set_basic_auth(conf.user, conf.passwd);
    }
    if (conf.user_agent.size() > 0) {
        ssextra << fmt::format(",user_agent=\"{}\"", conf.user_agent);
        cilent.set_user_agent(conf.user_agent);
    }
    this->extra = ssextra.str();
}

}  // namespace remote