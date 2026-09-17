#include <cstdlib>
#include <iostream>
#include <string>

#include "server.hpp"
#include "version.hpp"
#include "vision.hpp"

using std::cerr;
using std::cout;
using std::endl;
using std::string;

namespace {

int envInt(const char *name, int fallback) {
    const char *raw = std::getenv(name);
    if (raw == nullptr || *raw == '\0') return fallback;
    try {
        return std::stoi(raw);
    } catch (const std::exception &) {
        cerr << "Warning: ignoring invalid " << name << "='" << raw << "'" << endl;
        return fallback;
    }
}

string envStr(const char *name, const string &fallback) {
    const char *raw = std::getenv(name);
    return (raw == nullptr) ? fallback : string(raw);
}

void printUsage() {
    cout << "Usage:" << endl;
    cout << "  VisionEngine <image_path> [--mode MODE]   process one image" << endl;
    cout << "  VisionEngine serve [--port PORT]          run the HTTP backend" << endl;
    cout << "  VisionEngine healthcheck [--port PORT]    probe a running backend" << endl;
    cout << "  VisionEngine --version                    print build info" << endl;
    cout << endl;
    cout << "Modes: dog-vision | cat-vision | snake-vision" << endl;
}

int runServeCommand(int argc, char *argv[]) {
    avision::ServerOptions opts;
    // PORT is what most container platforms inject.
    opts.port = envInt("PORT", envInt("AVISION_PORT", opts.port));
    opts.host = envStr("AVISION_HOST", opts.host);
    opts.corsOrigin = envStr("AVISION_CORS_ORIGIN", opts.corsOrigin);

    for (int i = 2; i < argc; ++i) {
        const string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            opts.port = std::atoi(argv[++i]);
        } else if (arg == "--host" && i + 1 < argc) {
            opts.host = argv[++i];
        } else {
            cerr << "Error: unknown argument '" << arg << "'" << endl;
            return 1;
        }
    }

    if (opts.port <= 0 || opts.port > 65535) {
        cerr << "Error: invalid port " << opts.port << endl;
        return 1;
    }

    return avision::runServer(opts);
}

int runHealthcheckCommand(int argc, char *argv[]) {
    int port = envInt("PORT", envInt("AVISION_PORT", 8080));
    string host = envStr("AVISION_HOST", "127.0.0.1");

    for (int i = 2; i < argc; ++i) {
        const string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else {
            cerr << "Error: unknown argument '" << arg << "'" << endl;
            return 1;
        }
    }

    return avision::runHealthcheck(host, port);
}

int runCliCommand(int argc, char *argv[]) {
    string imagePath;
    string mode = "dog-vision"; // default

    for (int i = 1; i < argc; ++i) {
        const string arg = argv[i];

        if ((arg == "--mode" || arg == "-m") && i + 1 < argc) {
            mode = argv[++i];
        } else if (!arg.empty() && arg[0] != '-' && imagePath.empty()) {
            imagePath = arg;
        }
    }

    if (imagePath.empty()) {
        cerr << "Error: No image path provided." << endl;
        return 1;
    }

    cout << "Processing: " << imagePath << " | Mode: " << mode << endl;

    const cv::Mat src = cv::imread(imagePath, cv::IMREAD_COLOR);
    if (src.empty()) {
        cerr << "Error: Failed to load image: " << imagePath << endl;
        return 2;
    }

    cv::Mat output;
    avision::Mode parsed;
    if (avision::parseMode(mode, parsed)) {
        output = avision::simulate(src, parsed);
    } else {
        cerr << "Warning: Unknown mode '" << mode << "'. Using original image." << endl;
        output = src;
    }

    if (output.empty()) {
        cerr << "Error: Processing failed." << endl;
        return 3;
    }

    const string outPath = avision::makeOutputPath(imagePath, mode);

    if (!cv::imwrite(outPath, output)) {
        cerr << "Error: Failed to save output image: " << outPath << endl;
        return 4;
    }

    cout << "Output saved to: " << outPath << endl;
    return 0;
}

} // namespace

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    const string first = argv[1];

    if (first == "--help" || first == "-h") {
        printUsage();
        return 0;
    }

    if (first == "--version") {
        cout << "VisionEngine " << AVISION_VERSION << endl;
        cout << "commit:  " << AVISION_COMMIT << endl;
        cout << "built:   " << AVISION_BUILD_TIME << endl;
        cout << "opencv:  " << CV_VERSION << endl;
        return 0;
    }

    if (first == "serve") {
        return runServeCommand(argc, argv);
    }

    if (first == "healthcheck") {
        return runHealthcheckCommand(argc, argv);
    }

    return runCliCommand(argc, argv);
}
