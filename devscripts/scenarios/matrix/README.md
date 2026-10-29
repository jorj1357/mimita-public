# Multi-client actor matrix

`actor-weapon-matrix.json` defines the required contact validation matrix:

- NPC vs NPC
- NPC vs player
- player vs NPC
- player vs player

across the standard weapon families and local/degraded network profiles.

The matrix is not a normal single-client scenario. It requires one server and N
clients, each with its own command queue, journal, and assertions. The runner is
being added separately; until it exists, use this file as the acceptance
contract and run the pairings manually with separate client journals.

Rules:

- one server
- one journal per client
- one server journal
- linked by run/server/client IDs
- no merged live journal
- fail if any client or the server disagrees on the authoritative outcome
