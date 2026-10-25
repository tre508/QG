#!/usr/bin/env bash
# Runs Qt Quick OpenGL tests with virtual X display and fractional scaling support.
# Usage: run_gl_test.sh [--scale 1.0|1.25|1.5|2.0] <test-binary> [test-args...]
#        run_gl_test.sh --suite [--scale 1.0|1.25|1.5|2.0]  # runs full perf/scaling suite
set -euo pipefail

SCALE="${QG_TEST_SCALE:-1.0}"
RUN_SUITE=false
BIN=""
ARGS=()

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --scale)
            SCALE="${2:?--scale requires value}"
            shift 2
            ;;
        --suite)
            RUN_SUITE=true
            shift
            ;;
        --help|-h)
            cat <<EOF
Usage: $(basename "$0") [--scale 1.0|1.25|1.5|2.0] <test-binary> [args...]
       $(basename "$0") --suite [--scale 1.0|1.25|1.5|2.0]

Environment variables:
  QG_TEST_SCALE      Default scale factor (1.0, 1.25, 1.5, 2.0)
  QG_XVFB_DISPLAY    Xvfb display number (default: :99)
  QG_XVFB_RESOLUTION Base resolution (default: 1920x1080x24)
  QG_BUILD_DIR       Build directory (default: ./build)

Examples:
  $(basename "$0") --scale 1.5 ./build/tests/qg_performance_test
  $(basename "$0") --suite --scale 1.25
  QG_TEST_SCALE=2.0 $(basename "$0") ./build/tests/qg_qml_test -input tests/qml/tst_scaling.qml
EOF
            exit 0
            ;;
        *)
            BIN="$1"
            shift
            ARGS=("$@")
            break
            ;;
    esac
done

# Validate scale
case "$SCALE" in
    1.0|1.25|1.5|2.0) ;;
    *) echo "ERROR: Invalid scale '$SCALE'. Must be 1.0, 1.25, 1.5, or 2.0" >&2; exit 1 ;;
esac

BUILD_DIR="${QG_BUILD_DIR:-./build}"
XVFB_DISPLAY="${QG_XVFB_DISPLAY:-:99}"
BASE_RES="${QG_XVFB_RESOLUTION:-1920x1080x24}"

# Calculate scaled resolution for xrandr
IFS='x' read -r BASE_W BASE_H BASE_D <<< "$BASE_RES"
SCALED_W=$(awk "BEGIN {printf \"%.0f\", $BASE_W * $SCALE}")
SCALED_H=$(awk "BEGIN {printf \"%.0f\", $BASE_H * $SCALE}")

cleanup() {
    if [[ -n "${XVFB_PID:-}" ]]; then
        kill "$XVFB_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

# Start Xvfb if no DISPLAY OR if fractional scaling requested (needs virtual display for xrandr)
USE_XVFB=false
if [[ -z "${DISPLAY:-}" || "$SCALE" != "1.0" ]]; then
    USE_XVFB=true
    if ! command -v Xvfb >/dev/null 2>&1; then
        echo "SKIP: no DISPLAY and Xvfb not installed" >&2
        exit 0
    fi
    if ! xdpyinfo -display "$XVFB_DISPLAY" >/dev/null 2>&1; then
        echo "Starting Xvfb on $XVFB_DISPLAY with base resolution $BASE_RES..."
        Xvfb "$XVFB_DISPLAY" -screen 0 "$BASE_RES" +extension GLX +extension RANDR +render -noreset \
            >/dev/null 2>&1 &
        XVFB_PID=$!
        for _ in $(seq 1 50); do
            xdpyinfo -display "$XVFB_DISPLAY" >/dev/null 2>&1 && break
            sleep 0.1
        done
    else
        echo "Reusing existing Xvfb on $XVFB_DISPLAY"
        # Get PID of existing Xvfb on this display
        XVFB_PID=$(pgrep -f "Xvfb.*$XVFB_DISPLAY" | head -1 || echo "")
    fi
    export DISPLAY="$XVFB_DISPLAY"
fi

# Configure fractional scaling
# For Xvfb: rely on QT_AUTO_SCREEN_SCALE_FACTOR (xrandr --scale not supported on Xvfb)
# For real X servers: use xrandr --scale
if [[ -n "${XVFB_PID:-}" ]]; then
    echo "Xvfb detected: using QT_AUTO_SCREEN_SCALE_FACTOR=${SCALE} (xrandr --scale not supported on Xvfb)"
elif command -v xrandr >/dev/null 2>&1; then
    echo "Configuring xrandr scale ${SCALE}x (${SCALED_W}x${SCALED_H})..."
    # Get output name
    OUTPUT=$(xrandr --display "$DISPLAY" | awk '/ connected/ {print $1; exit}')
    if [[ -n "$OUTPUT" ]]; then
        xrandr --display "$DISPLAY" --output "$OUTPUT" --mode "${BASE_W}x${BASE_H}" \
            --scale "${SCALE}x${SCALE}" --panning "${SCALED_W}x${SCALED_H}" \
            --fb "${SCALED_W}x${SCALED_H}" 2>/dev/null || \
        xrandr --display "$DISPLAY" --output "$OUTPUT" --scale "${SCALE}x${SCALE}" 2>/dev/null || \
        echo "Warning: xrandr scale failed, falling back to QT_AUTO_SCREEN_SCALE_FACTOR" >&2
    fi
fi

# Qt High-DPI environment
export LC_NUMERIC=C
export QT_QPA_PLATFORM=xcb
export QT_AUTO_SCREEN_SCALE_FACTOR="$SCALE"
export QT_SCREEN_SCALE_FACTORS="$SCALE"
# Qt 6: PassThrough preserves fractional scales (default)
export QT_SCALE_FACTOR_ROUNDING_POLICY=PassThrough

# Verify effective DPR
if command -v xdpyinfo >/dev/null 2>&1; then
    if [[ -n "${XVFB_PID:-}" ]]; then
        # On Xvfb, DPI is fixed at 96; Qt scales via QT_AUTO_SCREEN_SCALE_FACTOR
        echo "Configured scale: ${SCALE}x | Base: ${BASE_W}x${BASE_H} | Scaled FB: ${SCALED_W}x${SCALED_H} | Qt DPR: ${SCALE} (via QT_AUTO_SCREEN_SCALE_FACTOR)"
    else
        ACTUAL_DPR=$(xdpyinfo -display "$DISPLAY" 2>/dev/null | awk '/resolution/ {print $2}' | cut -d'x' -f1 | awk "{print \$1/96}" 2>/dev/null || echo "$SCALE")
        echo "Configured scale: ${SCALE}x | Base: ${BASE_W}x${BASE_H} | Scaled FB: ${SCALED_W}x${SCALED_H} | Effective DPR: ~${ACTUAL_DPR}"
    fi
else
    echo "Configured scale: ${SCALE}x | Base: ${BASE_W}x${BASE_H} | Scaled FB: ${SCALED_W}x${SCALED_H}"
fi

# Run single test binary
run_test() {
    local test_bin="$1"
    shift
    local test_args=("$@")

    if [[ ! -x "$test_bin" ]]; then
        echo "ERROR: Test binary not found or not executable: $test_bin" >&2
        return 1
    fi

    echo "=== Running: $test_bin ${test_args[*]} ==="
    "$test_bin" "${test_args[@]}"
}

# Run full performance/scaling suite
run_suite() {
    local failed=0
    local passed=0

    echo "========== QG PERFORMANCE & SCALING TEST SUITE =========="
    echo "Scale: ${SCALE}x | Display: ${DISPLAY} | Build: ${BUILD_DIR}"
    echo "=========================================================="

    # 1. Performance benchmarks
    echo ""
    echo "[1/6] Performance benchmarks (qg_performance_test)..."
    if run_test "$BUILD_DIR/tests/qg_performance_test"; then
        ((passed++))
    else
        ((failed++))
        echo "FAILED: qg_performance_test"
    fi

    # 2. Rapid navigation
    echo ""
    echo "[2/6] Rapid navigation (qg_browser_controller_test rapidNavigationCancelsStaleResults)..."
    if run_test "$BUILD_DIR/tests/qg_browser_controller_test" rapidNavigationCancelsStaleResults; then
        ((passed++))
    else
        ((failed++))
        echo "FAILED: rapidNavigationCancelsStaleResults"
    fi

    # 3. Thumbnail priority scheduling
    echo ""
    echo "[3/6] Thumbnail priority scheduling (qg_thumb_scheduler_test)..."
    if run_test "$BUILD_DIR/tests/qg_thumb_scheduler_test"; then
        ((passed++))
    else
        ((failed++))
        echo "FAILED: qg_thumb_scheduler_test"
    fi

    # 4. 100k-entry memory + coalesced resort
    echo ""
    echo "[4/6] 100k memory & coalesced resort (qg_probe_memory_test)..."
    if run_test "$BUILD_DIR/tests/qg_probe_memory_test"; then
        ((passed++))
    else
        ((failed++))
        echo "FAILED: qg_probe_memory_test"
    fi

    # 5. Probe wiring (coalesced resort)
    echo ""
    echo "[5/6] Probe wiring / coalesced resort (qg_probe_wiring_test)..."
    if run_test "$BUILD_DIR/tests/qg_probe_wiring_test"; then
        ((passed++))
    else
        ((failed++))
        echo "FAILED: qg_probe_wiring_test"
    fi

    # 6. High-DPI scaling tests
    echo ""
    echo "[6/6] High-DPI scaling (qg_qml_test tst_scaling.qml)..."
    if run_test "$BUILD_DIR/tests/qg_qml_test" -input "$(dirname "$0")/qml/tst_scaling.qml"; then
        ((passed++))
    else
        ((failed++))
        echo "FAILED: qg_qml_test tst_scaling.qml"
    fi

    echo ""
    echo "========== SUITE SUMMARY =========="
    echo "Scale: ${SCALE}x"
    echo "Passed: $passed"
    echo "Failed: $failed"
    echo "==================================="

    if [[ $failed -gt 0 ]]; then
        return 1
    fi
    return 0
}

# Execute
if [[ "$RUN_SUITE" == true ]]; then
    run_suite
else
    if [[ -z "$BIN" ]]; then
        echo "ERROR: No test binary specified. Use --suite or provide a binary." >&2
        exit 1
    fi
    run_test "$BIN" "${ARGS[@]}"
fi