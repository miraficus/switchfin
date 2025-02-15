/*
    Copyright 2023 dragonflylee
*/

#include "activity/server_list.hpp"
#include "activity/main_activity.hpp"
#include "view/recycling_grid.hpp"
#include "view/auto_tab_frame.hpp"
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

        this->labelName->setText(s.name.empty() ? "-" : s.name);
        this->labelUrl->setText(s.urls.front());
    }

    void setActive(bool active) {
        auto theme = brls::Application::getTheme();
        if (active) {
            this->accent->setVisibility(brls::Visibility::VISIBLE);
            this->labelName->setTextColor(theme["brls/sidebar/active_item"]);
        } else {
            this->accent->setVisibility(brls::Visibility::INVISIBLE);
            this->labelName->setTextColor(theme["brls/text"]);
        }
    }

    bool getActive() { return this->accent->getVisibility() == brls::Visibility::VISIBLE; }

private:
    BRLS_BIND(brls::Rectangle, accent, "brls/sidebar/item_accent");
    BRLS_BIND(brls::Label, labelName, "server/name");
    BRLS_BIND(brls::Label, labelUrl, "server/url");
};

class UserCell : public RecyclingGridItem {
public:
    UserCell() { this->inflateFromXMLRes("xml/view/user_item.xml"); }
    ~UserCell() { Image::cancel(this->picture); }

    void prepareForReuse() override { this->picture->setImageFromRes("img/video-card-bg.png"); }

    void cacheForReuse() override { Image::cancel(this->picture); }

    BRLS_BIND(brls::Label, labelName, "user/name");
    BRLS_BIND(brls::Image, picture, "user/avatar");
};

class ServerUserDataSource : public RecyclingGridDataSource {
public:
    ServerUserDataSource(const std::vector<AppUser>& users, ServerList* server) : list(users), parent(server) {}

    size_t getItemCount() override { return this->list.size(); }

    RecyclingGridItem* cellForRow(RecyclingView* recycler, size_t index) override {
        UserCell* cell = dynamic_cast<UserCell*>(recycler->dequeueReusableCell("Cell"));
        auto& u = this->list.at(index);
        cell->labelName->setText(u.name);

        cell->registerAction("hints/delete"_i18n, brls::BUTTON_X, [this, u](brls::View* view) {
            Dialog::cancelable("main/setting/server/delete"_i18n, [this, u]() {
                AppConfig::instance().removeUser(u.id);
                this->parent->onUser(u.server_id);
            });
            return true;
        });

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
    this->tabFrame->setTabChangedAction([this](size_t index) {
        if (!index) this->willAppear();
    });

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

    this->recyclerUsers->registerCell("Cell", []() { return new UserCell(); });

    this->btnSignin->registerClickAction([this](brls::View* view) {
        view->present(new ServerLogin("", this->getUrl()));
        return true;
    });
}

void ServerList::willAppear(bool resetState) {
    auto list = AppConfig::instance().getServers();
    ServerCell* item = nullptr;
    std::string url = AppConfig::instance().getUrl();
    this->sidebarServers->clearViews();

    for (auto& s : list) {
        item = new ServerCell(s);
        item->getFocusEvent()->subscribe([this, s](brls::View* view) {
            this->setActive(view);
            this->onServer(s);
        });

        item->registerAction("hints/delete"_i18n, brls::BUTTON_X, [this, s](brls::View* item) {
            Dialog::cancelable("main/setting/server/delete"_i18n, [this, item, s]() {
                if (AppConfig::instance().removeServer(s.id)) {
                    brls::View* view = new ServerAdd();
                    this->tabFrame->setTabAttachedView(view);
                    brls::Application::giveFocus(view);
                } else {
                    this->sidebarServers->removeView(item);
                }
            });
            return true;
        });

        if (s.urls.size() > 0) {
            if (url.empty()) {
                url = s.urls.front();
            }
            if (s.urls.front() == url) {
                item->setActive(true);
                this->onServer(s);
            }
        }

        this->sidebarServers->addView(item);
    }

    if (!item) this->tabFrame->setTabAttachedView(new ServerAdd());
}

void ServerList::onServer(const AppServer& s) {
    this->serverVersion->setDetailText(s.version.empty() ? "-" : s.version);
    this->inputUrl->setDetailText(s.urls.front());
    this->inputUrl->registerAction("hints/preset"_i18n, brls::BUTTON_X, [this, s](...) {
        return brls::Application::getImeManager()->openForText(
            [this, s](const std::string& text) {
                AppConfig::instance().addServer({.id = s.id, .urls = {text}});
                this->inputUrl->setDetailText(text);
            },
            "main/setting/url"_i18n, "", 255, this->inputUrl->detail->getFullText());
    });
    this->inputUrl->registerClickAction([this, s](...) {
        brls::Dropdown* dropdown = new brls::Dropdown("main/setting/url"_i18n, s.urls, [this, s](int selected) {
            const std::string& url = s.urls[selected];
            AppConfig::instance().addServer({.id = s.id, .urls = {url}});
            this->inputUrl->setDetailText(url);
        });
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });

    this->onUser(s.id);
}

void ServerList::onUser(const std::string& id) {
    auto users = AppConfig::instance().getUsers(id);
    if (users.empty()) {
        this->recyclerUsers->setEmpty();
        brls::sync([this]() { brls::Application::giveFocus(this->sidebarServers); });
    } else {
        this->recyclerUsers->setDataSource(new ServerUserDataSource(users, this));
    }
}

std::string ServerList::getUrl() { return this->inputUrl->detail->getFullText(); }

void ServerList::setActive(brls::View* active) {
    for (auto item : this->sidebarServers->getChildren()) {
        ServerCell* cell = dynamic_cast<ServerCell*>(item);
        if (cell) cell->setActive(item == active);
    }
}