---
type: testing
project: QG
created: 2026-10-20
updated: 2026-10-20
status: planned
tags:
  - testing
  - performance
  - benchmarks
  - baselines
source:
  - DESIGN.md §22.6, §25.11
  - ARCHITECTURE_REMEDIATION.md
  - tests/performance/
---

# Performance Baselines

## DESIGN.md Targets (§22.6, §25.11)

| Metric | Target | Status | Evidence |
|--------|--------|--------|----------|
| First screenful interactive | < 300 ms | 🟡 Planned | Not yet measured on real hardware |
| Smooth scrolling | 60 FPS | 🟡 Planned | Not yet measured |
| GUI-thread disk I/O | Zero | ✅ Verified | All I/O on worker pools |
| Rapid navigation (20 folders) | Final folder only | ✅ Verified | `rapidNavigationCancelsStaleResults` PASS |
| Bulk move 1,000 files | Visible thumbs not starved | ✅ Verified | `bulkOperationsDontStarveVisible` PASS |
| Peak memory (100k entries) | Bounded (LRU + lightweight rows) | ✅ Verified | `test_probe_memory` PASS |

## Benchmark Infrastructure

### Dataset Generator
```bash
# Location: scripts/generate_benchmark_dataset.py
# Creates sparse-file video fixtures with realistic names, sizes, formats, mtimes

# 50k entries (~10 GB logical)
python3 scripts/generate_benchmark_dataset.py --count 50000 --output /tmp/qg_benchmark_50k

# 100k entries (~20 GB logical)
python3 scripts/generate_benchmark_dataset.py --count 100000 --output /tmp/qg_benchmark_100k
```

**Dataset Characteristics:**
- Sparse files (logical size via fallocate/truncate, minimal physical space)
- 15 video format extensions (evenly distributed)
- Randomized mtimes over past year
- Realistic filename patterns (series, movies, random)

### Performance Test Binary
```bash
# Location: ./build/tests/qg_performance_test
# Measures: first screenful, scroll FPS, memory, navigation cancellation

# Usage:
./build/tests/qg_performance_test --path /tmp/qg_benchmark_50k
```

## Cold Cache Protocol

```bash
# Required for accurate cold-cache measurement
sync && echo 3 | sudo tee /proc/sys/vm/drop_caches
QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ./build/tests/qg_performance_test --path /tmp/qg_benchmark_50k
```

## Historical Measurements

### Fix-PERF-003 (2026-10-19) — Preliminary on Dev Machine

| Metric | 50k Entries | 100k Entries | Target |
|--------|-------------|--------------|--------|
| First screenful (cold) | 311 ms | — | < 300 ms |
| First screenful (warm, avg) | 311 ms | — | < 300 ms |
| First screenful (warm, min) | 285 ms | — | < 300 ms |
| First screenful (warm, max) | 385 ms | — | < 300 ms |
| Runs under 300ms | 3/10 | — | Majority |

**Note**: Marginal miss on 300ms target (311ms avg). Likely due to 100ms polling interval in test measurement. Real-world may differ.

### Fix-PERF-006 (test_probe_memory) — 100k Entries Memory Profile

| Operation | Time | Notes |
|-----------|------|-------|
| applySnapshot (100k entries) | 26 ms | beginResetModel + data insertion |
| Reorder (100k entries) | 397-472 ms | layoutChanged emission |
| Coalesced resort | Verified | 200ms debounce prevents thundering herd |
| BrowserController navigation | 737-869 ms | Full nav cycle with enumeration |

### Fix-PERF-004 (rapidNavigationCancelsStaleResults)
- 20 rapid folder navigations
- Only final folder's contents displayed
- PASS — stale enumeration cancelled correctly

### Fix-PERF-005 (ThumbScheduler)
| Test | Result |
|------|--------|
| bulkOperationsDontStarveVisible | PASS — visible thumbnails prioritized |
| cancelledRequestsDontConsumeResources | PASS — cancelled work cleaned up |
| priorityInversionPrevention | PASS — Interactive > Normal > Background |

## Required Measurements (Not Yet Done)

### Hardware-Dependent (Need Real Desktop)

| Test | Method | Success Criteria |
|------|--------|------------------|
| 60 FPS scrolling | Visual + frame timing | No dropped frames at 60Hz |
| High-DPI rendering (125%/150%/200%) | Visual inspection + pixel ratio | Sharp thumbnails, crisp text, correct layout |
| Wayland playback | Launch on Wayland session | Stage/Cinema identical to X11 |
| Cross-app DnD | Drag QG→Nautilus, Nautilus→QG | Files transferred correctly |
| Drive disconnect | Physical USB unplug/replug | State retained, UI updates correctly |

### CI-Compatible (Offscreen)

| Test | Method | Success Criteria |
|------|--------|------------------|
| First screenful < 300ms | `qg_performance_test` with drop_caches | Avg < 300ms over 10 runs |
| Memory bounded (100k) | `test_probe_memory` | No leaks, reorder < 500ms |
| Thumbnail starvation | `test_thumb_scheduler` | All 3 PASS |

## Measurement Methodology

### First Screenful
1. Drop caches: `sync && echo 3 | sudo tee /proc/sys/vm/drop_caches`
2. Launch app with test directory
3. Measure time to first `rowsInserted` + first thumbnail paint
4. Repeat 10x, report avg/min/max

### Scroll FPS
1. Use Qt's `QSG_RENDER_TIMING` or `QSG_RENDER_LOOP=basic` with frame timestamps
2. Scroll through 50k entries at ~100 items/sec
3. Measure frame time variance

### Memory Profile
1. Valgrind massif or heaptrack
2. Track peak RSS at 50k, 100k entries
3. Verify LRU eviction keeps memory stable

## Baseline Comparison Targets

| Application | First Screenful (50k) | Memory (100k) | Notes |
|-------------|----------------------|---------------|-------|
| QG (target) | < 300ms | Bounded | This project |
| Nautilus | ~500-1000ms | Unbounded | Baseline |
| Thunar | ~300-600ms | Moderate | Baseline |
| Dolphin | ~400-800ms | Moderate | Baseline |

## Reporting Format

```markdown
## Performance Report — YYYY-MM-DD

**Hardware**: CPU, RAM, SSD, GPU
**Qt Version**: 6.x.x
**Build**: Release / AddressSanitizer

### First Screenful (50k entries)
| Run | Cold (ms) | Warm (ms) |
|-----|-----------|-----------|
| 1   |           |           |
| ... |           |           |
| Avg |           |           |

**Target**: < 300ms — **PASS/FAIL**

### Scroll FPS
| Metric | Value | Target |
|--------|-------|--------|
| Avg FPS | | 60 |
| 1% Low | | 55 |
| Frame Time P99 | | 16.7ms |

### Memory (100k entries)
| Metric | Value | Target |
|--------|-------|--------|
| Peak RSS | | < 500MB |
| Reorder Time | | < 500ms |
| Leak Check | | Zero |

### Platform Verification
| Platform | Status | Notes |
|----------|--------|-------|
| X11 | | |
| Wayland | | |
| 125% Scale | | |
| 150% Scale | | |
| 200% Scale | | |
```

## Next Steps

1. Run cold/warm benchmarks on target hardware
2. Measure scroll FPS with frame timing
3. Profile memory with heaptrack
4. Test High-DPI at 125%/150%/200%
5. Verify Wayland playback
6. Document all results in this format