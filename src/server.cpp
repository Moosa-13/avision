#include "server.hpp"

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include <httplib.h>

#include "vision.hpp"
#include "version.hpp"

namespace avision {
namespace {

std::string jsonEscape(const std::string &s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}

void sendJson(httplib::Response &res, int status, const std::string &body) {
    res.status = status;
    res.set_content(body, "application/json");
}

void sendError(httplib::Response &res, int status, const std::string &message) {
    sendJson(res, status, "{\"error\":\"" + jsonEscape(message) + "\"}");
}

std::string modesJsonArray() {
    std::ostringstream os;
    os << "[";
    const auto &names = modeNames();
    for (size_t i = 0; i < names.size(); ++i) {
        if (i) os << ",";
        os << "\"" << names[i] << "\"";
    }
    os << "]";
    return os.str();
}

// Body may arrive either as a multipart "image" field (what a browser
// FormData upload sends) or as the raw request body (curl --data-binary).
// find() hands back a reference into the Request rather than the copy
// get_file() would return, so a 16MB upload is not duplicated.
const std::string *extractImageBytes(const httplib::Request &req) {
    const auto it = req.form.files.find("image");
    if (it != req.form.files.end()) {
        return &it->second.content;
    }
    if (!req.body.empty()) {
        return &req.body;
    }
    return nullptr;
}

} // namespace

int runServer(const ServerOptions &opts) {
    httplib::Server server;

    server.set_payload_max_length(opts.maxUploadBytes);

    if (!opts.corsOrigin.empty()) {
        server.set_post_routing_handler(
            [&opts](const httplib::Request &, httplib::Response &res) {
                res.set_header("Access-Control-Allow-Origin", opts.corsOrigin);
                res.set_header("Access-Control-Allow-Headers", "Content-Type");
                res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            });
        server.Options(R"(/.*)", [](const httplib::Request &, httplib::Response &res) {
            res.status = 204;
        });
    }

    server.Get("/healthz", [](const httplib::Request &, httplib::Response &res) {
        res.set_content("ok\n", "text/plain");
    });

    // Lets you confirm which build is actually live -- the check that makes a
    // rollback verifiable rather than hopeful.
    server.Get("/version", [](const httplib::Request &, httplib::Response &res) {
        std::ostringstream os;
        os << "{"
           << "\"version\":\"" << jsonEscape(AVISION_VERSION) << "\","
           << "\"commit\":\"" << jsonEscape(AVISION_COMMIT) << "\","
           << "\"built\":\"" << jsonEscape(AVISION_BUILD_TIME) << "\","
           << "\"opencv\":\"" << jsonEscape(CV_VERSION) << "\""
           << "}";
        sendJson(res, 200, os.str());
    });

    server.Get("/modes", [](const httplib::Request &, httplib::Response &res) {
        sendJson(res, 200, "{\"modes\":" + modesJsonArray() + "}");
    });

    server.Post("/process", [&opts](const httplib::Request &req, httplib::Response &res) {
        const std::string modeParam =
            req.has_param("mode") ? req.get_param_value("mode") : "dog-vision";

        Mode mode;
        if (!parseMode(modeParam, mode)) {
            sendError(res, 400,
                      "unknown mode '" + modeParam + "'; supported: " + modesJsonArray());
            return;
        }

        const std::string format =
            req.has_param("format") ? req.get_param_value("format") : "png";
        if (format != "png" && format != "jpg") {
            sendError(res, 400, "unknown format '" + format + "'; supported: png, jpg");
            return;
        }

        const std::string *bytes = extractImageBytes(req);
        if (bytes == nullptr) {
            sendError(res, 400,
                      "no image in request; send multipart field 'image' or a raw body");
            return;
        }

        const cv::Mat encoded(1, static_cast<int>(bytes->size()), CV_8U,
                              const_cast<char *>(bytes->data()));
        const cv::Mat src = cv::imdecode(encoded, cv::IMREAD_COLOR);
        if (src.empty()) {
            sendError(res, 400, "could not decode image");
            return;
        }

        const size_t pixels =
            static_cast<size_t>(src.rows) * static_cast<size_t>(src.cols);
        if (pixels > opts.maxPixels) {
            sendError(res, 413,
                      "image too large: " + std::to_string(src.cols) + "x" +
                          std::to_string(src.rows) + " exceeds the pixel limit");
            return;
        }

        cv::Mat output;
        try {
            output = simulate(src, mode);
        } catch (const cv::Exception &e) {
            sendError(res, 500, std::string("processing failed: ") + e.what());
            return;
        }

        if (output.empty()) {
            sendError(res, 500, "processing produced an empty image");
            return;
        }

        std::vector<uchar> buf;
        const std::string ext = (format == "jpg") ? ".jpg" : ".png";
        if (!cv::imencode(ext, output, buf)) {
            sendError(res, 500, "could not encode output image");
            return;
        }

        res.set_header("X-Avision-Mode", modeName(mode));
        res.set_content(reinterpret_cast<const char *>(buf.data()), buf.size(),
                        (format == "jpg") ? "image/jpeg" : "image/png");
    });

    // httplib rejects some requests before any route runs -- notably a body
    // sent as application/x-www-form-urlencoded, which it caps at 8KB. Without
    // this, those callers get a bare status and no explanation.
    server.set_error_handler([](const httplib::Request &req, httplib::Response &res) {
        if (!res.body.empty()) {
            return httplib::Server::HandlerResponse::Unhandled; // a route already
                                                                // answered
        }

        std::string message = "request failed";
        if (res.status == 413) {
            const std::string ct = req.get_header_value("Content-Type");
            if (ct.rfind("application/x-www-form-urlencoded", 0) == 0) {
                message = "body was sent as application/x-www-form-urlencoded, which is "
                          "capped at 8KB; send the image as multipart field 'image' or "
                          "set Content-Type: application/octet-stream";
            } else {
                message = "request body too large";
            }
        }

        sendError(res, res.status, message);
        return httplib::Server::HandlerResponse::Handled;
    });

    server.set_exception_handler(
        [](const httplib::Request &, httplib::Response &res, std::exception_ptr ep) {
            std::string what = "unhandled error";
            try {
                std::rethrow_exception(ep);
            } catch (const std::exception &e) {
                what = e.what();
            } catch (...) {
            }
            sendError(res, 500, what);
        });

    std::printf("VisionEngine %s (%s) listening on %s:%d\n", AVISION_VERSION,
                AVISION_COMMIT, opts.host.c_str(), opts.port);
    std::fflush(stdout);

    if (!server.listen(opts.host, opts.port)) {
        std::fprintf(stderr, "Error: could not bind %s:%d\n", opts.host.c_str(),
                     opts.port);
        return 1;
    }
    return 0;
}

int runHealthcheck(const std::string &host, int port, int timeoutSeconds) {
    // 0.0.0.0 is a bind address, not a destination.
    const std::string target = (host == "0.0.0.0" || host.empty()) ? "127.0.0.1" : host;

    httplib::Client client(target, port);
    client.set_connection_timeout(timeoutSeconds, 0);
    client.set_read_timeout(timeoutSeconds, 0);

    const auto res = client.Get("/healthz");
    if (!res) {
        std::fprintf(stderr, "healthcheck: no response from %s:%d\n", target.c_str(),
                     port);
        return 1;
    }
    if (res->status != 200) {
        std::fprintf(stderr, "healthcheck: %s:%d returned %d\n", target.c_str(), port,
                     res->status);
        return 1;
    }
    return 0;
}

} // namespace avision
