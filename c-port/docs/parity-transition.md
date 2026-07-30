# Transition-period differential parity

The extraction set deliberately excludes the C++ reference engine, its golden outputs, reference
runner, generated differential levels, and differential scripts. Those remain in the originating
`bomb-box-state` checkout so extraction cannot hide a disagreement or duplicate the reference
implementation.

Before the parent checkout switches any production consumer, configure it to obtain the extracted
repository as an external CMake dependency while retaining its existing reference runner,
candidate transcript runner, contracts, comparator, and generated-corpus scripts. Both runners
must remain separate processes because they export identical C symbols.

From the parent checkout, rerun:

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug -L differential --output-on-failure
ctest --preset native-debug \
  -R 'game_rules.differential.explosions_parity|game_rules.differential.seeded_history_lifecycle' \
  --repeat until-fail:100 --output-on-failure
python3 tests/c-port/history_differential_test.py \
  out/build/native-debug/tests/game_rules_reference_runner \
  out/build/native-debug/tests/game_rules_candidate_runner \
  tools/c-port/compare_transcript.py
```

The comparator already accepts runner paths as arguments; it does not require either engine to be
in its own source tree. During dependency migration, only the candidate runner's link source
changes to the external `GameRules::StateC` target. Do not copy C++ implementation code or
reviewed golden data into this standalone package.
