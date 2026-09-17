// Compares a produced image against a golden.
// Gates on mean absolute difference rather than an exact hash: GaussianBlur and
// Canny can differ in the last bit across CPUs and OpenCV point releases, and an
// exact hash would fail on that noise while still missing nothing real.
#include <cstdio>
#include <cstdlib>
#include <opencv2/opencv.hpp>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::fprintf(stderr,
                     "usage: avision_compare <actual> <golden> [max_mean_abs_diff]\n");
        return 2;
    }

    const double tolerance = (argc > 3) ? std::atof(argv[3]) : 1.5;

    const cv::Mat actual = cv::imread(argv[1], cv::IMREAD_COLOR);
    const cv::Mat golden = cv::imread(argv[2], cv::IMREAD_COLOR);

    if (actual.empty()) {
        std::fprintf(stderr, "compare: could not read actual image %s\n", argv[1]);
        return 2;
    }
    if (golden.empty()) {
        std::fprintf(stderr, "compare: could not read golden image %s\n", argv[2]);
        return 2;
    }

    if (actual.size() != golden.size()) {
        std::fprintf(stderr, "compare: size mismatch: actual %dx%d vs golden %dx%d\n",
                     actual.cols, actual.rows, golden.cols, golden.rows);
        return 1;
    }

    cv::Mat diff;
    cv::absdiff(actual, golden, diff);

    const cv::Scalar meanPerChannel = cv::mean(diff);
    const double meanAbs =
        (meanPerChannel[0] + meanPerChannel[1] + meanPerChannel[2]) / 3.0;

    double maxAbs = 0.0;
    cv::minMaxLoc(diff.reshape(1), nullptr, &maxAbs);

    std::printf("compare: mean_abs=%.4f max_abs=%.0f tolerance=%.4f\n", meanAbs, maxAbs,
                tolerance);

    if (meanAbs > tolerance) {
        std::fprintf(stderr, "compare: FAIL mean absolute difference %.4f exceeds %.4f\n",
                     meanAbs, tolerance);
        return 1;
    }
    return 0;
}
