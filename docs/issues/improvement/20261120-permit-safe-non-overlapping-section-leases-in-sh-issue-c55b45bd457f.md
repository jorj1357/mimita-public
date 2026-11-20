# issue-c55b45bd457f: Permit safe non-overlapping section leases in shared files

## Human report (verbatim, immutable)

> can u make it so like , check the lease but if u arent editing the same section then just edit the file anyway , or coordinate with the lease owner on a together cahnge , is that doable


## Triage

### 2026-11-20T04:06:29.347Z — coordination-region-leases

Summary: Make same-file collaboration less blocking without risking lost edits. The shared checkout gives agents the same physical file, so arbitrary concurrent writes to different logical sections can still overwrite one another. Improve the lease-conflict path so an agent can prepare a disjoint patch while the owner holds the file, request a coordinated handoff, then apply/reconcile under a serialized file lease. Preserve default one-writer protection and explicit shared-region acceptance.
Owner: coordination-region-leases
Priority: high
