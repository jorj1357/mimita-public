# **MiMITA VIP System Specification**

## **1\. Goal**

Create a cosmetic-only VIP system that lets players financially support MiMITA and deeply customize how their identity appears throughout MiMITA.

VIP must **never provide a gameplay advantage**.

VIP must never:

* Increase damage.  
* Increase movement capability.  
* Increase health.  
* Improve networking priority.  
* Improve matchmaking.  
* Remove intentionally-created gameplay annoyances.  
* Unlock stronger weapons.  
* Provide competitive information unavailable to normal players.  
* Reduce gameplay restrictions applied to normal players.

Money buys expression, customization, cosmetics, supporter recognition, and future cosmetic content.

**There must never be pay-to-win.**

---

# **2\. Fundamental Name Rule**

A MiMITA username is not plain text with a color occasionally attached to it.

Every visible user identity is rendered through the universal MiMITA `NameStyle` system.

## **Absolute rule**

> If a user's name is visible anywhere in MiMITA, the user's current NameStyle must be applied there.

This includes current and future MiMITA systems.

Examples:

* Above the player character.  
* In-game chat.  
* Killfeed.  
* Scoreboard.  
* Spectator UI.  
* Main menu.  
* Lobby.  
* Duel queue.  
* Match results.  
* Friend lists.  
* Player lists.  
* Server browser.  
* Website profile.  
* Website leaderboards.  
* Website menus.  
* Forums.  
* Forum posts.  
* Private messages.  
* Notifications.  
* Search results.  
* Replays.  
* Historical posts/content.  
* Any future place where a MiMITA user identity appears.

Historical content resolves the user's **current** appearance.

Example:

A user writes a forum post in January with a turquoise name.

In June they change the name to an animated rainbow.

The January forum post should now display their current animated rainbow style.

No subsystem should permanently store a rendered username appearance inside the content.

Store the `user_id`.

Resolve the current NameStyle when rendering it.

---

# **3\. One Authoritative NameStyle Schema**

The website and `.exe` are separate renderers of the same truth.

There should be:

* One authoritative NameStyle schema.  
* One authoritative database representation.  
* One authoritative validation implementation on trusted MiMITA infrastructure.  
* One implementation of the renderer for the website.  
* One implementation of the renderer for the `.exe`.  
* Future renderers must consume the same schema.

The website and game do not need to share rendering source code.

They must share the **behavioral contract and data**.

Same NameStyle input should produce visually equivalent output.

---

# **4\. No Hardcoded VIP Appearance Rules**

VIP defaults, limitations, colors, tier capabilities, animation limits, performance settings, reserved colors, badge IDs, and similar configuration must not be scattered throughout source code.

They must come from authoritative configuration.

Configuration should be:

* JSON/config/database driven.  
* Versioned.  
* Hot reloadable where practical.  
* Validated when loaded.  
* Able to fall back to the last valid configuration if a new configuration is invalid.

Code should implement the rules.

Configuration should define the values.

For example, do not independently hardcode turquoise in five game files and four website files.

Define the turquoise VIP default once.

---

# **5\. VIP Tiers**

## **VIP**

Monthly price:

**$3.33**

Features:

* Fixed turquoise name style.  
* VIP badge.  
* Colored name everywhere.  
* Access to all normal VIP settings.  
* Can enable or disable the VIP appearance while membership is active.

Default style:

**Turquoise**

---

# **6\. Super VIP**

Monthly price:

**$8.88**

Includes everything in VIP.

Additional features:

* Custom solid name color.  
* Rainbow names.  
* Change between solid and rainbow.  
* Change selected color at any time.  
* Additional whole-name formatting as supported.  
* Reserved staff colors cannot be selected by non-staff users.

Default:

**Rainbow**

---

# **7\. Ultra VIP**

Monthly price:

**$17.77**

Includes everything in VIP and Super VIP.

Ultra VIP is intended to eventually become an extremely detailed identity editor.

Version-one functionality:

* Per-character colors.  
* Per-character bold.  
* Per-character italic.  
* Per-character strikethrough.  
* Editable rainbow speed.  
* Editable rainbow direction.  
* Animated names.  
* Multiple saved presets.  
* Whole-name styles.  
* Per-letter styles.

Future supported properties may include:

* Per-character effects.  
* Gradients.  
* Color cycling.  
* Glow.  
* Letter movement.  
* Letter rotation.  
* Letter scale.  
* Waves.  
* Pulses.  
* Sparkle effects.  
* Additional animation types.  
* Font selection.  
* Additional typography properties.

Default:

**Standard animated rainbow**

---

# **8\. Lifetime VIP**

Lifetime VIP exists for every tier.

Prices:

| Tier | Lifetime Price |
| ----- | ----- |
| Lifetime VIP | $111.11 |
| Lifetime Super VIP | $222.22 |
| Lifetime Ultra VIP | $333.33 |

Lifetime ownership never expires.

Do not implement lifetime by inserting an impossibly large timestamp into the database.

Use explicit state such as:

`is_lifetime = true`

and:

`expires_at = null`

The interface may deliberately display something playful such as:

`Forever ∞`

or:

`12/31/9999+ ∞`

Lifetime VIP has a special lifetime version of its tier badge.

Examples:

* Lifetime VIP ∞ badge.  
* Lifetime Super VIP ∞ badge.  
* Lifetime Ultra VIP ∞ badge.

Lifetime users may later receive exclusive permanent cosmetics.

Possible examples:

* Animated cape.  
* Special hat.  
* Particle cosmetic.  
* Gold-dust cosmetic.  
* Sparkle cosmetic.  
* Animated/breathing texture.  
* Other visually distinctive permanent supporter cosmetics.

These are cosmetic only.

---

# **9\. Badges**

Each normal tier has its own badge.

Each lifetime tier has its own special lifetime badge.

Existing image files include:

`C:\mimita-priv-v8\website\public\assets\images\mimita vip 2.png`

`C:\mimita-priv-v8\website\public\assets\images\mimita super vip.png`

`C:\mimita-priv-v8\website\public\assets\images\mimita ultra vip.png`

Lifetime variants may be added separately.

Multiple independent badges may appear.

Example:

A MiMITA administrator with Lifetime Ultra VIP may display:

* Administrator badge.  
* Lifetime Ultra VIP badge.  
* Other future earned badges.

Staff status must not be inferred only from name color.

The actual badge/profile information establishes the role.

---

# **10\. Staff Colors**

Reserved colors:

| Role | Reserved RGB |
| ----- | ----- |
| Owner | `0, 0, 0` |
| Administrator | `0, 0, 0` |
| Moderator | `255, 0, 0` |

Normal users cannot select these **exact** RGB values.

Very similar colors remain allowed.

Example:

`254, 0, 0`

may be allowed even though:

`255, 0, 0`

is reserved.

Reserved color validation applies to whole-name and per-character colors.

Staff users may deliberately select their reserved colors or use their VIP appearance instead.

Staff status does not force a particular color.

Staff and VIP badges may coexist.

---

# **11\. Purchase Options**

Every non-lifetime tier supports:

## **One month**

One-time payment.

Does not automatically renew.

## **Monthly subscription**

Automatically renews monthly until canceled.

## **Twelve months**

One-time payment.

Does not automatically renew.

Current twelve-month prices:

| Tier | 12-month price |
| ----- | ----- |
| VIP | $19.99 |
| Super VIP | $53.33 |
| Ultra VIP | $106.66 |

Additional durations can be added later without redesigning the entitlement system.

---

# **12\. Entitlements Are a Timeline**

Do not represent VIP only as:

`tier + expiration_date`

The account must support an entitlement timeline.

Each entitlement segment should know at minimum:

* Entitlement ID.  
* User ID.  
* Tier.  
* Purchase source.  
* Start time.  
* End time.  
* Lifetime status.  
* Purchase ID.  
* Stripe reference.  
* Original amount paid.  
* Current state.  
* Whether it is currently active.  
* Whether it is queued.  
* Whether it has been consumed.  
* Whether its unused value has been converted.  
* Creation timestamp.

The website should clearly show the timeline.

Example:

`Super VIP`  
Active now → October 1, 2026 12:34:56 PM

`Ultra VIP`  
Queued  
October 1, 2026 12:34:56 PM → October 1, 2027 12:34:56 PM

`All VIP access ends`  
October 1, 2027 12:34:56 PM

Use exact timestamps where useful.

---

# **13\. Tier Upgrade Queue Behavior**

A temporary higher-tier purchase can temporarily override a lower-tier entitlement without destroying it.

Example:

User has:

`90 days VIP remaining`

User buys:

`30 days Super VIP`

Result:

`30 days Super VIP`

followed by:

`90 days VIP`

Total remaining supporter access:

`120 days`

The 90 VIP days do not disappear while Super VIP is active.

They are paused/queued behind the higher-tier period.

After Super VIP ends, VIP automatically resumes.

---

# **14\. Subscription \+ Prepaid Behavior**

Example:

Monthly Super VIP is currently paid through October 1\.

On September 6 the user buys a 12-month Super VIP package.

Behavior:

* Current monthly period continues normally until October 1\.  
* The subscription is scheduled not to create another paid monthly period.  
* The prepaid twelve-month period begins after the currently-paid period ends.  
* The user receives everything already paid for.  
* No purchased time disappears.

The UI must show this transition before purchase confirmation wherever practical.

---

# **15\. Support Spend Credit**

MiMITA should reward long-term supporters rather than manufacture urgency.

Do not create artificial countdown pressure such as:

“BUY ULTRA IN 24 HOURS OR LOSE 50% OFF.”

Instead maintain a generous persistent supporter-credit system.

Money legitimately spent supporting MiMITA contributes toward lifetime VIP.

Credit applies **across tiers**.

Therefore somebody who repeatedly purchases the cheapest VIP can eventually qualify for lifetime tiers.

Conceptually:

`eligible lifetime price = lifetime list price - eligible supporter credit`

Never below `$0`.

Examples:

If eligible supporter credit is `$100.00`:

Lifetime VIP:  
`$111.11 - $100.00 = $11.11`

Lifetime Super:  
`$222.22 - $100.00 = $122.22`

Lifetime Ultra:  
`$333.33 - $100.00 = $233.33`

If eligible supporter credit reaches `$333.33`, Lifetime Ultra may cost `$0`.

The system should feel like:

**MiMITA remembers that you supported it.**

---

# **16\. Support Credit Ledger**

Do not implement support credit as cryptocurrency in version one.

Do not make it transferable.

Do not make it withdrawable for USD.

Do not make it speculative.

Version one should simply maintain an internal supporter ledger denominated in integer USD cents.

Example:

`support_credit_cents = 12345`

means:

`$123.45`

This preserves the desired behavior without unnecessarily creating a tradable financial asset.

A separate MiMITA currency/token system can be designed later.

All monetary calculations use integer cents, never floating-point dollars.

---

# **17\. Prevent Double Counting**

Every real paid cent must have an auditable ledger history.

Do not accidentally create infinite credit through conversions.

Remaining prepaid time can be converted toward lifetime access.

The system must track which monetary value backs that unused entitlement.

Example:

A user has 300 unused prepaid days.

The UI may offer:

**Convert remaining prepaid VIP value toward Lifetime Ultra VIP**

The conversion value is calculated from the actual net price paid for that entitlement, proportionally to unused time.

Conceptually:

`unused_value = original_net_paid × unused_duration / original_duration`

Once converted:

* Those days are removed from the entitlement timeline.  
* Their unused monetary value moves into lifetime upgrade credit.  
* The same value cannot later be converted again.

Refunded, reversed, fraudulent, or disputed transactions must not create permanent supporter credit.

All adjustment history must be auditable.

---

# **18\. Stripe Is Payment Authority**

MiMITA does not invent successful payments.

Stripe is authoritative for whether money was successfully paid.

Every checkout must be associated with the logged-in immutable MiMITA:

`user_id`

Do not use username as payment identity.

Username can change.

Stripe objects should contain enough internal identifiers to reconcile:

* MiMITA user ID.  
* MiMITA purchase ID.  
* Product/tier ID.  
* Purchase duration/type.  
* Environment.  
* Other required fulfillment metadata.

VIP entitlement is created only after trusted server-side Stripe verification.

The browser redirect after Checkout is **not proof of payment**.

The client saying “I paid” is **not proof of payment**.

---

# **19\. Stripe Failure Safety**

There are distinct failure states.

## **Confirmed payment failure**

If Stripe definitively reports that payment did not succeed:

Show:

`Payment failed.`

If Stripe also confirms that no charge occurred:

Show:

`Payment failed. You have not been charged.`

Provide an extremely obvious:

**HELP / REPORT PAYMENT PROBLEM**

button.

## **Payment succeeded but fulfillment failed**

If Stripe confirms successful payment but MiMITA cannot activate the entitlement:

DO NOT say:

`You were not charged.`

Show something like:

`Payment received, but VIP activation is delayed.`

`Do not pay again.`

`Your payment has been recorded.`

Provide a huge:

**GET HELP / REPORT PAYMENT ISSUE**

button.

Record everything required for reconciliation.

Payment fulfillment must be idempotent.

Receiving the same Stripe event more than once must not grant duplicate VIP.

---

# **20\. Payment Auditability**

Persist enough information to explain every entitlement.

Examples:

* Internal purchase ID.  
* Stripe Checkout Session ID.  
* PaymentIntent ID when applicable.  
* Subscription ID when applicable.  
* Stripe event IDs processed.  
* Amount.  
* Currency.  
* User ID.  
* Product.  
* Timestamp.  
* Environment/test/live status.  
* Fulfillment status.  
* Failure reason.  
* Refund/dispute state.

Do not put Stripe secret keys or webhook secrets in logs.

---

# **21\. VIP Expiration Warnings**

Send warnings:

* Seven days before final expiration.  
* Three days before final expiration.  
* One day before final expiration.  
* At final expiration.

Also notify users when a higher tier is about to end and a queued lower tier will resume.

Example:

`Your Super VIP ends in 3 days. Your existing VIP will automatically resume afterward with 90 days remaining.`

Warning delivery may include:

* Website notification.  
* In-game notification.  
* Email when available.

Do not repeatedly spam the same warning.

Record warning state.

---

# **22\. Expiration Behavior**

When all temporary VIP access expires:

* VIP badges disappear.  
* Name returns to configured normal/default appearance.  
* VIP animations stop.  
* VIP editing controls become locked.  
* Saved VIP configurations remain stored.  
* Presets remain stored.  
* Nothing is deleted merely because VIP expired.

If VIP is purchased again, restore previously-saved valid configurations automatically.

---

# **23\. NameStyle Data**

The exact schema may evolve, but conceptually it needs:

* Schema version.  
* User ID.  
* Style revision.  
* Style content hash.  
* Tier requirement.  
* Badge references.  
* Whole-name style.  
* Per-character overrides.  
* Animation configuration.  
* Animation seed.  
* Preset ID.  
* Future extension data.

Per-character styling may include:

* Character index.  
* RGB/RGBA.  
* Bold.  
* Italic.  
* Strikethrough.  
* Animation/effect references.

Future extensions can add additional properties without breaking older clients.

Unknown unsupported future properties should fail gracefully.

---

# **24\. Style Revision \+ SHA-256 Cache Identity**

Every saved NameStyle receives an increasing:

`style_revision`

Example:

`143`

The serialized canonical NameStyle can additionally have a SHA-256 content hash.

Example conceptual identifier:

`SHA256(canonical_style_payload)`

SHA-256 is 256 bits and is normally represented as 64 hexadecimal characters.

Use the revision for cheap “is mine outdated?” checks.

Use the hash for exact content/cache identity and corruption detection.

A hash is not an authentication mechanism.

Authorization still comes from trusted MiMITA systems.

---

# **25\. Client Cache**

Clients should cache validated NameStyles locally.

Cache key conceptually includes:

`user_id + style_revision + style_hash`

When encountering a player:

If the exact style is already cached:

* Do not download it again.

If the user's revision changed:

* Receive/fetch the new style.  
* Validate it.  
* Replace cached version.

Caches may persist between game sessions.

A player that has already seen the same unchanged user style hundreds of times should not repeatedly download it.

---

# **26\. Live Style Updates**

Style changes should appear rapidly.

Flow:

1. User changes style using an authorized editor.  
2. Trusted MiMITA service validates it.  
3. Database stores it.  
4. `style_revision` increments.  
5. New hash is generated.  
6. Connected game instance learns that the style changed.  
7. Host/server distributes a compact style-change notification.  
8. Other clients invalidate the old cached version.  
9. New authoritative style is obtained.  
10. Renderers change immediately.

Rate-limit style changes to prevent abuse.

Do not transmit continuously-changing RGB values over the network.

Transmit the **definition of the animation**.

---

# **27\. Deterministic Animation**

Animated cosmetics should be deterministic.

Transmit/store data such as:

* Animation type.  
* Seed.  
* Speed.  
* Direction.  
* Parameters.  
* Authoritative/shared time reference.

Every client independently evaluates:

`appearance = f(style, seed, shared_time)`

Do not stream animated colors every frame.

Two clients looking at the same animated name should see approximately the same animation state at the same time.

---

# **28\. Rendered-State Attachment**

Visual cosmetics follow the locally rendered representation of a player.

Example:

If another player's network movement visibly jitters on a client's screen, attached cosmetics should remain attached to the visible player representation rather than moving independently based on some hidden authoritative transform.

Future physical-looking cosmetic motion can use deterministic simulation seeded from the cosmetic/style data.

Cosmetics remain presentation only.

---

# **29\. Per-Letter Animation Bounds**

Ultra VIP may support physically animated letters.

Animation may be expressive, but it must have bounds.

Initial rule:

A character's animation should not move it more than approximately 10% beyond its normal positional/layout bounds in each direction unless a later validated effect explicitly permits something different.

The exact limit must be config-driven and hot reloadable.

Effects cannot be allowed to cover arbitrary unrelated UI or make gameplay information unusable.

---

# **30\. Performance Priority**

Gameplay always wins.

Priority order conceptually:

1. Input.  
2. Gameplay simulation.  
3. Movement.  
4. Networking required for gameplay.  
5. Collision/combat.  
6. Critical game rendering.  
7. User interface required to play.  
8. Cosmetic presentation.  
9. Animated name effects and decorative cosmetics.

VIP effects may never delay higher-priority gameplay work.

---

# **31\. Automatic Cosmetic Degradation**

The renderer should automatically lower cosmetic quality under performance pressure.

Possible degradation sequence:

`full animated effect`

→

`reduced animation update frequency`

→

`simplified animation`

→

`static styled name`

→

`plain configured fallback name`

This must happen without affecting gameplay state.

The exact thresholds are configuration-driven.

Do not repeatedly oscillate quality every frame.

Use hysteresis/cooldowns so quality levels are stable.

---

# **32\. User Cosmetic Rendering Preference**

Clients should expose local presentation settings such as:

* Full effects.  
* Reduced effects.  
* Static VIP styles.  
* Plain names.

A user choosing reduced effects changes only their local rendering.

It does not remove the VIP owner's ownership or saved style.

Accessibility and performance take priority over forcing animations on another person's device.

---

# **33\. Name Renderer Performance Requirements**

Animated VIP names should not:

* Allocate memory every frame.  
* Reparse JSON every frame.  
* Rebuild unchanged text every frame.  
* Recalculate static glyph geometry unnecessarily.  
* Query the database every frame.  
* Send animation packets every frame.  
* Perform network requests during ordinary rendering.  
* Block gameplay threads waiting for cosmetic data.

Prefer:

* Cached glyph layouts.  
* Cached style objects.  
* Batched rendering.  
* GPU-side animation where appropriate.  
* Deterministic shader/effect inputs.  
* Fixed-size/reused buffers where appropriate.  
* Low-frequency cosmetic updates.  
* Distance/visibility-based work avoidance.  
* No work for offscreen names when unnecessary.

Engineering objective:

**Make rich VIP rendering approach or beat the CPU cost of the previous ordinary username renderer for comparable visible-name counts.**

Correctness comes first, then measurement and optimization.

---

# **34\. Database/API Failure Behavior**

The database is authoritative, but clients should not directly expose database credentials.

Use the appropriate MiMITA trusted API/service boundary.

If authoritative name data cannot currently be reached:

* Use the last successfully validated cached version.  
* Do not fabricate a new VIP entitlement.  
* Do not silently pretend everything is healthy.  
* Continue gameplay whenever safe.  
* Fail loudly in presentation/diagnostics rather than breaking the match.

Show an understandable warning such as:

`MiMITA account service temporarily unavailable.`

`Using last known profile appearance.`

Include:

**REPORT / GET HELP**

The diagnostic should include a safe error code and timestamp suitable for screenshots/reports.

Never expose secrets.

---

# **35\. Missing-Cache Failure**

If:

* Account service is unavailable.  
* User has never been seen before.  
* No valid cached style exists.

Use the authoritative configured fallback appearance bundled/cached from the last valid configuration.

Do not invent VIP status.

---

# **36\. Server Authority**

Clients cannot grant themselves VIP.

Clients cannot claim:

* A higher tier.  
* Lifetime status.  
* A badge they do not own.  
* A reserved color they cannot use.  
* An unsupported effect.  
* Invalid animation parameters.

The trusted server/API validates saved NameStyle data against authoritative entitlement.

Peer-hosted game servers may distribute validated style data but must not turn an unverified client claim into authoritative VIP ownership.

---

# **37\. Current Version-One Scope**

Version one must support end-to-end:

* VIP turquoise names.  
* Super VIP solid colors.  
* Super VIP rainbow.  
* Ultra VIP per-character colors.  
* Ultra VIP per-character bold.  
* Ultra VIP per-character italics.  
* Ultra VIP per-character strikethrough.  
* Ultra editable rainbow speed.  
* Ultra editable rainbow direction.  
* Saved presets.  
* Normal badge for each tier.  
* Lifetime badge variant for each tier.  
* One-month purchases.  
* Monthly subscriptions.  
* Twelve-month purchases.  
* Lifetime purchases.  
* Support-spend credit.  
* Entitlement queue.  
* Higher-tier temporary override followed by queued lower tier.  
* Stripe webhook fulfillment.  
* Expiration warnings.  
* Settings preserved after expiration.  
* NameStyle caching.  
* Style revisions/hashes.  
* Live style updates.  
* Deterministic animations.  
* Performance degradation.  
* Local animation-reduction settings.  
* Website renderer.  
* `.exe` renderer.  
* Colored/styled names everywhere currently implemented.

Future cosmetics and additional effects can follow after this foundation works.

---

# **38\. Definition of Done**

VIP is NOT finished merely because checkout succeeds.

The implementation must demonstrate this complete flow:

1. Logged-in user selects VIP.  
2. Purchase is tied to immutable MiMITA user ID.  
3. Stripe successfully processes the payment.  
4. Signed/trusted Stripe event reaches MiMITA.  
5. Event is processed exactly once.  
6. Correct entitlement is created.  
7. Account immediately shows correct VIP.  
8. User opens name editor.  
9. User creates a detailed style.  
10. Server validates style.  
11. Database persists it.  
12. Website profile renders it.  
13. Website navigation/user references render it.  
14. Game launches.  
15. Main menu renders it.  
16. Player joins game.  
17. Overhead name renders it.  
18. Another client sees the same appearance.  
19. Chat renders it.  
20. Scoreboard renders it.  
21. Killfeed renders it.  
22. Spectator UI renders it.  
23. Match results render it.  
24. Per-letter differences remain intact.  
25. Animation is deterministic across clients.  
26. Restarting the website/game preserves it.  
27. Rejoining does not unnecessarily download unchanged style data.  
28. Website style edit propagates to connected clients.  
29. Low-performance mode degrades the effect without hurting gameplay.  
30. Simulated service outage uses valid cached data and visibly reports the failure.  
31. Simulated expiration removes active VIP appearance.  
32. Saved style remains stored.  
33. Re-purchasing VIP restores previous style.  
34. Queued entitlement transitions occur at the exact expected time.  
35. Lifetime entitlement never expires.  
36. Stripe duplicate webhook delivery does not duplicate entitlement.  
37. Failed payments do not grant VIP.  
38. Successfully-paid-but-failed-fulfillment cases tell the player not to pay again.

Until these behaviors pass, VIP is incomplete.

---

# **39\. Regression Rule**

Every VIP bug fixed should gain a regression test wherever technically possible.

Especially protect:

* Stripe fulfillment.  
* Duplicate Stripe events.  
* User ID attribution.  
* Entitlement queue math.  
* Lifetime calculations.  
* Support credit calculations.  
* Refund/dispute adjustments.  
* Reserved colors.  
* Per-character validation.  
* Cache invalidation.  
* Style revision behavior.  
* Deterministic animation.  
* Expiration.  
* Rendering consistency.  
* Performance degradation.

The desired behavior in this document is authoritative unless intentionally superseded by a newer explicit specification.

