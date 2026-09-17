#!/usr/bin/env bash
# Smoke + golden-image suite for VisionEngine.
#
#   run_tests.sh <VisionEngine> <avision_compare> <avision_make_fixture>
#
# Set AVISION_UPDATE_GOLDEN=1 to regenerate the golden images instead of
# asserting against them.
set -uo pipefail

ENGINE="${1:?path to VisionEngine required}"
COMPARE="${2:?path to avision_compare required}"
MAKE_FIXTURE="${3:?path to avision_make_fixture required}"

TESTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIXTURE_DIR="${TESTS_DIR}/fixtures"
GOLDEN_DIR="${TESTS_DIR}/golden"
FIXTURE="${FIXTURE_DIR}/sample.png"
MODES=(dog-vision cat-vision snake-vision)

WORK="$(mktemp -d)"
SERVER_PID=""

cleanup() {
  if [[ -n "${SERVER_PID}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
    kill "${SERVER_PID}" 2>/dev/null
    wait "${SERVER_PID}" 2>/dev/null
  fi
  rm -rf "${WORK}"
}
trap cleanup EXIT

PASS=0
FAIL=0

ok()   { PASS=$((PASS + 1)); printf '  ok   %s\n' "$1"; }
bad()  { FAIL=$((FAIL + 1)); printf '  FAIL %s\n' "$1"; }
note() { printf '\n== %s ==\n' "$1"; }

check() { # check <description> <expected_exit> <command...>
  local desc="$1" expected="$2"; shift 2
  local out rc
  out="$("$@" 2>&1)"; rc=$?
  if [[ "${rc}" -eq "${expected}" ]]; then
    ok "${desc}"
  else
    bad "${desc} (expected exit ${expected}, got ${rc})"
    printf '%s\n' "${out}" | sed 's/^/       /'
  fi
}

# --------------------------------------------------------------------------
note "fixture"
mkdir -p "${FIXTURE_DIR}" "${GOLDEN_DIR}"
if [[ ! -f "${FIXTURE}" ]]; then
  "${MAKE_FIXTURE}" "${FIXTURE}" >/dev/null || { echo "could not build fixture"; exit 1; }
  echo "  generated ${FIXTURE}"
else
  echo "  using ${FIXTURE}"
fi

# --------------------------------------------------------------------------
note "cli: each mode produces a correct image"
for mode in "${MODES[@]}"; do
  cp "${FIXTURE}" "${WORK}/sample.png"
  if ! "${ENGINE}" "${WORK}/sample.png" --mode "${mode}" >/dev/null 2>&1; then
    bad "${mode}: exits 0"
    continue
  fi
  ok "${mode}: exits 0"

  produced="${WORK}/sample_${mode}.png"
  if [[ ! -s "${produced}" ]]; then
    bad "${mode}: writes a non-empty file named sample_${mode}.png"
    continue
  fi
  ok "${mode}: writes a non-empty file named sample_${mode}.png"

  golden="${GOLDEN_DIR}/sample_${mode}.png"
  if [[ "${AVISION_UPDATE_GOLDEN:-0}" == "1" ]]; then
    cp "${produced}" "${golden}"
    ok "${mode}: golden refreshed"
  elif [[ ! -f "${golden}" ]]; then
    bad "${mode}: golden missing (run with AVISION_UPDATE_GOLDEN=1 to create it)"
  elif "${COMPARE}" "${produced}" "${golden}" "${AVISION_GOLDEN_TOLERANCE:-1.5}"; then
    ok "${mode}: matches golden"
  else
    bad "${mode}: differs from golden"
  fi
done

# --------------------------------------------------------------------------
note "cli: defaults and error paths"
cp "${FIXTURE}" "${WORK}/default.png"
check "no arguments exits 1"                 1 "${ENGINE}"
check "missing image file exits 2"           2 "${ENGINE}" "${WORK}/does-not-exist.png"
check "--version exits 0"                    0 "${ENGINE}" --version
check "--help exits 0"                       0 "${ENGINE}" --help
check "no --mode defaults to dog-vision"     0 "${ENGINE}" "${WORK}/default.png"

if [[ -s "${WORK}/default_dog-vision.png" ]]; then
  ok "default run writes default_dog-vision.png"
else
  bad "default run writes default_dog-vision.png"
fi

# Documented current behaviour: an unknown mode warns and copies the input
# through rather than failing.
cp "${FIXTURE}" "${WORK}/bogus.png"
check "unknown mode warns but exits 0"       0 "${ENGINE}" "${WORK}/bogus.png" --mode not-a-mode
if [[ -s "${WORK}/bogus_not-a-mode.png" ]]; then
  ok "unknown mode still writes an output"
else
  bad "unknown mode still writes an output"
fi

# --------------------------------------------------------------------------
note "http: endpoints"
if ! command -v curl >/dev/null 2>&1; then
  bad "curl is required for the HTTP tests"
else
  PORT=""
  for candidate in $(seq 18080 18095); do
    "${ENGINE}" serve --port "${candidate}" >"${WORK}/server.log" 2>&1 &
    SERVER_PID=$!
    for _ in $(seq 1 50); do
      if curl -fsS "http://127.0.0.1:${candidate}/healthz" >/dev/null 2>&1; then
        PORT="${candidate}"
        break
      fi
      if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
        break
      fi
      sleep 0.1
    done
    [[ -n "${PORT}" ]] && break
    kill "${SERVER_PID}" 2>/dev/null; wait "${SERVER_PID}" 2>/dev/null
    SERVER_PID=""
  done

  if [[ -z "${PORT}" ]]; then
    bad "server starts and answers /healthz"
    sed 's/^/       /' "${WORK}/server.log"
  else
    ok "server starts and answers /healthz on ${PORT}"
    BASE="http://127.0.0.1:${PORT}"

    status() { curl -s -o "$2" -w '%{http_code}' "${@:3}" "$1"; }

    code="$(status "${BASE}/version" "${WORK}/version.json")"
    if [[ "${code}" == "200" ]] && grep -q '"commit"' "${WORK}/version.json"; then
      ok "GET /version returns build metadata"
    else
      bad "GET /version returns build metadata (got ${code})"
    fi

    code="$(status "${BASE}/modes" "${WORK}/modes.json")"
    if [[ "${code}" == "200" ]] && grep -q 'snake-vision' "${WORK}/modes.json"; then
      ok "GET /modes lists the modes"
    else
      bad "GET /modes lists the modes (got ${code})"
    fi

    for mode in "${MODES[@]}"; do
      code="$(status "${BASE}/process?mode=${mode}" "${WORK}/http_${mode}.png" \
              -X POST -H 'Content-Type: application/octet-stream' \
              --data-binary "@${FIXTURE}")"
      if [[ "${code}" == "200" ]] && [[ -s "${WORK}/http_${mode}.png" ]]; then
        ok "POST /process?mode=${mode} returns an image"
      else
        bad "POST /process?mode=${mode} returns an image (got ${code})"
        continue
      fi
      golden="${GOLDEN_DIR}/sample_${mode}.png"
      if [[ -f "${golden}" ]] && [[ "${AVISION_UPDATE_GOLDEN:-0}" != "1" ]]; then
        if "${COMPARE}" "${WORK}/http_${mode}.png" "${golden}" \
             "${AVISION_GOLDEN_TOLERANCE:-1.5}" >/dev/null; then
          ok "HTTP ${mode} output matches the CLI golden"
        else
          bad "HTTP ${mode} output matches the CLI golden"
        fi
      fi
    done

    code="$(status "${BASE}/process?mode=nope" "${WORK}/err.json" \
            -X POST -H 'Content-Type: application/octet-stream' \
            --data-binary "@${FIXTURE}")"
    [[ "${code}" == "400" ]] && ok "unknown mode is rejected with 400" \
                             || bad "unknown mode is rejected with 400 (got ${code})"

    code="$(status "${BASE}/process" "${WORK}/err2.json" -X POST --data-binary "")"
    [[ "${code}" == "400" ]] && ok "empty body is rejected with 400" \
                             || bad "empty body is rejected with 400 (got ${code})"

    printf 'not an image' > "${WORK}/garbage.bin"
    code="$(status "${BASE}/process" "${WORK}/err3.json" \
            -X POST --data-binary "@${WORK}/garbage.bin")"
    [[ "${code}" == "400" ]] && ok "undecodable body is rejected with 400" \
                             || bad "undecodable body is rejected with 400 (got ${code})"

    code="$(status "${BASE}/process?mode=dog-vision" "${WORK}/multipart.png" \
            -X POST -F "image=@${FIXTURE}")"
    [[ "${code}" == "200" && -s "${WORK}/multipart.png" ]] \
      && ok "multipart upload is accepted" \
      || bad "multipart upload is accepted (got ${code})"

    code="$(status "${BASE}/nothing-here" "${WORK}/404" )"
    [[ "${code}" == "404" ]] && ok "unknown route returns 404" \
                             || bad "unknown route returns 404 (got ${code})"

    code="$(status "${BASE}/process?mode=dog-vision" "${WORK}/form.json" \
            -X POST -H 'Content-Type: application/x-www-form-urlencoded' \
            --data-binary "@${FIXTURE}")"
    if [[ "${code}" == "413" ]] && grep -q 'x-www-form-urlencoded' "${WORK}/form.json"; then
      ok "form-urlencoded body gives 413 with an explanatory message"
    else
      bad "form-urlencoded body gives 413 with an explanatory message (got ${code})"
    fi

    check "healthcheck subcommand succeeds against a live server" 0 \
          "${ENGINE}" healthcheck --port "${PORT}"
    check "healthcheck subcommand fails against a dead port"      1 \
          "${ENGINE}" healthcheck --port 18099
  fi
fi

# --------------------------------------------------------------------------
printf '\n---------------------------------------\n'
printf '%d passed, %d failed\n' "${PASS}" "${FAIL}"
[[ "${FAIL}" -eq 0 ]]
