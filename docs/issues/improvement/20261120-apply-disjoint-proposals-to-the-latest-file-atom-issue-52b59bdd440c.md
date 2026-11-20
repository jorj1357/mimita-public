# issue-52b59bdd440c: Apply disjoint proposals to the latest file atomically

## Human report (verbatim, immutable)

> The current edit propose workflow uses the separate-patch approach, but the final apply still requires a free file lease. To allow truly concurrent final updates, the coordinator would need to apply each patch to the latest file under a brief lock, then write the merged result atomically.

