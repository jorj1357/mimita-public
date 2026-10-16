# Gold reference: one prompt to a playable human-tested release

Status: Gold reference behavior
Date: 2026-10-15
Repository: `C:\mimita-v9`
Release: `v2.0.7`
Reported model context: GPT-5.6 Terra, High reasoning (user recollection; not independently verified)
Reported usage: approximately 55% of a 5-hour window (user estimate; not independently verified)

## Why this is gold

This is a reference for a successful end-to-end AI-assisted release loop. The
user reports that one prompt was enough to produce a working release, and that
the released game was then played with another real human for `40m 43s`. The
user described the result as very good, including the current gameplay content
such as Juggernaut.

The important result is not merely that code compiled. A downloadable release
was created, published, extracted, launched, and then accepted through a real
human multiplayer session.

## User-visible result

The current release is downloadable here:

- ZIP: <https://github.com/jorj1357/mimita-public/releases/download/v2.0.7/mimita-game-v2.0.7.zip>
- Release page: <https://github.com/jorj1357/mimita-public/releases/tag/v2.0.7>

The release contains the current game build and can be used to play the
current latest version, including the Juggernaut gameplay path. The user’s
reported acceptance was a real session with another human player lasting
`40m 43s`.

## Evidence boundary

### User-reported human acceptance

- One prompt produced the desired working-release outcome.
- The user downloaded and played the release with another human being.
- The session lasted `40m 43s`.
- The user judged the result very good.
- The user recalls GPT-5.6 Terra with High reasoning and approximately 55%
  usage of a 5-hour window.

The model setting and usage percentage are preserved as reported context, not
as independently verified telemetry. The session duration and gameplay
judgment are also a user acceptance report; they are not inferred from a build
log.

### Repository and release evidence

- The GitHub tag is `v2.0.7`.
- The published asset is `mimita-game-v2.0.7.zip`.
- The published ZIP digest is
  `sha256:6812eb0ca17c605e8e57a39639d57f384d316afa691e85890fa29514c732db6d`.
- The ZIP was cleanly extracted and its executable reported the expected
  release identity.
- The extracted dedicated server loaded `juggernaut6`, started Juggernaut
  mode, spawned two NPCs, registered a temporary room, and shut down cleanly.
- The public download endpoint returned HTTP 200 with the expected
  `218410900` byte asset.

These checks prove packaging, publication, extraction, executable identity,
and a bounded Juggernaut server path. They remain separate from the user’s
human gameplay acceptance.

## What made the one-prompt outcome work

1. The prompt named the real end goal: a downloadable release that works when
   played, not only a source change.
2. The agent worked from the current repository state and preserved existing
   gameplay systems instead of creating a parallel demo path.
3. The release was packaged as a single ZIP with a stable version and public
   download URL.
4. The extracted artifact was exercised through the actual Juggernaut server
   entry point.
5. The final proof included a real human multiplayer session, which is the
   strongest acceptance signal in this record.
6. Source, packaging, runtime, and human acceptance evidence stayed distinct,
   so a passing build was not mistaken for proof of gameplay.

## Reusable lesson

For a release-oriented game request, one clear prompt can be enough when it
states the user-visible finish line and the work remains coupled to the real
build, packaging, runtime, and acceptance loop:

```text
one clear release goal
    -> current implementation
    -> versioned ZIP
    -> clean extraction and runtime check
    -> real human play session
    -> downloadable release
```

This is a successful reference pattern, not a guarantee that every future
change will require only one prompt or that model usage will be identical.

## Does not claim

- It does not claim that GPT-5.6 Terra High or the reported usage percentage
  has been independently reconstructed from account telemetry.
- It does not claim that a clean build alone proves the full gameplay result.
- It does not replace future human acceptance for changes after `v2.0.7`.
- It does not require every feature or release to be completed in one prompt.
