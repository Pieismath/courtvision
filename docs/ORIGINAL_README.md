# CourtVision

Native computer-vision tennis, built with Unreal Engine 5.8, Blender 5.2, and Apple Vision on an M5 Mac.

## Play

Open **Play CourtVision.command**. The packaged game is also at `Build/Mac/CourtVision.app` and runs without opening Unreal Editor. It opens the Motion companion and the native game. In the companion, start the camera, select your racket hand, and calibrate a still ready stance with shoulders, hips and hand visible. In the game, choose a surface and press **Enter** for a match or **P** for rally practice.

Move your torso left and right to position the player. Swing your selected hand relative to your body to hit. A stronger swing produces a faster shot; lateral follow-through aims it, and upward/downward motion changes spin. Longitudinal movement and a generous contact window assist play from a single webcam. The camera remains local and starts only when you press its Start button. The game is also playable with the keyboard.

| Key | Action |
| --- | --- |
| A / D or left / right arrows | Move laterally and aim |
| W / up arrow, down arrow | Move toward the net / retreat |
| N | Toggle net approach and automatic depth assistance |
| Space or left mouse | Swing / serve |
| Shift + swing | More power |
| S + swing | Slice |
| Enter | Start a match; restart after match end |
| P | Start rally practice |
| 1–4 | Grass, New York hard, Paris clay, Melbourne hard |
| V | Cycle three gameplay cameras |
| M / Escape | Open menu / resume |
| F1 | Toggle performance / quality rendering and spectators |
| F2 | Toggle motion diagnostics |
| F3 | Cycle normal, half and quarter speed |
| F4 | Cycle motion-inspection cameras |
| F5 / F6 | Pause motion / advance one frame |

The launcher opens a native 1920 × 1080 window with Retina support. Quality mode renders at 100% resolution; F1 switches to 75% rendering and hides spectators for better performance. The current avatar uses a right-handed, single-hand forehand and a two-handed backhand. Choosing the left hand in Motion changes the tracked real-world hand; it does not yet mirror the avatar.

A match uses best-of-three sets, deuce/advantage, alternating service, second serves/double faults and a seven-point tiebreak at six games all. Practice automatically feeds a ball after each point. Court surfaces change restitution/friction; flight includes gravity, aerodynamic drag and spin lift, simulated at 240 Hz.

## What is here

- `Unreal/CourtVision.uproject`: editable native C++ project and imported game content.
- `VisionBridge/CourtVision Motion.app`: compiled Apple Silicon webcam companion, with its complete Swift source and tests.
- `Assets/Environment`: editable Blender stadium, equipment, FBX exports, original 2K PBR surfaces and procedural generation scripts.
- `Assets/Athletes`: anatomically modeled, textured, rigged human; ready, running, forehand, backhand and overhead serve clips; editable Blender source and licensing records.
- `Assets/Crowd`: three seated spectator variants, editable Blender source and deterministic stadium placement.
- `Assets/Audio`: original racket/ball Foley, a stadium ambience loop, applause and footsteps.
- Native animation blends locomotion and one-shot strokes, with connected arm solves, a rigid palm-relative racket attachment, curled grip fingers and a support-hand grip during backhands. Serves have a gravity-driven toss and timed overhead contact; groundstrokes continue into follow-through at collision.
- `Captures/MotionPass`: complete 60 Hz motion recordings, normal/quarter-speed video, before/after comparison player, bone telemetry and the motion issue log.
- `Captures`: historical static rendering checks.
- `Tools`: reproducible asset import, audio generation, build and packaging scripts.
- `Tests`: deterministic native ball-physics and tennis-scoring checks.

The four court profiles share one original stadium. They evoke the Grand Slam surface types; this build does not contain four completed stadium replicas or official tournament branding.

The current build and test evidence is recorded in `VALIDATION.md`.

## Development status

This is a playable native development build pursuing the requested visual target. It is not represented as a finished AAA release. Current animation is authored procedural motion, not tennis motion capture or MetaHuman. Further production work includes heel/toe articulation, richer movement and serve clips, tennis motion capture, deeper opponent tactics, spectator diversity and animation, richer environmental audio, replay, separate venue architecture, and physical camera playtesting across people and lighting.

The automated tests establish simulation/control behavior, not actual webcam recognition accuracy or hand-to-display latency. The companion displays measured inference time. Real responsiveness and comfort still need a person playing through the camera.

## Motion review

Version 0.1.3 adds repeatable motion capture, stable hand-relative grips, continuous stroke/contact timing, elbow continuity, velocity-matched footwork and planted-foot locks. Use `Tools/MOTION_QA.md` for the 26-scenario suite, multi-angle recording and frame-by-frame review. The capture workflow is the primary animation check; a static image is not evidence that a swing is correct.

## Rebuild

The installed engine defaults to `/Users/Shared/Epic Games/UE_5.8`; set `COURTVISION_ENGINE` to another UE5.8 installation if needed. `Tools/build_native.sh` builds the native module/companion and runs the actual full-editor Python import pipeline. Full-editor Python is used because this UE version asserts in its headless FBX warning UI. `Tools/package_native.sh` cooks and archives a native Mac application. `Tools/record_motion.sh` is the primary motion QA workflow described above. `Tools/capture_native.sh` retains historical static rendering checks; its older grip/rally pose checks are superseded by the recorded motion suite. `Tests/run_physics.sh` and `VisionBridge/build.sh` run independent tests; `Tests/run_native.sh` checks gameplay regressions through Unreal.

This project enables software Lumen, Metal SM6, virtual shadow maps and temporal anti-aliasing, with scalable resolution for the 16 GB machine. macOS hardware-ray-traced Lumen is not used. The authored stadium uses ordinary static meshes; Nanite is enabled at project level but is not claimed as an asset feature.

Retain `Assets/Athletes/CREDITS.md` and its `Licenses` files when redistributing the athlete; some clothing is CC-BY. Original stadium, equipment, surface textures, code and procedural Foley were authored in this workspace.

Players recover behind the baseline for deep rallies, move forward for short balls, and can approach the net for volleys. Depth assistance follows the predicted reception point; W/up and down provide manual depth control, and N toggles a net approach. Forehand and two-handed backhand groundstrokes use revised Blender curves; volleys use a compact one-handed block. Feet lift and land individually, with world-space planting independent of the stroke animation. See `References/MOTION_REFERENCES.md` for the footage and annotated frames used.


### Version 0.1.4 — arms and directional running

Arms now route around the torso, with a continuous elbow solution through grip release and recovery. Longer court coverage changes from adjustment shuffles to direction-facing running with alternating knee/heel recovery and one-handed racket carry. World-space foot contacts stay locked while planted.

Use the existing desktop **Play CourtVision.command** to open this build. The focused before/after clips are `Captures/MotionPass/followup-final/running-comparison.mp4` and `swings-comparison.mp4`. The motion review page defaults to the old/new running comparison and supports normal speed, quarter speed and individual frames. Details and remaining limitations are in `Captures/MotionPass/FOLLOWUP.md`.

This remains procedural tennis animation. Toe-off, hard braking and crowded-ball strokes still need refinement; the update is not a claim that all clipping or motion-quality issues are eliminated.
