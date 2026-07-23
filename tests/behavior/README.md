# Behavior tests

Put specification-level scenarios here as each implementation phase begins. Prefer tests that
construct a complete pre-tick state, submit one command (or initialize a level), and compare the
entire ordered tick/event/state result.

Keep these tests platform-independent. The same scenario corpus should eventually run against the
native library, the WebAssembly build, and the Unreal adapter so determinism regressions are visible
at integration boundaries.

