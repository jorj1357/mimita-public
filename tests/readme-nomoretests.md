instead, diagnose problem, add logging that logs to events.jsonl, build the newest .exe, run it, go into the game and run a scenario/test for it, look at the live events.jsonl file for the results of changes and logging you added, and edit/tweak until the behavior/numbers match spec or ideal behavior 

from C:\mimita-v9\docs\jorj-docs\20261006plan.md

last update 2026 10 06 1042 jorj 

todo explain better this ideal behavior, but, 

no more things like "14/14 checks passed" then live gameplay behavior is not the ideal behavior 

more things like launch the actual .exe, check the events.jsonl live for values and things going on in the actual game, use that to make code changes, repeat until either desired behavior is reached with proven specific line in the events.jsonl that proves that the changes worked, or , context runs out? and then write what seems best to do next, e.g. we need to add logging to figure out if XYZ is aactually doing what we want it to do 