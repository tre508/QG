---
type: testing
project: QG
created: 2026-10-20
updated: 2026-10-20
status: verified
tags:
  - testing
  - build
  - commands
source:
  - CMakeLists.txt
  - tests/CMakeLists.txt
  - GUIDE_STATE.md
---

# Build & Test Commands

## Build Commands

### Standard Build
```bash
cd /home/treshen/Projects/QG
cmake --build build
```

### Clean Build
```bash
cd /home/treshen/Projects/QG
rm -rf build
cmake -B build -S .
cmake --build build
```

### AddressSanitizer Build
```bash
cd /home/treshen/Projects/QG
cmake --build build-asan
```

### Install Targets (for packaging)
```bash
cd /home/treshen/Projects/QG
cmake --build build --target install
# Or for DEB packaging:
cpack -G DEB
```

## Test Commands

### Full Test Suite
```bash
cd /home/treshen/Projects/QG
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build
```

### Filtered Test Runs
```bash
# By keyword (ctest -R uses regex)
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build -R "playback"
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build -R "thumb"
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build -R "search"
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build -R "browser"
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build -R "composition"
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build -R "worker"
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build -R "core"
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build -R "persistence"
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build -R "mount"
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build -R "action"
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build -R "sort"
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build -R "probe"
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build -R "reel"
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build -R "resume"
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build -R "keyboard"
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build -R "performance"

# Exclude failing tests
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build -E "qg_qml|dragdrop"

# Verbose output for failures
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build --output-on-failure

# Rerun only failed tests
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build --rerun-failed --output-on-failure
```

### Run Specific Test Binary
```bash
# Performance benchmark
./build/tests/qg_performance_test

# Individual test binaries (from build/tests/)
./build/tests/test_browser_controller
./build/tests/test_thumb_scheduler
./build/tests/test_probe_memory
./build/tests/test_composition
# etc.
```

## GL Test Suite (with Xvfb)

### Standard GL Test
```bash
cd /home/treshen/Projects/QG
./tests/run_gl_test.sh
```

### GL Test with Options
```bash
# Specific scale factor
./tests/run_gl_test.sh --scale 1.25
./tests/run_gl_test.sh --scale 1.5
./tests/run_gl_test.sh --scale 2.0

# Specific test suite
./tests/run_gl_test.sh --suite playback
./tests/run_gl_test.sh --suite scaling

# Combined
./tests/run_gl_test.sh --scale 1.5 --suite scaling
```

### GL Test Script Options
| Option | Description |
|--------|-------------|
| `--scale <factor>` | Set QT_AUTO_SCREEN_SCALE_FACTOR (1.0, 1.25, 1.5, 2.0) |
| `--suite <name>` | Run specific test suite (playback, scaling, all) |
| `--help` | Show usage |

## Environment Variables

| Variable | Purpose | Typical Value |
|----------|---------|---------------|
| `QT_QPA_PLATFORM` | Qt platform plugin | `offscreen` (CI), `xcb` (desktop), `wayland` |
| `TMPDIR` | Temporary directory | `/tmp` (avoids /tmp permission issues in containers) |
| `QT_AUTO_SCREEN_SCALE_FACTOR` | High-DPI scaling | `1.0`, `1.25`, `1.5`, `2.0` |
| `LC_NUMERIC` | Numeric locale for mpv | `C` (set internally by MpvBackend) |

## Debug Commands

### Run with GDB
```bash
cd /home/treshen/Projects/QG
gdb --args ./build/QG
# Or for tests:
gdb --args ./build/tests/test_browser_controller
```

### Valgrind (Memory)
```bash
valgrind --leak-check=full --show-leak-kinds=all ./build/tests/test_browser_controller
```

### AddressSanitizer (Already in build-asan)
```bash
cd /home/treshen/Projects/QG
cmake --build build-asan
ASAN_OPTIONS=detect_leaks=1 ./build-asan/tests/test_browser_controller
```

## Benchmark Commands

### Generate Test Datasets
```bash
cd /home/treshen/Projects/QG
python3 scripts/generate_benchmark_dataset.py --count 50000 --output /tmp/qg_benchmark_50k
python3 scripts/generate_benchmark_dataset.py --count 100000 --output /tmp/qg_benchmark_100k
```

### Run Performance Test
```bash
cd /home/treshen/Projects/QG
# Cold cache (requires sudo for drop_caches)
sync && echo 3 | sudo tee /proc/sys/vm/drop_caches
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ./build/tests/qg_performance_test --path /tmp/qg_benchmark_50k

# Warm cache
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ./build/tests/qg_performance_test --path /tmp/qg_benchmark_50k
```

## Packaging Commands

### Build DEB Package
```bash
cd /home/treshen/Projects/QG
cmake --build build --target package
# Or:
cpack -G DEB
```

### Verify DEB
```bash
# Check contents
dpkg -c *.deb

# Install test
sudo dpkg -i *.deb
# Run installed app
QG
# Check desktop entry
xdg-mime query default video/mp4
```

## Development Workflow Commands

### Format Code (if clang-format configured)
```bash
cd /home/treshen/Projects/QG
find src qml -name "*.cpp" -o -name "*.hpp" -o -name "*.h" -o -name "*.qml" | xargs clang-format -i
```

### Static Analysis
```bash
# Clang tidy (if configured)
cd /home/treshen/Projects/QG/build
run-clang-tidy.py -p .
```

### Quick Sanity Check
```bash
cd /home/treshen/Projects/QG
cmake --build build && QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build -E "qg_qml|dragdrop" --output-on-failure
```

## CI/CD Equivalent

The pipeline runs these in sequence:
1. `cmake --build build` — Build
2. `QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build` — Unit/Integration tests
3. `./tests/run_gl_test.sh` — GL/Playback tests (Xvfb)
4. `./tests/run_gl_test.sh --scale 1.25` — Fractional scale test
5. `cpack -G DEB` — Package (if packaging validated)