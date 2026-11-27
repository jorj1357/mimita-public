# Long-term account and currency data architecture note

**Date:** 2026-11-26
**Scope:** Architecture guidance and issue context only

## Changed

- Added the user's account-durability question to shared context and appended a design proposal to existing issue `issue-29115dfd03db`.
- The proposal separates authentication, authoritative currency transactions, and private profile/recovery data; it calls for replicated/quorum-committed transaction history, encrypted backups with separate recovery keys, portable schemas, crypto agility, and restore drills.
- Recorded that signatures/hashes provide integrity or authorship evidence but do not alone prove honest gameplay, ownership, confidentiality, availability, or double-spend prevention.

## Evidence

- Read the persistent player-data, world-project/distributed-storage, and existing RPG economy specifications/issues.
- Reviewed NIST digital identity and key-management guidance and RFC 9106 (Argon2).
- No password/authentication, payment, storage, game source, build, or runtime system was changed or exercised.
