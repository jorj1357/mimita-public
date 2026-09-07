// 09 03 2026, 15 41
/* purpose
* check that project documentation is clear, current, and routed correctly
* ensure specifications remain the desired behavior source of truth
* identify contradictions, missing routes, and stale instructions
* this skill DOES NOT silently rewrite specifications to match code
* this skill DOES NOT treat archive material as current authority
* this skill DOES NOT replace human approval of desired behavior
*/

# Documentation Checker v1

Check the changed documents and their links. Confirm that each file has a clear
purpose, a DOES NOT section, and an obvious audience. Confirm that router links
point to real current files and that instructions do not contradict higher-level
specifications. Flag duplicate rules, stale commands, unexplained exceptions,
and claims unsupported by the repository. Report exact paths, headings, and
recommended smallest corrections.

# Todo Checker 

Check the entire \docs directory in the repo to see if there are any comments such as "todo: explain this better" or "todo: clean this document up" or "todo: add an explanation for (insert thing)" in any document. Note these, with exact code paths and line numbers and quotes of waht it says, with a suggestion as to what we should replace the todo with.