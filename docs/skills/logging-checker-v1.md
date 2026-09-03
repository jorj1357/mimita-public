// 09 03 2026, 15 41
/* purpose
* check that changed behavior can explain its inputs, decisions, and outputs
* ensure diagnostics belong to the correct subsystem owner
* prevent repeated log spam and unsupported claims
* this skill DOES NOT add logging without identifying its owner
* this skill DOES NOT expose secrets or private values
* this skill DOES NOT treat logs as proof of behavior they cannot observe
*/

# Logging Checker v1

Trace the changed path and check whether important state transitions, rejection
reasons, and outputs are observable. Confirm category, throttling, context, and
the owning log file. Flag printf-style or per-frame spam, logs that omit the
decision reason, and logs that claim success before the result exists. Report
exact paths and lines plus the missing evidence or smallest diagnostic fix.
