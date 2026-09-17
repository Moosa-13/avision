#pragma once

#include <cstddef>
#include <string>

namespace avision {

struct ServerOptions {
    std::string host = "0.0.0.0";
    int port = 8080;
    // Rejected before the body is buffered.
    size_t maxUploadBytes = 16u * 1024u * 1024u;
    // Rejected after decode, to bound per-request processing cost.
    size_t maxPixels = 40u * 1000u * 1000u;
    // Value for Access-Control-Allow-Origin; empty disables CORS headers.
    std::string corsOrigin = "*";
};

// Blocks until the server stops. Returns a process exit code.
int runServer(const ServerOptions &opts);

// GETs /healthz on localhost:port. Returns 0 when the server answers 200.
// Used as the container HEALTHCHECK so the runtime image needs no curl.
int runHealthcheck(const std::string &host, int port, int timeoutSeconds = 3);

} // namespace avision
