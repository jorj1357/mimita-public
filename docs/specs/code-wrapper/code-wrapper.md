2026 10 21 1554 jorj todo explain

Formal target architecture, language bake-off, and the deferred 50-file
one-second latency contract are in
[../hotreload/hot-layer-latency-and-language.md](../hotreload/hot-layer-latency-and-language.md).
This note remains informal product input, not the normative specification.

2026 10 21 1622 suggestion 
1561796482116362291 
in mimita discord server
frmo Aksolodrom   1088843415019593799 - "What I meant by that is you can add mods to the server you create so every player on the server will have those mods, like in yomih"
good
we do this
mod loader 

likeroblox has lua

make c++ funcitons
omg  aaaaa
scary and powerufl

so 
make lua
call the safe version fo those funcitons

ok cool

problem:  its on roblox
i cant change c++ code of roblox
cant fix the stuio
cant add antthing iw ant
cant add entirelt new c++ code that would be better and allow new things that are sick and cool

solution:
the c++ is visible
the things making up teh c++ is visible (e.g. specs)
the things amking that stuff up is visible e.g. mindste  vision  inspriaitions etc 
all its visible
and open source
and modifiable
editable
reverse engineerable
all of it 

so idk whern but 
lua interpreter
matbe
alos phton
rust? 
c#
all languages
whatever language u know  we support that 
even html if u awnt
idk
need a reallt good ai translator code translator
but it alread does so whtever

## Long-term ideal: universal live creation and modification

This is an aspirational product ideal, not a description of current behavior
or a promise that the present architecture can meet it. Keep it visible when
choosing languages, runtimes, operating systems, and module boundaries. A
script wrapper may be one useful authoring path, but it must not become a
ceiling that prevents creators or developers from adding deeper engine
capabilities.

The ideal experience is that people can create, edit, and extend the game from
whatever device they have available, including PC, Mac, Linux, mobile, console,
and future devices. Authoring tools should work on constrained devices too;
creation should not require access to a desktop-only studio. The chosen
implementation may use native code, portable bytecode, JIT compilation,
interpreters, or multiple languages, and may use different operating systems.
Those are implementation choices. The goal is a coherent, reliable creation
experience across them, not a requirement to invent a new operating system or
support every language literally.

Players on supported platforms should be able to join the same live server.
While they remain connected, creators and developers should ideally be able to
change and activate game behavior, including deep systems such as collision,
physics, and networking. This should include ambitious changes that introduce
new capabilities or models, not only edits to exposed tuning values or a fixed
set of wrapper functions. A radical idea such as a new form of “4D networking”
is an example of the intended creative freedom; the term is illustrative and
does not define a specific networking design.

The aspirational latency is about one second from a valid saved edit to the
new behavior being active, even for substantial changes. This is a north-star
goal, not a measured guarantee or a reason to hide slower work. Report actual
save-to-active latency and supported change scope honestly as the system
develops.

Live updates must aim to preserve connected play and consistent behavior
across server and clients. They need a reliable way to validate an update,
activate it at a safe point, keep state or migrate it, maintain protocol and
simulation compatibility across platforms, and recover from a failed update.
Some changes may still require client updates, migration, or a restart; the
ideal is to reduce those cases without silently applying incompatible code.
