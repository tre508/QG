---
type: template
project: QG
created: 2026-10-20
updated: 2026-10-20
status: template
tags:
  - template
  - task
---

# Task Template

## Task: [TITLE]

**ID**: [FIX-XXX | CHECKPOINT-XXX | FI-01 | RV-01]
**Work Item**: [from dev-guide.md]
**Status**: [Not Started | In Progress | Blocked | Complete]
**Assigned**: [Agent/Session]
**Started**: YYYY-MM-DD
**Completed**: YYYY-MM-DD

## Objective

What is the specific goal? Reference dev-guide.md section.

## Acceptance Criteria

- [ ] Criterion 1 (verifiable)
- [ ] Criterion 2 (verifiable)
- [ ] Criterion 3 (verifiable)

## Scope

### In Scope
- 

### Out of Scope
- 

## Dependencies

| Dependency | Status | Notes |
|------------|--------|-------|
| | | |

## Implementation Plan

| Step | Action | Verification |
|------|--------|--------------|
| 1 | | |
| 2 | | |
| 3 | | |

## Files to Modify

| File | Change Type | Notes |
|------|-------------|-------|
| | | |

## Verification Gates

### Build
- [ ] `cmake --build build` PASS

### Tests
- [ ] Unit tests: `ctest -R "<keyword>"` PASS
- [ ] Integration tests: `ctest -R "<keyword>"` PASS
- [ ] QML tests: `ctest -R "qg_qml"` PASS
- [ ] GL tests: `./tests/run_gl_test.sh` PASS

### Performance (if applicable)
- [ ] First screenful < 300ms
- [ ] 60 FPS scroll
- [ ] Memory bounded

### Documentation
- [ ] GUIDE_STATE.md updated
- [ ] DECISIONS.md updated (if architectural)
- [ ] REQUIREMENTS_MATRIX.md updated
- [ ] Obsidian notes updated

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| | | | |

## Blockers

- 

## Notes

- 

## References

- dev-guide.md §X.Y
- DESIGN.md §X.Y
- Related task: [ID]