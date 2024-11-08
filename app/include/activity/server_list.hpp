/*
    Copyright 2023 dragonflylee
*/

#pragma once

#include <borealis.hpp>
#include "utils/config.hpp"

class RecyclingGrid;
class ServerCell;

class ServerList : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/server_list.xml");

    ServerList();
    ~ServerList();

    void onContentAvailable() override;
    void onSelect(const AppServer &s);
    std::string getUrl();
    void willAppear(bool resetState = false) override;

private:
    BRLS_BIND(brls::Button, btnServerAdd, "btn/server/add");
    BRLS_BIND(brls::Box, sidebarServers, "server/sidebar");
    BRLS_BIND(brls::Box, serverDetail, "server/detail");
    BRLS_BIND(RecyclingGrid, recyclerUsers, "user/recycler");
    BRLS_BIND(brls::DetailCell, serverVersion, "server/version");
    BRLS_BIND(brls::InputCell, inputUrl, "selector/server/urls");
    BRLS_BIND(brls::Button, btnSignin, "btn/server/signin");
    BRLS_BIND(brls::AppletFrame, mainframe, "server/frame");

    void setActive(brls::View *active);
};