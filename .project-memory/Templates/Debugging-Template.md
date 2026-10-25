---
type: template
project: QG
created: 2026-10-20
updated: 2026-10-20
status: template
tags:
  - template
  - debugging
---

# Debugging Template

## Issue: [TITLE]

**ID**: DEBUG-XXX
**Date**: YYYY-MM-DD
**Status**: [Investigating | Root Cause Found | Fixed | Won't Fix]
**Reporter**: [Name]
**Severity**: [Critical | High | Medium | Low]

## Symptom

What is observed? Include error messages, logs, stack traces.

## Environment

- OS: 
- Qt Version: 
- Build: [Release/Debug/ASAN]
- Hardware: 

## Reproduction Steps

1. 
2. 
3. 

## Hypotheses

| # | Hypothesis | Evidence For | Evidence Against |
|---|------------|--------------|------------------|
| 1 | | | |
| 2 | | | |
| 3 | | | |

## Investigation Log

| Date | Action | Result |
|------|--------|--------|
| | | |

## Root Cause

What is the actual cause? Reference code location.

## Fix

What was changed? Files modified.

## Verification

- [ ] Unit test added/updated
- [ ] Integration test passes
- [ ] Manual verification
- [ ] Regression test passes

## Prevention

How to prevent recurrence? (Code pattern, test, review checklist)

## Related Issues

- DEBUG-XXX
- DEC-XXX (if architectural)

## References

- Source file:line
- DESIGN.md §X.Y
- Stack trace / log excerpt