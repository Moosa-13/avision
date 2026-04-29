#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

static string makeOutputPath(const string &inputPath, const string &mode) {
    size_t pos = inputPath.find_last_of(".");
    string suffix = "_" + mode;

    if (pos == string::npos) {
        return inputPath + suffix + ".jpg";
    }

    return inputPath.substr(0, pos) + suffix + inputPath.substr(pos);
}

// ---------- DOG VISION ----------
static Mat simulateDogVision(const Mat &src) {
    Mat img;
    cvtColor(src, img, COLOR_BGR2RGB);
    img.convertTo(img, CV_32F, 1.0/255.0);

    // Better dichromatic approximation (blue-yellow dominant)
    Matx33f dogMatrix(
        0.3f, 0.6f, 0.1f,   // R reduced
        0.3f, 0.6f, 0.1f,   // G dominates
        0.0f, 0.2f, 0.8f    // B preserved
    );

    Mat transformed;
    transform(img, transformed, dogMatrix);

    // blur (lower acuity)
    GaussianBlur(transformed, transformed, Size(5,5), 1.2);

    // slightly brighter
    transformed *= 1.1;

    transformed.convertTo(transformed, CV_8UC3, 255.0);
    cvtColor(transformed, transformed, COLOR_RGB2BGR);

    return transformed;
}

// ---------- CAT VISION ----------
static Mat simulateCatVision(const Mat &src) {
    Mat img;
    src.convertTo(img, CV_32F, 1.0/255.0);

    // brighten (low-light vision)
    img *= 1.3;

    // desaturate slightly
    Mat gray, gray3;
    cvtColor(img, gray, COLOR_BGR2GRAY);
    cvtColor(gray, gray3, COLOR_GRAY2BGR);

    Mat colored;
    addWeighted(img, 0.6, gray3, 0.4, 0.0, colored);

    // edge detection (on original uint8 image)
    Mat edges;
    Canny(src, edges, 80, 160);

    // convert edges → float and 3 channels
    edges.convertTo(edges, CV_32F, 1.0/255.0);
    cvtColor(edges, edges, COLOR_GRAY2BGR);

    // now types match → safe
    Mat result;
    addWeighted(colored, 0.8, edges, 0.2, 0.0, result);

    result.convertTo(result, CV_8UC3, 255.0);
    return result;
}

// ---------- SNAKE (THERMAL STYLE) ----------
static Mat simulateSnakeVision(const Mat &src) {
    Mat gray;
    cvtColor(src, gray, COLOR_BGR2GRAY);

    // normalize intensity
    Mat normalized;
    normalize(gray, normalized, 0, 255, NORM_MINMAX);

    // apply heatmap
    Mat heatmap;
    applyColorMap(normalized, heatmap, COLORMAP_JET);

    return heatmap;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Usage: VisionEngine <image_path> [--mode MODE]" << endl;
        cout << "Modes: dog-vision | cat-vision | snake-vision" << endl;
        return 1;
    }

    string imagePath;
    string mode = "dog-vision"; // default

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];

        if ((arg == "--mode" || arg == "-m") && i + 1 < argc) {
            mode = argv[++i];
        } else if (arg[0] != '-' && imagePath.empty()) {
            imagePath = arg;
        }
    }

    if (imagePath.empty()) {
        cerr << "Error: No image path provided." << endl;
        return 1;
    }

    cout << "Processing: " << imagePath << " | Mode: " << mode << endl;

    // Load image
    Mat src = imread(imagePath, IMREAD_COLOR);
    if (src.empty()) {
        cerr << "Error: Failed to load image: " << imagePath << endl;
        return 2;
    }

    Mat output;

    // Mode selection
    if (mode == "dog-vision") {
        output = simulateDogVision(src);
    } 
    else if (mode == "cat-vision") {
        output = simulateCatVision(src);
    } 
    else if (mode == "snake-vision") {
        output = simulateSnakeVision(src);
    } 
    else {
        cerr << "Warning: Unknown mode '" << mode << "'. Using original image." << endl;
        output = src;
    }

    if (output.empty()) {
        cerr << "Error: Processing failed." << endl;
        return 3;
    }

    // Output path
    string outPath = makeOutputPath(imagePath, mode);

    if (!imwrite(outPath, output)) {
        cerr << "Error: Failed to save output image: " << outPath << endl;
        return 4;
    }

    cout << "Output saved to: " << outPath << endl;

    return 0;
}