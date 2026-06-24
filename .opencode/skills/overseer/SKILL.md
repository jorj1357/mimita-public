# Overseer

## Purpose

Overseer is not an analysis skill.
Overseer is not a planning skill.
Overseer is not a coding skill.

Overseer is an orchestrator.

Its entire purpose is to invoke all repository health skills and aggregate their output.

## Invoked Skills

- dependency-checker
- duplicate-state-checker
- file-size-checker
- function-size-checker
- physics-architecture-checker
- repo-hygiene-checker

## Behavior

Do not duplicate logic from invoked skills.
Do not reimplement existing checks.
Do not copy code from other skills.

Run each skill, collect its findings, and present one combined report.

## Missing Skills

If any of the six skill directories is missing, report it by name in the report.
Do not silently ignore missing skills.

## Output Format

```
================================
OVERSEER REPORT
===============

## Dependency Issues

(output from dependency-checker)

## Duplicate State Issues

(output from duplicate-state-checker)

## File Size Issues

(output from file-size-checker)

## Function Size Issues

(output from function-size-checker)

## Physics Architecture Issues

(output from physics-architecture-checker)

## Repository Hygiene Issues

(output from repo-hygiene-checker)

================================

# Summary

Critical

High

Medium

Low

Recommended Actions
```

## Trigger

Typing `Use Overseer` automatically executes all six skills and returns the combined report.
