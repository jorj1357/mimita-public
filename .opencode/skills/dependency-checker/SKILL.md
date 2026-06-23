# Dependency Checker

## Purpose
Prevent subsystem coupling.

## Bad (forbidden) dependencies
- physics -> ui
- physics -> menu
- physics -> replay
- physics -> website
- physics -> audio

## Good (allowed) dependencies
- physics -> math
- physics -> collision
- physics -> player

## Report
Report any dependency violations found in the codebase.

## Action
Report only. Do not edit code.
