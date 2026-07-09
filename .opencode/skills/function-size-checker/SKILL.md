---
name: function-size-checker
description: Informational function size check. Reports sizes but never blocks.
---

# Function Size Checker

## Purpose
Report function sizes for awareness. This check is INFORMATIONAL ONLY and NEVER blocks a build.

A function may be as long as it needs to be to fulfill its single responsibility.

## Rules
| Lines    | Severity    |
|----------|-------------|
| any      | Informational only |

There are no enforced limits. Large functions are not inherently bad.

## Report
- function name
- file
- line count

## Action
Report only. Do not block builds based on function size.
