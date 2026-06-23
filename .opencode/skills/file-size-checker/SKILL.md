# File Size Checker

## Purpose
Prevent giant files.

## Rules
| Lines    | Severity    |
|----------|-------------|
| 1-300    | Ideal       |
| 300+     | Warning     |
| 500+     | High Risk   |
| 1000+    | Critical    |

## Special Rule
- `main.cpp` must be <= 100 lines.

## Report
For each oversized file, report:
- file
- line count
- likely reason for growth
- suggested split points

## Action
Report only. Do not edit code.
