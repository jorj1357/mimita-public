# Quality tooling

These scripts are report-only by default. They exist so MiMITA can find dead
code, duplicate owners, forbidden damage writes, and missing actor/weapon/network
coverage without relying on memory.

Run:

```text
python devscripts/quality/lint.py --all
python devscripts/quality/lint.py --gate
```

Individual reports:

```text
python devscripts/quality/check_damage_authority.py
python devscripts/quality/check_contact_ownership.py
python devscripts/quality/dead_code_report.py
python devscripts/quality/duplicate_report.py
python devscripts/quality/coverage_report.py
```

Recommended rollout:

1. report-only baseline
2. blocking on changed files
3. blocking on the full tree after the legacy baseline is clean

Rules and the multi-client matrix: `docs/specs/dev-loop/quality-gates.md`.

## External tools

The scripts use these when installed and degrade to repository-local checks when
they are absent:

- `clang-format`
- `clang-tidy`
- `cppcheck`
- `jscpd` or PMD CPD

Do not add a new exception to silence a finding. Fix the owner or update the
specification first.
