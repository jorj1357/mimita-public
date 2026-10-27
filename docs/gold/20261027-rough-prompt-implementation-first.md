# Gold note: Start uncertain features with a rough prompt

- **Date:** 2026-10-27 (player feedback; exact time not supplied)
- **Audience:** AI coding agents and the player
- **Purpose:** Preserve a working preference for turning spontaneous ideas into implementation.
- **This note does not:** Approve a specific feature, define a vehicle design, guarantee that a short prompt is sufficient for every task, or replace implementation evidence and human review.

## Player feedback

The player gets ideas at random while working on something else, such as thinking of a horror idea while working on animations. They want to be able to say the rough idea in ordinary speech, even if the description is incomplete, and ask AI to add it to the open gameplay issues tracker.

For a large, uncertain feature such as vehicles, the player would currently rather start with a direct prompt like “put vehicles in the game” than spend time writing a very long plan to predict every unknown and bug in advance. Their preferred loop is to let AI begin implementation, discover the real questions while building, and refine the feature from concrete results. While AI works, the player can work on complementary material outside the executable, including maps, 3D models, sounds, and further ideas.

The player sees Blender animation export as a strong example of this spoken workflow: they could ask AI to export an animation and use it as a named move for Smash Melee, without personally directing each file path, JSON edit, and export adjustment. They recall this working especially well with GPT-6 Luna at medium reasoning, possibly GPT-5.6 Luna; the exact model used is uncertain. This is personal experience, not a comparative model evaluation.

## Working lesson

For exploratory work, treat a rough spoken request as a valid starting point. Add the idea to the tracker when the player asks, then begin useful implementation from the request rather than requiring a complete design up front. Let implementation and concrete play reveal unknowns, and bring material decisions back to the player when they genuinely affect scope or behavior. Keep the source of truth, focused validation, runtime evidence, and human acceptance appropriate to the change.

This is a preference for discovery through implementation, not a blanket rule to skip all planning. Small reversible implementation steps can expose hidden requirements sooner than speculative detail, while high-impact decisions still need clarification when they cannot be inferred safely.

## Related records

- [Open gameplay issues](../specs/open-gameplay-issues.md): vehicle-focused gamemode and this implementation-first preference.
- [Blender Animation Authoring and Import](../specs/animations/blender-animation-authoring.md).
