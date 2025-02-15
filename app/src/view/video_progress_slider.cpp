//
// Created by fang on 2022/8/15.
//

#include "view/video_progress_slider.hpp"
#include "view/svg_image.hpp"

using namespace brls;

VideoProgressSlider::VideoProgressSlider() {
    input = Application::getPlatform()->getInputManager();

    line = new brls::Rectangle();
    lineEmpty = new brls::Rectangle();
    pointerIcon = new SVGImage();
    pointer = new brls::Box();

    line->detach();
    lineEmpty->detach();
    pointer->detach();

    line->setHeight(7);
    line->setCornerRadius(3.5f);

    lineEmpty->setHeight(7);
    lineEmpty->setCornerRadius(3.5f);

    pointerIcon->setDimensions(22, 22);
    pointerIcon->setImageFromSVGRes("icon/ico-circle.svg");

    pointer->setDimensions(30, 30);
    pointer->setFocusable(true);
    pointer->setHighlightCornerRadius(30);
    pointer->setHideHighlightBackground(true);
    pointer->setHideClickAnimation(true);
    pointer->setAlignItems(brls::AlignItems::CENTER);
    pointer->setJustifyContent(brls::JustifyContent::CENTER);
    pointer->addView(pointerIcon);

    this->registerFloatXMLAttribute("pointer", [this](float value) { pointer->setDimensions(value, value); });
    this->registerFloatXMLAttribute("icon", [this](float value) { pointerIcon->setDimensions(value, value); });
    this->registerFloatXMLAttribute("line", [this](float value) {
        line->setHeight(value);
        lineEmpty->setHeight(value);
    });

    addView(pointer);
    addView(line);
    addView(lineEmpty);

    Theme theme = Application::getTheme();

    line->setColor(theme["brls/slider/line_filled"]);
    lineEmpty->setColor(theme["brls/slider/line_empty"]);

    pointer->addGestureRecognizer(new PanGestureRecognizer(
        [this](PanGestureStatus status, Sound* soundToPlay) {
            Application::giveFocus(pointer);

            static float lastProgress = progress;

            if (status.state == GestureState::UNSURE) {
                *soundToPlay = SOUND_FOCUS_CHANGE;
                return;
            }

            else if (status.state == GestureState::INTERRUPTED || status.state == GestureState::FAILED) {
                *soundToPlay = SOUND_TOUCH_UNFOCUS;
                return;
            }

            else if (status.state == GestureState::START) {
                lastProgress = progress;
            }

            float paddingWidth = getWidth() - pointer->getWidth();
            float delta = status.position.x - status.startPosition.x;

            setProgress(lastProgress + delta / paddingWidth);
            progressEvent.fire(this->progress);

            if (status.state == GestureState::END) {
                Application::getPlatform()->getAudioPlayer()->play(SOUND_SLIDER_RELEASE);
                progressSetEvent.fire(this->progress);
                Application::giveFocus(this->getParentActivity()->getContentView());
            }
        },
        PanAxis::HORIZONTAL));

    this->addGestureRecognizer(
        new TapGestureRecognizer([this](brls::TapGestureStatus status, brls::Sound* soundToPlay) {
            if (status.state != GestureState::END) return;
            float paddingWidth = getWidth() - pointer->getWidth();
            float delta = status.position.x - pointer->getWidth() / 2 - pointer->getX();
            setProgress(progress + delta / paddingWidth);
            progressSetEvent.fire(this->progress);
            Application::giveFocus(this->getParentActivity()->getContentView());
        }));

    progress = 0;
}

brls::View* VideoProgressSlider::create() { return new VideoProgressSlider(); }

void VideoProgressSlider::onLayout() {
    Box::onLayout();
    updateUI();
}

View* VideoProgressSlider::getDefaultFocus() { return pointer; }

void VideoProgressSlider::setProgress(float progress) {
    static int lastProgressTicker = this->progress * 10;

    this->progress = progress;

    if (this->progress < 0) this->progress = 0;

    if (this->progress > 1) this->progress = 1;

    if (lastProgressTicker != (int)(this->progress * 10)) {
        lastProgressTicker = this->progress * 10;
        Application::getAudioPlayer()->play(SOUND_SLIDER_TICK);
    }

    updateUI();
}

void VideoProgressSlider::updateUI() {
    float paddingWidth = getWidth() - pointer->getWidth();
    float lineStart = pointer->getWidth() / 2;
    float lineStartWidth = paddingWidth * progress;
    float lineEnd = paddingWidth * progress + pointer->getWidth() / 2;
    float lineEndWidth = paddingWidth * (1 - progress);
    float lineYPos = getHeight() / 2 - line->getHeight() / 2;

    line->setDetachedPosition(lineStart, lineYPos);
    line->setWidth(lineStartWidth);

    lineEmpty->setDetachedPosition(lineEnd, lineYPos);
    lineEmpty->setWidth(lineEndWidth);

    pointer->setDetachedPosition(lineEnd - pointer->getWidth() / 2, getHeight() / 2 - pointer->getHeight() / 2);
}

VideoProgressSlider::~VideoProgressSlider() = default;

void VideoProgressSlider::clearClipPoint() { clipPointList.clear(); }

void VideoProgressSlider::setClipPoint(const std::vector<float>& data) { clipPointList = data; }

void VideoProgressSlider::draw(
    NVGcontext* vg, float x, float y, float width, float height, Style style, FrameContext* ctx) {
    for (View* child : this->getChildren()) {
        if (child == this->pointer) {
            // draw clip point before pointer
            nvgBeginPath(vg);
            nvgFillColor(vg, a(nvgRGBf(1.0f, 1.0f, 1.0f)));
            for (auto& i : clipPointList) {
                nvgCircle(vg, x + 32 + i * (width - 64), y + height / 2, 3);
            }
            nvgFill(vg);
        }
        child->frame(ctx);
    }
}