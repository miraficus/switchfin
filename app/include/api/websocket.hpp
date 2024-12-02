/*
    Copyright 2024 dragonflylee
*/

#pragma once

#include <string>
#include <atomic>
#include <memory>
#ifdef BOREALIS_USE_STD_THREAD
#include <thread>
#else
#include <pthread.h>
#endif
#include <curl/curl.h>

class websocket {
public:
    websocket(const std::string& url);
    ~websocket();

private:
    static void* ws_recv(void*);
    static void on_msg(const std::string& msg);

#ifdef BOREALIS_USE_STD_THREAD
    std::shared_ptr<std::thread> th;
#else
    pthread_t th;
#endif
    CURL *easy;
    std::shared_ptr<std::atomic_bool> isStopped;
};