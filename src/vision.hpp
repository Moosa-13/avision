#pragma once

#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

namespace avision {

enum class Mode { Dog, Cat, Snake };

// Returns false if `name` is not a recognised mode.
bool parseMode(const std::string &name, Mode &out);

const char *modeName(Mode mode);

const std::vector<std::string> &modeNames();

// Applies the perception model for `mode`. `src` must be a non-empty 8-bit
// 3-channel BGR image, as produced by cv::imread(..., IMREAD_COLOR).
cv::Mat simulate(const cv::Mat &src, Mode mode);

// "images/cat.jpg" + "dog-vision" -> "images/cat_dog-vision.jpg"
std::string makeOutputPath(const std::string &inputPath, const std::string &mode);

} // namespace avision
