# Function Size Checker

## Purpose
Prevent giant functions.

## Rules
| Lines    | Severity    |
|----------|-------------|
| 1-30     | Ideal       |
| 50+      | Warning     |
| 100+     | High Risk   |
| 200+     | Critical    |

## Report
For each oversized function, report:
- function name
- file
- line count
- nested depth
- suggested extraction opportunities

## Action
Report only. Do not edit code.
