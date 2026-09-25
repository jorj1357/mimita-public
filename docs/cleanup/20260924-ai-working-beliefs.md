# AI working beliefs review

This is a simple review of the current working method, not a judgment about
the person using it.

## Belief: “Adding code first is faster.”

**Partly true.** A small experiment can be faster when the goal is unknown.
But adding code becomes expensive when the owner is unknown. Then every new
piece becomes another person touching the same toy.

**Better rule:** make the smallest working slice first, but write down its
owner and one acceptance test before expanding it.

## Belief: “Cleaning while implementing makes everything slower.”

**Usually true if cleanup means polishing everything.** That kind of cleanup
can stop progress.

**Better rule:** do not polish everything. Do a tiny “doorway cleanup” before
adding behavior: identify the owner, the input, the output, and the one place
that is allowed to change the state. Then continue building.

## Belief: “AI has gotten better, so old junk may not matter as much.”

**Only partly true.** Better AI can read more code, but duplicate owners still
make answers less reliable. More code means more possible answers to the same
question.

## Belief: “Fewer files automatically makes AI better.”

**Not always.** Five giant files can be harder than fifty small files. AI works
best when files have clear jobs, names, boundaries, and examples.

The real goal is not the fewest files. It is the fewest unclear choices.

## Recommended working rhythm

1. Choose one behavior.
2. Write the desired behavior in a short spec.
3. Find the current owner and all current writers.
4. Make one small implementation slice.
5. Test it.
6. Remove or redirect duplicate paths immediately if they are proven obsolete.
7. Record what remains before starting the next behavior.

This keeps “move fast” and “do not grow a junk pile” together.

