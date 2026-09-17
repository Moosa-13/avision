// Generates the deterministic test image used by the golden comparisons.
// Purely a function of (x, y) so it reproduces bit-for-bit anywhere.
#include <cstdio>
#include <opencv2/opencv.hpp>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: avision_make_fixture <out.png> [width] [height]\n");
        return 1;
    }

    const int width = (argc > 2) ? std::atoi(argv[2]) : 160;
    const int height = (argc > 3) ? std::atoi(argv[3]) : 120;
    if (width <= 0 || height <= 0) {
        std::fprintf(stderr, "error: bad dimensions\n");
        return 1;
    }

    cv::Mat img(height, width, CV_8UC3);

    // Smooth colour ramps give the dog/snake transforms something to work on.
    for (int y = 0; y < height; ++y) {
        auto *row = img.ptr<cv::Vec3b>(y);
        for (int x = 0; x < width; ++x) {
            const int b = (x * 255) / (width - 1 > 0 ? width - 1 : 1);
            const int g = (y * 255) / (height - 1 > 0 ? height - 1 : 1);
            const int r =
                ((x + y) * 255) / ((width + height - 2) > 0 ? width + height - 2 : 1);
            row[x] = cv::Vec3b(static_cast<uchar>(b), static_cast<uchar>(g),
                               static_cast<uchar>(r));
        }
    }

    // Hard-edged shapes so Canny (cat-vision) has real edges to find.
    cv::rectangle(img, cv::Rect(width / 8, height / 8, width / 3, height / 3),
                  cv::Scalar(255, 255, 255), cv::FILLED);
    cv::rectangle(img, cv::Rect(width / 2, height / 2, width / 3, height / 3),
                  cv::Scalar(0, 0, 0), cv::FILLED);
    cv::circle(img, cv::Point((width * 3) / 4, height / 4), height / 6,
               cv::Scalar(0, 0, 255), cv::FILLED);
    cv::line(img, cv::Point(0, height - 1), cv::Point(width - 1, 0),
             cv::Scalar(0, 255, 0), 2);

    if (!cv::imwrite(argv[1], img)) {
        std::fprintf(stderr, "error: could not write %s\n", argv[1]);
        return 1;
    }
    std::printf("wrote %s (%dx%d)\n", argv[1], width, height);
    return 0;
}
