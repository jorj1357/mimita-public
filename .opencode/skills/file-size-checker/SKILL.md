---
name: file-size-checker
description: Informational file size check. Reports sizes but never blocks. One file = one task.
---

# File Size Checker

## Purpose
Report file sizes for awareness. This check is INFORMATIONAL ONLY and NEVER blocks a build.

One file = one task. A single coherent task may be any length.

## Rules
| Lines    | Severity    |
|----------|-------------|
| any      | Informational only |

There are no enforced limits. Large files are not inherently bad.

## Report
- file
- line count

## Action
Report only. Do not block builds based on file size.
