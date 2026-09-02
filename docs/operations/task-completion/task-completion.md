# Bug Report and Explanation Standards

When reporting a bug or explaining a fix, ALWAYS show:

1. **The exact code path that is wrong** — include file path, line numbers, and the actual code. Example:
   - `src/effects/effect-part-render.cpp:307` renders `damage_impact_sphere` using `drawFilledSphereOriented` with `impact.localDimensions`
   - The `death_ellipsoid` branch at `effect-part-render.cpp:297` uses `drawFilledSphere` with a `scaleVec` that stretches along world axes, not the hit direction

2. **The exact code that should replace it** — include the full replacement snippet, not vague descriptions. Example:
   ```cpp
   // WRONG: stretches along world axes
   glm::vec3 scaleVec = dir * (len / std::max(rad, 0.001f)) + glm::vec3(1.0f) - dir;
   DebugVis::drawFilledSphere(camera, effect.position, rad, drawColor, scaleVec);

   // CORRECT: oriented along hit direction
   DebugVis::drawFilledSphereOriented(camera, center, axis, 1.0f, drawColor, dims, localAxis);
   ```

3. **Evidence from logs or code logic** — explain WHY the current code fails and WHY the fix works, citing specific function behavior, data flow, or log output. Example:
   - "The `death_ellipsoid` path computes `scaleVec` from a direction vector but applies it as a non-uniform scale in world space. When the hit direction is `(0.7, 0.7, 0)`, the scaleVec becomes `(0.7, 0.7, 1.0)` which stretches the sphere diagonally in XY but not along the actual hit axis. `drawFilledSphereOriented` builds an orthonormal basis from the direction vector and remaps via `impactBasis()`, so the sphere's local Z pole always aligns with the hit direction regardless of world-space orientation."

Vague statements like "this rendering looks wrong" or "try a different approach" are insufficient. Always trace the exact data flow from input to output.

# Mandatory Overseer Check

Before marking ANY task as complete, ALWAYS run `python overseer.py` from the workspace root.

Every checker must pass. If any checker reports a finding — fix it and re-run.

Do not claim work is complete while `overseer.py` returns anything other than `Overall Status: PASS`.

This is the final quality gate. Nothing overrides it.

# TASK COMPLETION REQUIREMENTS

Before ending any task:

1. **Run specialized skills** — Load and execute any skill relevant to the work (collision, physics, state, dependencies, etc.). See "Specialized Skills Check" in the Overseer skill.
2. **Build** if code changed (using `build_agent.py`)
3. **Run relevant validation/tests**
4. **Verify expected outputs exist**

## Final Validation (Required)

Before considering ANY task complete, always execute:

```
python overseer.py
```

Every checker must pass.

Do not claim work is complete while any checker reports failures.

If a checker fails:

1. Fix the reported issues.
2. Run overseer.py again.
3. Repeat until every checker passes.

5. **Save logs**
6. **Trigger completion notification script**
7. **Print summary**

Agents should never simply stop after editing files.

They must validate work first.

If Overseer cannot be loaded: treat this as a FAILURE condition. Run the diagnostics section below and report the exact reason.

---

# TASK COMPLETION HOOK

When work is complete, run:

```
python devscripts/agent_task_complete.py [task_name]
```

Do NOT use `devscripts\agent_finish.bat` — it can cause AI agents to hang.

This will:

* Play a completion sound (`assets/sound/entity/player/spawning.wav`)
* Show a Windows toast notification ("MiMITA Agent: Task Completed")
* Print `[AGENT COMPLETE]` to the console

## Example

After building and verifying:

```
python build_agent.py
:: check for SUCCESS
python devscripts/agent_task_complete.py "Fix duel replay flow"
```

## Failure handling

If the sound file is missing: a warning is printed.
If the notification fails: a warning is printed.
The script never crashes.

---

# Mandatory Final Check

Before completing any task that modifies code, configuration, scripts, documentation, or build files, always execute:

```
python overseer.py
```

Every checker must pass.

## Zero-Tolerance Policy

Overseer is the final authority. Nothing is considered complete until `python overseer.py` returns:

```
