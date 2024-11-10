#include "utils/image.hpp"
#include "utils/thread.hpp"
#include <fmt/format.h>
#include <borealis/core/cache_helper.hpp>
#ifdef USE_WEBP
#include <webp/decode.h>
#endif
#include <stb_image.h>

Image::Image() : image(nullptr) {
    this->isCancel = std::make_shared<std::atomic_bool>(false);
    brls::Logger::verbose("new Image {}", fmt::ptr(this));
}

Image::~Image() { brls::Logger::verbose("delete Image {}", fmt::ptr(this)); }

void Image::with(brls::Image* view, const std::string& url) {
    int tex = brls::TextureCache::instance().getCache(url);
    if (tex > 0) {
        view->innerSetImage(tex);
        return;
    }

    Ref item;
    std::lock_guard<std::mutex> lock(requestMutex);

    if (pool.empty()) {
        item = std::make_shared<Image>();
    } else {
        item = pool.front();
        pool.pop_front();
    }

    auto it = requests.insert(std::make_pair(view, item));
    if (!it.second) {
        brls::Logger::warning("insert Image {} failed", fmt::ptr(view));
        return;
    }

    item->image = view;
    item->url = url;
    item->isCancel->store(false);
    view->ptrLock();
    // 设置图片组件不处理纹理的销毁，由缓存统一管理纹理销毁
    view->setFreeTexture(false);

    ThreadPool::instance().submit(std::bind(&Image::doRequest, item));
}

void Image::cancel(brls::Image* view) {
    brls::TextureCache::instance().removeCache(view->getTexture());
    view->clear();

    clear(view);
}

void Image::doRequest() {
    if (this->isCancel->load()) {
        Image::clear(this->image);
        return;
    }
    try {
        HTTP s;
        std::ostringstream body;
        char* ct = nullptr;
        HTTP::set_option(s, this->isCancel, HTTP::Timeout{});
        s._get(url, &body, &ct);

        std::string data = body.str();
        uint8_t* imageData = nullptr;
        int imageW = 0, imageH = 0;
#ifdef USE_WEBP
        if (ct && strcmp(ct, "image/webp") == 0) {
            imageData = WebPDecodeRGBA((const uint8_t*)data.c_str(), data.size(), &imageW, &imageH);
        } else
#endif
        {
            int n;
            imageData = stbi_load_from_memory((unsigned char*)data.c_str(), data.size(), &imageW, &imageH, &n, 4);
        }
        brls::Logger::verbose("request Image {} size {}", this->url, data.size());
        brls::sync([this, imageData, imageW, imageH] {
            if (!this->isCancel->load()) {
                // Load texture
                int tex = brls::TextureCache::instance().getCache(url);
                if (tex == 0 && imageData != nullptr) {
                    NVGcontext* vg = brls::Application::getNVGContext();
                    tex = nvgCreateImageRGBA(vg, imageW, imageH, 0, imageData);
                    brls::TextureCache::instance().addCache(this->url, tex);
                }
                if (tex > 0) this->image->innerSetImage(tex);
                clear(this->image);
            }
            if (imageData) {
#ifdef USE_WEBP
                WebPFree(imageData);
#else
                stbi_image_free(imageData);
#endif
            }
        });
    } catch (const std::exception& ex) {
        brls::Logger::warning("request image {} {}", this->url, ex.what());
        Image::clear(this->image);
    }
}

void Image::clear(brls::Image* view) {
    std::lock_guard<std::mutex> lock(requestMutex);

    auto it = requests.find(view);
    if (it == requests.end()) return;

    it->second->image->ptrUnlock();
    it->second->image = nullptr;
    it->second->url.clear();
    it->second->isCancel->store(true);
    pool.push_back(it->second);
    requests.erase(it);
}