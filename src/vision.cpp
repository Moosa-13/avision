#include "vision.hpp"

#include <algorithm>

namespace avision {
namespace {

const std::vector<std::string> kModeNames = {"dog-vision", "cat-vision", "snake-vision"};

// ---------- DOG VISION ----------
// Dichromatic approximation: red and green collapse onto a single response
// curve (hence the two identical matrix rows), blue is largely preserved.
cv::Mat simulateDogVision(const cv::Mat &src) {
    cv::Mat img;
    cv::cvtColor(src, img, cv::COLOR_BGR2RGB);
    img.convertTo(img, CV_32F, 1.0 / 255.0);

    // clang-format off
    // Laid out as the 3x3 matrix it is; the formatter would fold it into a
    // flat argument list and lose the rows.
    cv::Matx33f dogMatrix(
        0.3f, 0.6f, 0.1f,   // R reduced
        0.3f, 0.6f, 0.1f,   // G dominates
        0.0f, 0.2f, 0.8f    // B preserved
    );
    // clang-format on

    cv::Mat transformed;
    cv::transform(img, transformed, dogMatrix);

    // blur (lower acuity)
    cv::GaussianBlur(transformed, transformed, cv::Size(5, 5), 1.2);

    // slightly brighter
    transformed *= 1.1;

    transformed.convertTo(transformed, CV_8UC3, 255.0);
    cv::cvtColor(transformed, transformed, cv::COLOR_RGB2BGR);

    return transformed;
}

// ---------- CAT VISION ----------
cv::Mat simulateCatVision(const cv::Mat &src) {
    cv::Mat img;
    src.convertTo(img, CV_32F, 1.0 / 255.0);

    // brighten (low-light vision)
    img *= 1.3;

    // desaturate slightly
    cv::Mat gray, gray3;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::cvtColor(gray, gray3, cv::COLOR_GRAY2BGR);

    cv::Mat colored;
    cv::addWeighted(img, 0.6, gray3, 0.4, 0.0, colored);

    // edge detection (on the original uint8 image)
    cv::Mat edges;
    cv::Canny(src, edges, 80, 160);

    edges.convertTo(edges, CV_32F, 1.0 / 255.0);
    cv::cvtColor(edges, edges, cv::COLOR_GRAY2BGR);

    cv::Mat result;
    cv::addWeighted(colored, 0.8, edges, 0.2, 0.0, result);

    result.convertTo(result, CV_8UC3, 255.0);
    return result;
}

// ---------- SNAKE (THERMAL STYLE) ----------
cv::Mat simulateSnakeVision(const cv::Mat &src) {
    cv::Mat gray;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

    cv::Mat normalized;
    cv::normalize(gray, normalized, 0, 255, cv::NORM_MINMAX);

    cv::Mat heatmap;
    cv::applyColorMap(normalized, heatmap, cv::COLORMAP_JET);

    return heatmap;
}

} // namespace

bool parseMode(const std::string &name, Mode &out) {
    if (name == "dog-vision") {
        out = Mode::Dog;
        return true;
    }
    if (name == "cat-vision") {
        out = Mode::Cat;
        return true;
    }
    if (name == "snake-vision") {
        out = Mode::Snake;
        return true;
    }
    return false;
}

const char *modeName(Mode mode) {
    switch (mode) {
    case Mode::Dog: return "dog-vision";
    case Mode::Cat: return "cat-vision";
    case Mode::Snake: return "snake-vision";
    }
    return "unknown";
}

const std::vector<std::string> &modeNames() {
    return kModeNames;
}

cv::Mat simulate(const cv::Mat &src, Mode mode) {
    switch (mode) {
    case Mode::Dog: return simulateDogVision(src);
    case Mode::Cat: return simulateCatVision(src);
    case Mode::Snake: return simulateSnakeVision(src);
    }
    return cv::Mat();
}

std::string makeOutputPath(const std::string &inputPath, const std::string &mode) {
    const std::string suffix = "_" + mode;

    // Only a dot *after* the final path separator delimits an extension,
    // otherwise "images/v1.2/cat" would splice the suffix into the directory.
    const size_t slash = inputPath.find_last_of("/\\");
    const size_t dot = inputPath.find_last_of('.');

    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
        return inputPath + suffix + ".jpg";
    }

    return inputPath.substr(0, dot) + suffix + inputPath.substr(dot);
}

} // namespace avision
