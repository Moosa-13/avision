# VisionEngine

A **C++ image processing service** built with OpenCV that simulates how different
animals perceive visual information.

It runs two ways from one binary: as a **CLI** for batch processing, and as an
**HTTP backend** for a web UI.

---

## 🚀 Features

* Multiple vision simulation modes:
  * 🐶 **Dog Vision** – reduced red sensitivity, blur, slight brightness boost
  * 🐱 **Cat Vision** – enhanced low-light perception with edge emphasis
  * 🐍 **Snake Vision** – thermal-style heatmap simulation
* Fast native processing using **OpenCV**
* CLI *and* HTTP interfaces sharing one processing core
* Containerised, with build → test → publish → rollback wired up in CI

---

## 🛠️ Build

### In a container (recommended)

No local OpenCV needed — everything is pinned in the image:

```bash
podman build -t avision .          # or: docker build -t avision .
```

### Natively with sdc

The build is defined in [`Proto.xml`](Proto.xml) and driven by **sdc**, which
lives in [its own repo](https://github.com/Moosa-13/sdc) so it can be shared
across projects:

```bash
git clone https://github.com/Moosa-13/sdc.git && (cd sdc && ./install.sh)
```

Then, with OpenCV development headers and pkg-config present:

```bash
# Debian/Ubuntu
sudo apt install -y build-essential pkg-config python3 libopencv-dev
# Arch
sudo pacman -S base-devel pkg-config python opencv

sdc build     # libavision.so + the executables
sdc -t        # the test binaries
sdc -b        # both
sdc run       # execute the test suite
sdc clean     # remove build/
```

Artifacts land at `build/lib/libavision.so` and `build/bin/VisionEngine`. The
executable carries an `$ORIGIN/../lib` rpath, so it finds the library beside it
without `LD_LIBRARY_PATH`.

---

## ▶️ Usage

### CLI

```bash
./build/bin/VisionEngine <image_path> [--mode <mode>]
```

```bash
./build/bin/VisionEngine images/input.jpg --mode dog-vision
```

Output is written next to the input as `<input_name>_<mode>.<ext>`.
If no mode is given, the default is `dog-vision`.

### HTTP backend

```bash
./build/bin/VisionEngine serve --port 8080
# or
podman run --rm -p 8080:8080 avision
```

| Method | Route       | Purpose                                            |
| ------ | ----------- | -------------------------------------------------- |
| `GET`  | `/healthz`  | Liveness probe; returns `ok`                        |
| `GET`  | `/version`  | Version, commit, build time, OpenCV version         |
| `GET`  | `/modes`    | The supported mode names                            |
| `POST` | `/process`  | Process an image and return it                      |

`POST /process` takes the image either as a multipart `image` field or as the raw
request body, and accepts two query parameters:

* `mode` — `dog-vision` (default), `cat-vision`, or `snake-vision`
* `format` — `png` (default) or `jpg`

```bash
# raw body -- the Content-Type matters, see below
curl -X POST -H "Content-Type: application/octet-stream" --data-binary @cat.jpg \
  "http://localhost:8080/process?mode=snake-vision" -o out.png

# multipart, as a browser FormData upload sends it
curl -X POST -F "image=@cat.jpg" \
  "http://localhost:8080/process?mode=cat-vision&format=jpg" -o out.jpg
```

> **Set a Content-Type on raw-body uploads.** curl's `--data-binary` defaults to
> `application/x-www-form-urlencoded`, which the HTTP layer parses as a form and
> caps at 8KB — so anything bigger comes back `413`, with a response body saying
> so. Send `application/octet-stream`, the real image type, or use multipart.
> Browser `FormData` and `fetch` with a `Blob` both do the right thing already.

Configuration is by environment variable:

| Variable              | Default   | Meaning                                     |
| --------------------- | --------- | ------------------------------------------- |
| `PORT`                | `8080`    | Listen port (what most platforms inject)    |
| `AVISION_HOST`        | `0.0.0.0` | Bind address                                |
| `AVISION_CORS_ORIGIN` | `*`       | `Access-Control-Allow-Origin`; empty disables CORS |

---

## 🧪 Tests

```bash
sdc -b     # build the code and the test binaries
sdc run    # execute the suite
```

The suite covers every mode end to end, the CLI's exit codes and error paths,
and each HTTP endpoint against a live server. Mode outputs are compared against
golden images in `tests/golden/` using a mean-absolute-difference tolerance, so
real regressions fail while last-bit noise across CPUs and OpenCV point releases
does not.

The fixture and goldens are generated, not hand-placed — `avision_make_fixture`
builds the input image from a pure function of `(x, y)`, so it reproduces
anywhere. After an intentional change to the image maths, refresh them:

```bash
AVISION_UPDATE_GOLDEN=1 sdc run
```

and commit the updated `tests/golden/*.png` alongside the change.

---

## 🎨 Formatting

C++ sources are formatted with **clang-format**, configured in `.clang-format`
to match the style already in `src/` rather than impose a new one.

```bash
scripts/format.sh           # rewrite files in place
scripts/format.sh --check   # report unformatted files, change nothing
```

To format automatically on every push, install the repo's git hooks once:

```bash
scripts/install-hooks.sh
```

This points `core.hooksPath` at `.githooks/`. The `pre-push` hook then formats
the tracked C++ sources before anything leaves your machine.

**If formatting changes something, the push is stopped.** That is deliberate:
the commits being pushed already exist, so reformatting at push time cannot
alter them — letting the push through would send the unformatted code anyway
and leave you with modified files. The hook fixes the files and asks you to
commit them:

```bash
git add -u && git commit -m 'Apply clang-format' && git push
```

Bypass once with `git push --no-verify`; disable with
`git config --unset core.hooksPath`. If clang-format isn't installed the hook
warns and lets the push through rather than blocking you.

`third_party/` is excluded — vendored code keeps upstream's style — and the dog
matrix in `src/vision.cpp` is wrapped in `// clang-format off` so it stays laid
out as a 3×3 matrix.

> ⚠️ **clang-format output varies between major versions.** Everyone working on
> the repo should use the same one, or you'll get formatting churn passing back
> and forth. These files were formatted with **clang-format 22**. Set
> `CLANG_FORMAT=/path/to/clang-format-22` to pick a specific binary. For the same
> reason there's no CI formatting check — it would fail purely on version drift
> unless the version is pinned in the workflow first.

---

## 🔁 CI/CD

`.github/workflows/ci.yml` runs on every push and PR to `main`:

1. **Build** the image.
2. **Test** — `sdc run` runs inside the `test` build stage, so a suite failure
   fails the build and the image is never pushed.
3. **Publish** to `ghcr.io/moosa-13/avision` on `main`, tagged with the
   immutable `sha-<short>` plus `latest`.
4. **Deploy** — retags that exact digest as `production`. Nothing is rebuilt
   between test and deploy, so what goes live is the artifact that passed.

Pull requests build and test but never push.

The build image installs sdc by cloning its repo at a **pinned tag**
(`SDC_REF`, currently `v1.0.0`). That pin matters: an unpinned clone would let a
change to the build tool silently alter what this image produces. To move to a
newer sdc, bump `SDC_REF` in the `Dockerfile` as a deliberate commit.

### Rollback

`.github/workflows/rollback.yml`, run manually from the Actions tab, moves
`production` back to an earlier build. It retags the previous image **by
digest** — the old artifact itself is restored, not a fresh compile of old
source.

* Leave **target** empty and it finds the build immediately before the current
  `production` automatically.
* Or give it a specific `sha-a1b2c3d` tag or a `sha256:` digest.
* **dry_run** resolves and verifies the target without moving anything.

Both deploy and rollback finish by running `VisionEngine --version` against the
`production` tag, so the logs show which commit is actually live.

---

## 🧱 Project Structure

```text
avision/
├── Proto.xml               # the build definition sdc reads
├── Dockerfile              # builder → test → slim runtime stages
├── src/
│   ├── vision.{hpp,cpp}    # the perception models
│   ├── server.{hpp,cpp}    # HTTP backend
│   ├── main.cpp            # CLI and subcommand dispatch
│   └── version.hpp.in      # build metadata template
├── tests/
│   ├── run_tests.sh        # the suite
│   ├── make_fixture.cpp    # deterministic fixture generator
│   ├── compare.cpp         # tolerance-based image comparison
│   ├── fixtures/           # generated input image
│   └── golden/             # expected outputs per mode
├── scripts/
│   ├── format.sh           # clang-format, in place or --check
│   └── install-hooks.sh    # points core.hooksPath at .githooks/
├── .githooks/pre-push      # formats C++ before a push leaves the machine
├── .clang-format           # style config
├── third_party/httplib.h   # cpp-httplib v0.56.0, vendored
└── .github/workflows/      # ci.yml, rollback.yml
```

---

## ⚠️ Notes

* Input must be an image OpenCV can decode; the server answers `400` when it
  cannot, and `413` past the pixel limit.
* Uploads are capped at 16 MB and decoded images at 40 MP.
* An unknown `--mode` on the **CLI** warns and passes the original image
  through; the **HTTP** API rejects it with `400`.

---

## 🔮 Future Improvements

* Additional vision modes (infrared, UV approximation)
* Batch image processing
* Real-time webcam support
* AI-based perception models
