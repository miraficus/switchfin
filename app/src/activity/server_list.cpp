/*
    Copyright 2023 dragonflylee
*/

#include "activity/server_list.hpp"
#include "activity/main_activity.hpp"
#include "view/recycling_grid.hpp"
#include "tab/server_add.hpp"
#include "tab/server_login.hpp"
#include "utils/image.hpp"
#include "utils/dialog.hpp"
#include "api/jellyfin.hpp"

using namespace brls::literals;  // for _i18n

class ServerCell : public brls::Box {
public:
    ServerCell(const AppServer& s) {
        this->inflateFromXMLRes("xml/view/server_item.xml");

        this->setFocusSound(brls::SOUND_FOCUS_SIDEBAR);
        this->registerAction(
            "hints/ok"_i18n, brls::BUTTON_A,
            [](View* view) {
                brls::Application::onControllerButtonPressed(brls::BUTTON_NAV_RIGHT, false);
                return true;
            },
            false, false, brls::SOUND_CLICK_SIDEBAR);

        this->addGestureRecognizer(new brls::TapGestureRecognizer(this));

        this->serverId = s.id;
        this->labelName->setText(s.name.empty() ? "-" : s.name);
        this->labelUrl->setText(s.urls.front());
        this->labelUsers->setText(brls::getStr("main/setting/server/users", s.users.size()));
    }

    void setActive(bool active) {
        auto theme = brls::Application::getTheme();
        if (active) {
            this->accent->setVisibility(brls::Visibility::VISIBLE);
            this->labelName->setTextColor(theme["brls/sidebar/active_item"]);
            this->labelUsers->setTextColor(theme["brls/sidebar/active_item"]);
        } else {
            this->accent->setVisibility(brls::Visibility::INVISIBLE);
            this->labelName->setTextColor(theme["brls/text"]);
            this->labelUsers->setTextColor(theme["brls/text"]);
        }
    }

    std::string serverId;

private:
    BRLS_BIND(brls::Rectangle, accent, "brls/sidebar/item_accent");
    BRLS_BIND(brls::Label, labelName, "server/name");
    BRLS_BIND(brls::Label, labelUrl, "server/url");
    BRLS_BIND(brls::Label, labelUsers, "server/users");
};

class UserCell : public RecyclingGridItem {
public:
    UserCell() { this->inflateFromXMLRes("xml/view/user_item.xml"); }
    ~UserCell() { Image::cancel(this->picture); }

    void prepareForReuse() override { this->picture->setImageFromRes("img/video-card-bg.png"); }

    void cacheForReuse() override { Image::cancel(this->picture); }

    BRLS_BIND(brls::Label, labelName, "user/name");
    BRLS_BIND(brls::Image, picture, "user/avatar");

    std::string userId;
};

class ServerUserDataSource : public RecyclingGridDataSource {
public:
    ServerUserDataSource(const std::vector<AppUser>& users, ServerList* server) : list(users), parent(server) {}

    size_t getItemCount() override { return this->list.size(); }

    RecyclingGridItem* cellForRow(RecyclingView* recycler, size_t index) override {
        UserCell* cell = dynamic_cast<UserCell*>(recycler->dequeueReusableCell("Cell"));
        auto& u = this->list.at(index);
        cell->userId = u.id;
        cell->labelName->setText(u.name);

        std::string url = fmt::format(fmt::runtime(jellyfin::apiUserImage), u.id, "");
        Image::with(cell->picture, this->parent->getUrl() + url);
        return cell;
    }

    void onItemSelected(brls::Box* recycler, size_t index) override {
        brls::Application::blockInputs();

        brls::async([this, index]() {
            auto& u = this->list.at(index);
            HTTP::Header header = {fmt::format("X-Emby-Token: {}", u.access_token)};
            try {
                HTTP::get(this->parent->getUrl() + jellyfin::apiInfo, header, HTTP::Timeout{});
                brls::sync([this, u]() {
                    AppConfig::instance().addUser(u, this->parent->getUrl());
                    brls::Application::unblockInputs();
                    brls::Application::clear();
                    brls::Application::pushActivity(new MainActivity(), brls::TransitionAnimation::NONE);
                });
            } catch (const std::exception& ex) {
                std::string msg = ex.what();
                brls::sync([msg]() {
                    brls::Application::unblockInputs();
                    Dialog::show(msg);
                });
            }
        });
    }

    void clearData() override { this->list.clear(); }

private:
    std::vector<AppUser> list;
    ServerList* parent;
};

ServerList::ServerList() { brls::Logger::debug("ServerList: create"); }

ServerList::~ServerList() { brls::Logger::debug("ServerList Activity: delete"); }

void ServerList::onContentAvailable() {
    this->inputUrl->detail->setSingleLine(true);

    this->btnServerAdd->registerClickAction([](brls::View* view) {
        view->present(new ServerAdd());
        return true;
    });

    if (AppConfig::instance().getRemotes().empty() || brls::Application::getActivitiesStack().size() > 1) {
        // Hide the remote tab if there are no remotes
        brls::View* tab = this->getView("tab/remote");
        if (tab) tab->setVisibility(brls::Visibility::GONE);
    }

    this->sidebarServers->registerAction("main/setting/server/connect_new"_i18n, brls::BUTTON_Y, [](brls::View* view) {
        view->present(new ServerAdd());
        return true;
    });

    this->recyclerUsers->registerCell("Cell", [this]() {
        UserCell* cell = new UserCell();
        cell->registerAction("hints/delete"_i18n, brls::BUTTON_X, [this, cell](brls::View* view) {
            Dialog::cancelable("main/setting/server/delete"_i18n, [this, cell]() {
                if (AppConfig::instance().removeUser(cell->userId)) {
                    brls::sync([this]() { this->willAppear(); });
                }
            });
            return true;
        });
        return cell;
    });

    this->btnSignin->registerClickAction([this](brls::View* view) {
        view->present(new ServerLogin("", this->getUrl()));
        return true;
    });
}

void ServerList::willAppear(bool resetState) {
    auto list = AppConfig::instance().getServers();
    if (list.empty()) {
        this->mainframe->pushContentView(new ServerAdd());
        this->mainframe->setActionAvailable(brls::BUTTON_B, false);
        return;
    }
    this->mainframe->setActionAvailable(brls::BUTTON_B, true);
    this->sidebarServers->clearViews();

    for (auto& s : list) {
        ServerCell* item = new ServerCell(s);
        item->getFocusEvent()->subscribe([this, s](brls::View* view) {
            this->setActive(view);
            this->onSelect(s);
        });

        item->registerAction("hints/delete"_i18n, brls::BUTTON_X, [this, item](brls::View* view) {
            Dialog::cancelable("main/setting/server/delete"_i18n, [this, item]() {
                if (AppConfig::instance().removeServer(item->serverId)) {
                    brls::sync([this]() { this->willAppear(); });
                }
            });
            return true;
        });

        if (s.urls.front() == AppConfig::instance().getUrl()) {
            item->setActive(true);
            this->onSelect(s);
        }

        this->sidebarServers->addView(item);
    }
}

void ServerList::onSelect(const AppServer& s) {
    this->serverVersion->setDetailText(s.version.empty() ? "-" : s.version);
    this->inputUrl->init("main/setting/url"_i18n, s.urls.front(),
        [s](const std::string& text) { AppConfig::instance().addServer({.id = s.id, .urls = {text}}); });
    this->inputUrl->registerAction("hints/preset"_i18n, brls::BUTTON_X, [this, s](...) {
        brls::Dropdown* dropdown = new brls::Dropdown("main/setting/url"_i18n, s.urls, [this, s](int selected) {
            AppConfig::instance().addServer({
                .id = s.id,
                .urls = {s.urls[selected]},
            });
            brls::sync([this]() { this->willAppear(); });
        });
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });

    if (s.users.empty()) {
        this->recyclerUsers->setEmpty();
        brls::sync([this]() { brls::Application::giveFocus(this->sidebarServers); });
    } else {
        this->recyclerUsers->setDataSource(new ServerUserDataSource(s.users, this));
    }
}

std::string ServerList::getUrl() { return this->inputUrl->detail->getFullText(); }

void ServerList::setActive(brls::View* active) {
    for (auto item : this->sidebarServers->getChildren()) {
        ServerCell* cell = dynamic_cast<ServerCell*>(item);
        if (cell) cell->setActive(item == active);
    }
}