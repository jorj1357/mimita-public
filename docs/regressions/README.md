# Regression Records

Use this folder only for confirmed behavior breaks discovered through human
review or playtesting.

Normal AI work history belongs in `docs/changelog/`, not here.

Every regression record must include:

- observed date and time;
- expected behavior;
- actual behavior;
- exact specification used;
- exact wrong code;
- exact corrected code;
- confirmed cause;
- fix;
- proof; and
- the related changelog file.

`regressions-v1.md` is append-only. Never rewrite or delete an older entry.
