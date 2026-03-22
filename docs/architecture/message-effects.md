# Message Effects

This document describes how Komai detects, routes, renders, and evolves
timeline message effects.

## Goals

- Keep effects easy to recognize from message content.
- Prefer emoji-triggered behavior over hidden command or protocol trivia.
- Support a small curated set of effects, not an unbounded gimmick surface.
- Preserve compatibility for legacy effect message types where practical.
- Keep rendering code maintainable as effects grow.

## Current Model

Komai supports timeline effects as a separate presentation layer on top of
ordinary Matrix timeline events.

Effects are detected from either:

- message body content, usually by emoji presence
- legacy explicit effect message types still accepted for compatibility

Detected effects are forwarded from the timeline model into QML as a list of
effect names, so one message can trigger multiple effects.

Example:

- `⛈` triggers both `rainfall` and `lightning`

## Trigger Sources

Detection currently lives in:

- `src/timeline/TimelineSpecialEffects.h`
- `src/timeline/TimelineSpecialEffects.cpp`

Supported content-triggered effect families:

- `confetti`
  - `🎉`
  - `🎊`
- `sunlight`
  - `☀`
  - `🌞`
- `rainfall`
  - `🌧`
  - `🌦`
  - `☔`
- `lightning`
  - `⚡`
- `storm` composite trigger
  - `⛈`
  - expands to `rainfall + lightning`
- `komaiLogo`
  - `🦁`
  - `⛩️`

Legacy compatibility triggers still supported:

- `nic.custom.confetti` -> `confetti`
- `io.element.effect.rainfall` -> `rainfall`

These explicit message types are compatibility affordances, not the preferred
product model.

## Detection Pipeline

Detection runs after an event is materialized as a timeline event variant:

- `m.text` bodies are inspected
- unknown message bodies are also inspected
- legacy effect message types are recognized explicitly

The detector returns `QVector<SpecialEffect>`, preserving insertion order while
deduplicating repeated matches.

This order matters because the downstream signal passes a `QStringList` of
effect names to QML, and some behavior may depend on combined effects arriving
in a predictable order.

## Rendering Pipeline

High-level flow:

1. `TimelineSpecialEffects::detect(...)` returns zero or more effects.
2. `TimelineModel` emits the effect-name list.
3. `TimelineRoomEventConnections.qml` forwards that list to
   `TimelineEffects.qml`.
4. `TimelineEffects.qml` routes each effect to the relevant renderer and
   computes the total lifetime for the stop timer.

Relevant QML files:

- `resources/qml/ui/TimelineEffects.qml`
- `resources/qml/ui/TimelineParticleLayer.qml`
- `resources/qml/ui/TimelineLightningEffect.qml`
- `resources/qml/ui/TimelineSunlightEffect.qml`
- `resources/qml/ui/TimelineKomaiEffect.qml`
- `resources/qml/timeline/TimelineView.qml`
- `resources/qml/timeline/components/TimelineRoomEventConnections.qml`

## Rendering Split

Komai now splits effect rendering by style:

- particle effects:
  - `confetti`
  - `rainfall`
- overlay effects:
  - `lightning`
  - `sunlight`
  - `komaiLogo`

`TimelineEffects.qml` acts as the coordinator:

- maps effect names to renderers
- computes per-effect and combined durations
- triggers renderers
- resets overlay effects
- recreates the particle layer after each effect cycle

`TimelineParticleLayer.qml` is intentionally limited to particle-based effects.
This avoids mixing particle state management with overlay animation logic.

Each overlay effect component exposes a tiny common interface:

- `property int durationMs`
- `function trigger(...)`
- `function reset()`

This keeps the coordinator small and makes adding new overlay effects
straightforward.

## Duration and Shutdown

Effect lifetime is not just particle lifespan.

For particle effects, total duration is:

- pulse duration
- plus particle lifespan

For overlay effects, total duration is declared directly by the effect
component via `durationMs`.

The timeline stop timer uses the maximum duration of all triggered effects for
the message, then:

- disables effect visibility on the timeline view
- resets effects
- recreates the particle layer for the next run

This recreation step exists because reusing Qt particle state caused rain to
stick visually after some storm runs.

## Product Rules

Current intended product direction:

- visible, ordinary effects should primarily be emoji-triggered
- combined effects are allowed when the emoji itself clearly implies a
  combination
- multiple emoji may map to the same effect family
- support should be curated and explainable

This is why:

- `☀️` and `🌞` both map to `sunlight`
- `⛈` maps to `rainfall + lightning`

## Adding a New Effect

When adding an effect:

1. Decide whether it is particle-based or overlay-based.
2. Add a `SpecialEffect` enum value.
3. Add trigger definitions in `TimelineSpecialEffects.cpp`.
4. Add the effect name mapping in `effectName(...)`.
5. Extend `tests/TimelineSpecialEffectsTest.cpp`.
6. If particle-based:
   - add emitter/particle setup to `TimelineParticleLayer.qml`
   - wire duration through `TimelineEffects.qml`
7. If overlay-based:
   - add a dedicated `Timeline...Effect.qml`
   - expose `durationMs`, `trigger()`, and `reset()`
   - register it in `TimelineEffects.qml`

Prefer a new effect family only when:

- the trigger is obvious to users
- the visual has a distinct, intentional identity
- it does not dilute the feature into novelty noise

## Testing

Current automated coverage is centered on effect detection:

- `tests/TimelineSpecialEffectsTest.cpp`

Verification commands:

- `just build`
- `just lint`
- `ctest --test-dir var/build/native -R komai_timeline_special_effects_test`

Visual tuning remains manual and should be checked in the app after any change
to timing, density, layering, or placement.
