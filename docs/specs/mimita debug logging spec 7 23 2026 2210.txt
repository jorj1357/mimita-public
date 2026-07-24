MiMITA debug logging and profiling specification
Date: July 18, 2026
Status: Target architecture
Maximum principle: Everything important must be observable, searchable, measurable, and reproducible.
________________


1. End goal
When mimita.exe runs, everything needed to understand the game’s behavior is:
printed live in the terminal if enabled
        +
written into readable text files
Logs must explain:
what happened
when it happened
where it happened
why it happened
what caused it
what was expected
what actually happened
how large the difference was
A human should be able to open the files and understand them.
An AI agent should be able to read the files and follow one event through the entire engine without guessing.
The logging system exists to replace statements such as:
I think it broke.
It feels laggy.
The grenade looks wrong.
The animation sometimes stops.
With evidence such as:
Grenade position error:
expected = (18.42, 3.10, 7.83)
actual = (18.42, 1.77, 7.83)
difference = 1.33 units vertically


First mismatch:
server tick = 18,401
client tick = 18,406
file = src/physics/loose-object-physics.cpp
line = 284
The central equation is:
bug = actual behavior - expected behavior

________________
2. Log folder and file output
All logs are written under:
C:\important\mimita-priv-v8\logs\mm-dd-yyyy\
Example:
C:\important\mimita-priv-v8\logs\07-18-2026\
The format is:
month-day-year
Not:
day-month-year
If the daily folder does not exist, create it.
If it already exists, reuse it.
Every execution creates new timestamped files.
Files are like
(section)_mmddyyyy_hhmmss.txt
And its .txt because other extensions are hard to understand for other players 
Example:
Summary_07182026_093015.txt
Network_07182026_093015.txt
Weapons_07182026_093015.txt
Physics_07182026_093015.txt
Performance_07182026_093015.txt
Errors_07182026_093015.txt
A second execution creates:
Summary_07182026_101422.txt
Network_07182026_101422.txt
Weapons_07182026_101422.txt
Physics_07182026_101422.txt
Performance_07182026_101422.txt
Errors_07182026_101422.txt
Never overwrite an earlier run.
Summary contains the relevant contents of all enabled category logs in chronological order.
Logs must also print live to the game terminal.
The terminal and text files use the same formatted log records.
________________


3. One central logger
There is one debug logging architecture. JUST ONE
NO PRINTF NO NOTHING ELSE JUST THIS 
Gameplay files do not each invent their own logging system.
All systems call one public logging entry point:
debug::log(...);
Conceptual interface:
namespace debug
{
    void log(const LogRecord& record);


    void logValue(const ValueLogRecord& record);


    void logEvent(const EventLogRecord& record);


    void logDifference(const DifferenceLogRecord& record);


    void logAssertion(const AssertionLogRecord& record);


    void logPerformance(const PerformanceLogRecord& record);
}
The central logger handles:
level filtering
category filtering
formatting
timestamps
tick and frame metadata
source file and line
event IDs
correlation IDs
queueing
thread safety
throttling
sampling
terminal output
file routing
summary output
flushing
retention
Gameplay systems only create structured records.
They do not decide how files are opened, named, rotated, or retained.
Do not use raw printf, std::cout, or unrelated file streams for debug output.
________________


4. Debug configuration
A single JSON file controls logging:
C:\important\mimita-priv-v8\config\debuglogger.json
This file is the authority for debug logging configuration.
It is hot-reloadable.
Changing it while the game runs must update logging without restarting the game.
Initial conceptual configuration:
{
  "enabled": true,
  "level": "important",


  "terminal_output": true,
  "summary_file": true,


  "categories": {
    "startup": true,
    "errors": true,
    "assertions": true,
    "performance": true,
    "network": false,
    "weapons": false,
    "projectiles": false,
    "damage": false,
    "movement": false,
    "collisions": false,
    "npcs": false,
    "animation": false,
    "effects": false,
    "gui": false,
    "avatar": false,
    "models": false,
    "replay": false,
    "audio": false
  },


  "sampling": {
    "default_every_n_ticks": 60,
    "log_on_abnormal_change": true
  },


  "retention": {
    "daily_logs_days": 14,
    "performance_logs_days": 30,
    "trace_runs": 3,
    "crash_logs_forever": true
  }
}
For now, logging is controlled through this JSON.
Do not make in-game console commands another competing authority.
An in-game GUI may be added later after the external JSON system is reliable.
________________


5. Log levels
Supported levels:
enum class LogLevel
{
    Off,
    Error,
    Important,
    Verbose,
    Trace
};
Off
No logging.
Error
Logs failures that prevent or seriously damage execution:
startup failure
missing required DLL
invalid required configuration
fatal network protocol mismatch
uncaught exception
assertion failure
NaN entering authoritative state
Important
Includes errors plus meaningful warnings and transitions:
player connected
player disconnected
attack rejected
file missing and fallback used
frame-time spike
server tick overrun
projectile correction above threshold
player died
map loaded
Verbose
Includes important plus detailed state needed to investigate systems:
exact function
exact duration
exact entity
exact tick
exact input
exact before state
exact after state
exact validation result
Trace
Records extremely detailed execution.
Trace may include individual steps through a system, but must still use sampling and throttling where continuous data would become unusable.
Trace is not permission to freeze the game by writing millions of lines per second.
________________


6. Log record format
Every log record should contain enough information to locate and understand the event.
Conceptual structure:
struct LogRecord
{
    LogLevel level;
    LogCategory category;


    uint64_t eventId;
    uint64_t correlationId;
    uint64_t parentEventId;


    uint64_t frameNumber;
    uint64_t simulationTick;
    uint64_t serverTick;
    uint64_t clientTick;


    EntityId entityId;
    PlayerId playerId;


    const char* sourceFile;
    int sourceLine;
    const char* functionName;


    std::string eventName;
    std::string reason;
    std::string message;
};
Formatted example:
[2026-07-18 09:30:15.284]
[IMPORTANT]
[NETWORK]
[EVENT=NETWORK_00381]
[CORRELATION=ATTACK_00481]
[PLAYER=2]
[CLIENT_TICK=1000]
[SERVER_TICK=1002]
[src/network/server-weapon-validation.cpp:184]
[validateAttackRequest]


Attack request accepted.


reason = player alive, weapon equipped, ammo available,
         cooldown complete, direction valid
Every record should answer:
What is this?
Why was it logged?
Which event does it belong to?
Where in the code did it happen?
Bad:
player pos = 1 2 3
Better:
[MOVEMENT]
Player position before wall collision resolution.


reason = investigating unexpected wall penetration
player = 2
position = (1.0, 2.0, 3.0)
velocity = (8.2, 0.0, -1.3)

________________
7. Event and correlation IDs
Every important operation receives a searchable ID.
Category event examples:
COLLISION_00042
GUI_00013
NETWORK_00381
AVATAR_00009
PERFORMANCE_00127
Cross-system operations receive correlation IDs:
ATTACK_00481
PROJECTILE_09821
EXPLOSION_15502
RESPAWN_00088
CONNECTION_00014
Example attack chain:
[ATTACK_00481] input detected
[ATTACK_00481] request created
[ATTACK_00481] request transmitted
[ATTACK_00481] server received request
[ATTACK_00481] ammo validated
[ATTACK_00481] cooldown validated
[ATTACK_00481] projectile created
[ATTACK_00481] confirmation transmitted
[ATTACK_00481] prediction reconciled
The projectile may continue under its own ID while preserving the parent:
[PROJECTILE_09821]
parent = ATTACK_00481
The explosion may then continue:
[EXPLOSION_15502]
parent = PROJECTILE_09821
root = ATTACK_00481
This makes the entire causal chain searchable.
________________


8. Expected, actual, and difference
Important diagnostic records must compare expected and actual behavior.
Conceptual record:
struct DifferenceLogRecord
{
    std::string measurement;


    Value expected;
    Value actual;
    Value difference;


    Value allowedMinimum;
    Value allowedMaximum;


    DifferenceStatus status;
};
Example:
[COLLISION DIFFERENCE]
event = COLLISION_00042
measurement = penetration depth


expected = 0.000 units
actual = 0.420 units
difference = +0.420 units


allowed maximum = 0.010 units
status = FAILED
over limit = 42.0x
Another example:
[PROJECTILE RECONCILIATION]
projectile = 9821


expected server position = (10.20, 4.40, 8.10)
actual client position = (10.18, 4.12, 8.09)


position error = 0.281 units
small correction threshold = 0.100 units
major correction threshold = 2.000 units


classification = MEDIUM_CORRECTION
Do not log only vague descriptions.
Use:
numbers
units
thresholds
ratios
exact states
true/false

________________
9. Assertions and invariants
Assertions define states that must never become invalid.
Examples:
player position must be finite
player velocity must be finite
health must remain within valid bounds
projectile radius must be greater than zero
camera quaternion magnitude must remain near 1
ammo must not become negative
server tick must never decrease
an event ID must not be processed twice
Conceptual assertion:
debug::assertInvariant(
    std::isfinite(player.velocity.x),
    "PLAYER_VELOCITY_FINITE",
    context);
Failure output:
[ASSERTION FAILED]
assertion = PLAYER_VELOCITY_FINITE


file =
C:\important\mimita-priv-v8\src\physics\movement\physics-collision.cpp


line = 582
function = resolvePlayerCollision


player = 2
tick = 18,401


current velocity = (NaN, 0.0, 4000.0)
previous velocity = (12.4, 0.0, -3.2)
collision normal = (NaN, NaN, NaN)
contact point = (42.1, 3.8, 17.2)


parent event = COLLISION_00042
An assertion record must include the surrounding state needed to investigate it.
Do not only print:
assert failed

________________
10. Performance profiling
The debug system includes one shared performance profiler.
Systems use scoped timers:
debug::ScopedTimer timer(
    "NPC_RESPAWN",
    LogCategory::Performance,
    PERFORMANCE_BUDGET_NPC_RESPAWN_MS);
The timer records:
start time
end time
duration
frame
tick
thread
entity
event
budget
over-budget ratio
Example:
[PERFORMANCE]
operation = NPC_RESPAWN
budget = 0.500 ms
actual = 4.200 ms
status = OVER_BUDGET
ratio = 8.40x
Frame-time accounting should provide a breakdown:
Frame 48,120


total frame time = 20.02 ms


simulation = 19.86 ms
    NPC respawn = 19.20 ms
    collisions = 0.31 ms
    weapons = 0.18 ms
    network = 0.09 ms
    animation = 0.08 ms


rendering = 0.16 ms


unaccounted time = 0.00 ms
The measured categories should approximately add to the total.
If they do not, report:
unaccounted frame time
Do not hide missing time.
The long-term goal is to reduce every duration toward zero, while budgets provide measurable current targets.
________________


11. Sampling, throttling, and queues
Do not write unchanged continuous state every tick unless explicitly required.
Instead of:
print player position every frame forever
Use policies such as:
log every 60 ticks
log when value changes beyond threshold
log when an abnormal state occurs
log the first occurrence
log once per time interval
log a summary of repeated events
Example configuration:
{
  "movement_position": {
    "every_n_ticks": 60,
    "minimum_change": 0.5,
    "always_log_abnormal": true
  }
}
Repeated event summary:
[THROTTLED EVENT SUMMARY]
event = PROJECTILE_SMALL_CORRECTION


suppressed repetitions = 842
period = 10.0 seconds


maximum error = 0.042 units
average error = 0.011 units
Logging must use a queue so gameplay systems do not constantly block on disk writes.
Conceptual flow:
gameplay thread creates record
        ↓
record enters bounded log queue
        ↓
logging worker formats record
        ↓
terminal and file outputs receive record
If the queue becomes full:
* Preserve errors and assertions.
* Preserve major performance events.
* Drop or summarize low-priority trace records.
* Report how many records were dropped.
* Never silently lose critical failures.
________________


12. Reproducibility header
Every log file begins with a run header.
The header includes:
run ID
start date and time
git commit
git branch
dirty working tree true/false
changed files summary
build configuration
compiler version
build timestamp
protocol version
operating system
CPU
GPU
RAM
command-line arguments
working directory
loaded mods
loaded maps
loaded config files
config hashes
weapon-definition hash
network mode
server/client role
Example:
============================================================
MiMITA DEBUG RUN
============================================================


run_id = RUN_20260718_093015_0001


git_commit = a42cd189
git_branch = network-rewrite
working_tree_dirty = true


build = Debug
compiler = MSVC 19.42
build_timestamp = 2026-07-18 09:10:42


os = Windows 11
cpu = Intel i5-13420H
gpu = NVIDIA RTX 4050
ram = 16 GB


process_role = client
network_protocol = 18
server_address_type = local


loaded_config:
C:\important\mimita-priv-v8\config\gameplay.json
C:\important\mimita-priv-v8\config\debuglogger.json
C:\important\mimita-priv-v8\config\weapons.json
This eliminates ambiguity about which code and configuration created the behavior.
________________


13. Causality and golden paths
Logs should explain causes, not only symptoms.
Example:
NPC stopped updating for 3.2 seconds.
The useful chain is:
NPC death
    ↓
ragdoll creation
    ↓
particle burst
    ↓
4,300 allocations
    ↓
allocator stall
    ↓
simulation tick overrun
    ↓
NPC update delayed
Golden paths define the expected stages of important operations.
Example weapon path:
attack input
    ↓
attack request created
    ↓
client prediction started
    ↓
server request received
    ↓
ammo checked
    ↓
cooldown checked
    ↓
attack accepted
    ↓
hitscan/projectile/melee executed
    ↓
damage or impact resolved
    ↓
effects emitted
    ↓
confirmation received
    ↓
prediction reconciled
Every stage logs the same correlation ID.
At completion, the logger may compare expected and observed stages:
[GOLDEN PATH RESULT]
path = PROJECTILE_ATTACK
correlation = ATTACK_00481


expected stages = 12
completed stages = 8


missing:
server_projectile_spawned
projectile_confirmation_sent
client_prediction_adopted
reconciliation_completed


first missing stage =
server_projectile_spawned
This identifies where execution stopped.
________________


14. System-specific logging
Every major system defines meaningful diagnostics.
Networking
Log:
packet type
packet size
sequence
acknowledgements
request ID
event ID
client tick
server tick
ping
jitter
packet loss
accept/reject result
rejection reason
prediction error
Weapons
Log:
weapon definition
weapon runtime before
attack request
ammo before/after
cooldown before/after
generated directions
spread seed
execution path
hit/projectile/melee result
Projectiles and loose objects
Log:
position
velocity
angular velocity
gravity
collision shape
contact point
contact normal
impact velocity
bounce result
fuse
client/server position error
Movement and collisions
Log:
input
position before
velocity before
collision query
surface normal
penetration depth
resolution impulse
position after
velocity after
expected movement
actual movement
NPCs
Log:
NPC ID
definition
position
rotation
state
decision
target
spawn reason
death reason
respawn duration
AI time
physics time
render time
GUI
Log:
element ID
position
size
scale
rotation
text
font
color
alpha
visible
hovered
clicked
layout parent
Avatar and models
Log:
exact file paths
file existence
asset hashes
loaded dimensions
metadata
face/body assignment
position
rotation
scale
color
alpha
model parse result
Animation and effects
Log:
event ID
entity generation
animation state
requested pose
actual pose
effect definition
spawn position
lifetime
destroy reason
Only enabled categories write detailed records.
________________


15. Logging around suspected code
When a specific line or operation is suspected, log immediately before and after it.
Example:
debug::logValue({
    .eventName = "VELOCITY_BEFORE_RESOLUTION",
    .value = player.velocity
});


resolveCollision(player, collision);


debug::logValue({
    .eventName = "VELOCITY_AFTER_RESOLUTION",
    .value = player.velocity
});
The records use the same correlation ID.
This creates:
state before operation
operation begins
operation result
state after operation
difference
Do not scatter custom file-writing code around the target function.
Only call the central logger.
Temporary investigation logs should include an explicit reason:
reason = investigating grenade vertical velocity becoming zero
When the investigation ends, either:
* Remove the temporary record.
* Convert it into a permanent useful diagnostic.
* Disable it through sampling or level configuration.
________________


16. Retention and crash safety
Default retention targets:
daily general logs:
keep 14 days


performance logs:
keep 30 days


trace logs:
keep latest 3 runs


crash logs:
keep indefinitely


assertion failure logs:
keep indefinitely
Retention runs safely at startup or shutdown.
It must not delete files from the current run.
Crash-sensitive records should flush promptly:
fatal error
assertion failure
server shutdown
protocol corruption
NaN authoritative state
The logger should maintain a small recent in-memory ring buffer.
If the game crashes, write recent records into a crash file:
Crash_07182026_093442.txt
This file includes the events immediately before the crash.
________________


17. Regression investigation
When a bug may have worked previously, the investigation process is:
1. Identify whether it ever worked.
2. Find the most recent known working commit.
3. Record the current commit.
4. Compare changed files.
5. Identify changed systems.
6. Reproduce the bug on the current version.
7. Reproduce the same test on the old version.
8. Compare logs.
9. Find the first event where behavior diverges.
10. Modify the smallest responsible layer.
The logging system records Git metadata, but it does not automatically rewrite or revert source code.
An AI agent investigating a regression should inspect:
current logs
known-working logs
git diff
changed configuration
changed asset hashes
changed packet protocol
The important comparison is:
same action
same initial state
old expected event path
new broken event path
first divergence

________________
18. Numeric success conditions
Every fix must define what counts as success.
Bad:
Camera export is fixed.
Good:
camera position error <= 0.05 units
camera rotation error <= 0.25 degrees
camera FOV error <= 0.10 degrees
Bad:
Grenade looks smooth.
Good:
server simulation = 60 Hz
client simulation = 60 Hz
average projectile correction <= 0.05 units
95th percentile correction <= 0.15 units
major corrections = 0 during test
duplicate visible projectile count = 0
Bad:
Respawn no longer lags.
Good:
respawn total CPU time <= 1.00 ms
maximum frame time during respawn <= 16.67 ms
unaccounted frame time <= 0.20 ms
unexpected allocations during respawn = 0
Expected values must come from the actual design requirement.
Do not define expected behavior by copying the broken actual value.
After a fix, read the logs and verify:
the test actually exercised the feature
the expected value is correct
the actual value meets it
the behavior repeats consistently
no new warnings appeared

________________
19. File responsibilities
debug/debug-log.h
Does:
define public logging API
define log levels
define categories
define structured record types
define helper macros for source location
Does not:
contain gameplay logic
know weapon behavior
perform collision resolution
debug/debug-log.cpp
Does:
receive records
filter records
assign event IDs
format output
route records
write terminal output
write category files
write summary file
Does not:
decide what gameplay should happen
calculate expected gameplay values
debug/debug-config.cpp
Does:
load debuglogger.json
validate configuration
hot reload configuration
provide current category and level settings
Does not:
write logs directly
contain GUI settings unrelated to logging
debug/debug-writer.cpp
Does:
manage output folders
manage files
write queued records
flush critical records
apply retention
Does not:
inspect gameplay entities
debug/debug-profiler.cpp
Does:
measure durations
track nested scopes
calculate budgets
calculate frame breakdowns
report unaccounted time
Does not:
render frames
update NPCs
debug/debug-events.cpp
Does:
generate event IDs
generate correlation IDs
track parent/root relationships
track golden-path stages
Does not:
execute weapon attacks
send network packets
Gameplay files only call these shared systems.
________________


20. Final rules
The debug architecture follows these permanent rules:
One logger.


One hot-reloadable JSON authority.


No raw printf-based debug architecture.


Every important record has a reason.


Every important operation has an ID.


Every cross-system operation has a correlation ID.


Every record includes exact source location.


Important bugs compare expected, actual, and difference.


Important values use numbers and units.


Performance uses budgets and measured ratios.


Continuous data uses sampling and throttling.


Critical records are never silently discarded.


Logs contain reproducibility metadata.


Logs explain causes, not only symptoms.


Golden paths reveal the first missing stage.


Assertions capture surrounding state.


Every fix has numeric success conditions.


Every run remains readable by a human and an AI.


The logger observes gameplay.


The logger does not own gameplay.
The final target is:
Something breaks
        ↓
open the newest Summary file
        ↓
search the event or correlation ID
        ↓
follow the complete event chain
        ↓
find the first expected/actual difference
        ↓
open the exact file and line
        ↓
fix the responsible system
        ↓
run again
        ↓
verify numeric success conditions
