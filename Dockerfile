# syntax=docker/dockerfile:1

# ---------------------------------------------------------------- builder ---
FROM debian:bookworm-slim AS builder

# python3 is sdc's runtime; pkg-config is how it resolves <Packages>.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        git \
        libopencv-dev \
        pkg-config \
        python3 \
    && rm -rf /var/lib/apt/lists/*

# sdc lives in its own repo so it can be shared across projects. Pinned to a
# tag: an unpinned clone would let a change to the build tool silently alter
# what this image produces.
ARG SDC_REPO=https://github.com/Moosa-13/sdc.git
ARG SDC_REF=v1.0.0

RUN git clone --depth 1 --branch "${SDC_REF}" "${SDC_REPO}" /tmp/sdc \
    && install -m 0755 /tmp/sdc/sdc /usr/local/bin/sdc \
    && rm -rf /tmp/sdc \
    && sdc --version

WORKDIR /src

# Build definition first, so editing sources does not invalidate its layer.
COPY Proto.xml ./
COPY third_party/ third_party/
COPY src/ src/
COPY tests/ tests/

# CI passes the real commit; otherwise sdc falls back to git, then "unknown".
ARG AVISION_COMMIT=""
ARG SOURCE_DATE_EPOCH=""

ENV SDC_COMMIT=${AVISION_COMMIT}

# -b builds the library, the executables and the test binaries.
RUN sdc -b

# ------------------------------------------------------------------- test ---
# Built with --target test. Failing tests fail the build, so nothing that
# fails its suite can reach the registry.
FROM builder AS test

RUN apt-get update && apt-get install -y --no-install-recommends curl \
    && rm -rf /var/lib/apt/lists/*

RUN sdc run

# ---------------------------------------------------------------- runtime ---
FROM debian:bookworm-slim AS runtime

# Only the three OpenCV runtime libraries the binary actually links against --
# which is why Proto.xml names components explicitly rather than taking
# everything opencv4.pc advertises.
RUN apt-get update && apt-get install -y --no-install-recommends \
        libopencv-core406 \
        libopencv-imgproc406 \
        libopencv-imgcodecs406 \
    && rm -rf /var/lib/apt/lists/*

RUN useradd --system --create-home --uid 10001 avision
USER avision

# The executable is linked with -Wl,-rpath,$ORIGIN/../lib, so bin/ and lib/
# under the same prefix is all it needs to find the library.
COPY --from=builder /src/build/lib/libavision.so /usr/local/lib/libavision.so
COPY --from=builder /src/build/bin/VisionEngine  /usr/local/bin/VisionEngine

ENV PORT=8080 \
    AVISION_HOST=0.0.0.0 \
    AVISION_CORS_ORIGIN=*

EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=5s --start-period=5s --retries=3 \
    CMD ["/usr/local/bin/VisionEngine", "healthcheck"]

ENTRYPOINT ["/usr/local/bin/VisionEngine"]
CMD ["serve"]
