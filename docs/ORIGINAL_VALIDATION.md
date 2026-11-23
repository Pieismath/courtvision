# CourtVision validation record

Validated on the inspected Apple M5 Mac with 16 GB memory, Unreal Engine 5.8 and Blender 5.2. The deliverable is an Apple Silicon native development build.

## September 26 temporal motion pass — 0.1.3 (4)

Rebuilt and updated the native desktop app after a motion-focused Unreal/Blender pass. The evidence is recorded gameplay and complete frame sequences, not isolated beauty frames. No texture, lighting, crowd, stadium or menu polish was part of this pass.

- Captured all 26 requested conditions in a104-second suite, matching before/after alternating strokes,12-second rear footwork, running forehands, serves, and a separate final low-ball correction. Reviewed complete sequences temporally and dense consecutive frames around discontinuities. The suite low-ball segment is explicitly pre-fix; `Captures/MotionPass/after/low_ball_diagonal` supersedes it.
- Final full-quality gameplay recording runs20 seconds / 1,200 frames with 10 hits and no point reset. A12-second side view independently exposes contact, loading and follow-through; frames 250–273 and 482–505 were inspected consecutively.
- Contact-triggered hand jumps fell 24.001/28.981 cm→3.298/4.991 cm in matching alternating fixtures. The source timeline advances through contact rather than rewinding. Running elbow-pole flips were removed; low-ball maximum one-frame hand rotation fell 37.819°→20.162° after spherical interpolation.
- Nine live ground returns crossed the string-bed collision region with ball-center plane clearance 0–2.196 cm, within the 3.35 cm ball radius. Fixture contacts are prescribed and are not counted as live physics validation.
- World-space foot planting, stride timing, passing lanes, support release and pelvis compensation were verified in the complete rear footwork sequence. Recorded final ground-motion planted-foot displacement is 0.00 cm to telemetry precision. Serve deliberately blends away from this foot solver and is not covered by that plant claim.
- Native animation/game-state regression and deterministic physics/scoring checks passed. Final data-only 26-case sweep has6,240 finite poses. The signed bundle is 0.1.3(4). In-app F2–F6 overlay/speed/camera/pause/frame-step controls were checked; camera remained offline.

Remaining limitations: limited source clip variety, flat-footed push-off/braking, weak serve leg drive/landing, and no dedicated compact crowded-ball stroke. This is improved procedural/authored animation, not motion-capture-quality tennis. No physical webcam test or latency benchmark was performed. Capture simulation is 60 Hz and runs slower than real time; encoded 60 FPS does not claim real-time rendering performance.

Evidence: `Captures/MotionPass/review.html` provides synchronized before/after playback and frame stepping; `Captures/MotionPass/ISSUES.md` records causes, rejected iterations, changes, timestamps and remaining work. `Tools/MOTION_QA.md` documents reproduction. Test logs and final pose telemetry are under `Captures/MotionPass/validation`.

## September 22 full-court motion revision — 0.1.2

The fixed baseline constraint from the preceding pass is superseded. Players now choose a receiving depth from the ball trajectory, approach short balls and can play pre-bounce net volleys. Manual depth controls are W/up and down; N toggles an approach. The serve-return bounce requirement remains enforced.

Re-authored forehand and backhand animation curves in Blender, preserving wrist-path velocity through contact and adding torso load/release. Added separate compact volley posing. A post-blend leg solve now follows independently planted world-space feet instead of allowing stroke animation to freeze the legs. Sound events follow foot landings. Sources and precise video/frame inspection notes are recorded in `References/MOTION_REFERENCES.md`; these are original animations informed by observation, not extracted motion capture.

The deterministic suite passed stepping and planted-foot invariance in four directions, short-ball reception on all four surfaces, volley legality/reach/net clearance, and the prior physics/scoring tests. The editor approach capture recorded 18 landings, zero reported foot-target error, and a 3.53 cm volley contact gap after tightening the reach window. The initial overreaching volley failed the capture check and was corrected. Final packaged verification passed on September 22: native state/playback regressions plus motion, grip, rally and serve capture helpers all completed with fresh images. The final run recorded 18 approach landings, 0.00 cm maximum reported foot-target error, 3.53 cm volley contact error, 0.00 cm rally contact error and 0.000 cm serve error. All four grip checks passed, including the free forehand hand and attached backhand support hand. Inspected the step, volley and groundstroke follow-through renders. Detailed measurements are in `Captures/MotionValidation.json`.

The final game binary was rebuilt after the cooked package to correct volley recovery and then copied into the package and re-signed; the verification above ran on that final executable. The capture tests use fixed-step simulation and muted audio, not physical webcam input or a frame-rate benchmark. The preceding performance record remains historical. Wrist/elbow posing, directional crossover steps and anticipatory split-step timing still need refinement; this is not motion-capture-quality movement.

## September 21 baseline positioning follow-up

Both athletes now use a 12.65 m recovery depth (0.765 m behind the baseline), with rally reception beginning at 12.15 m. Removed the old ball-following forward movement and forced 10.5 m swing position. Lateral footwork and the dedicated serve stance remain active. Short-ball approaches are not yet implemented.

The rebuilt packaged native regression suite passed. Physics/scoring checks passed, including 96 one-bounce returns across four surfaces. The packaged rally capture also passed: both athletes measured 1265.0 cm from the net at contact, ball contact depths were 1215.0–1218.2 cm, and inspected racket contact error was 0.00 cm. Fresh contact, follow-through and recovery captures were saved; the contact image was visually inspected. Rally/court capture helpers now reject contact inside the baseline for either player. The earlier performance data was not rerun for this positioning change. macOS audio-device timeout warnings occurred during capture; this run does not validate audio performance.

## September 21 grip and native-resolution release — 0.1.1

- Rebuilt, cooked, packaged and locally signed the app. The packaged native regression suite passes, including actual source-animation playback as well as the serve and groundstroke state checks. The deterministic physics/scoring suite also passed during this pass.
- Fixed the macOS bundle's Retina capability and default rendering to 1920 × 1080 at 100% screen percentage. All four grip captures confirm those settings. TAA replaces TSR after comparison captures showed fewer persistent racket-string afterimages; some lighting/temporal artifacts remain visible.
- The racket now has a fixed anatomical palm attachment, wrist orientation and curled fingers. Forehands use the dominant hand only; backhands solve both arms onto the handle. At the inspected contact and follow-through poses, dominant attachment error was 0.000 cm, backhand support error was 0.000 cm, and the free forehand hand remained 53–70 cm away. These are controlled pose measurements, not proof for every possible tracking position.
- Corrected animation-node updates so source strokes advance through follow-through. A new regression check detects frozen source animation. The avatar remains right-handed; selecting the left physical control hand does not mirror the avatar.
- Sampled the imported serve animation to align the toss endpoint with the actual grip-adjusted racket sweet spot. Packaged measurements report 0.000 cm serve endpoint error and 0.00 cm scripted rally contact error. The serve contact screenshot is taken just after release, so the moving ball is already beyond the strings.
- All four grip, three rally, three serve, menu and four-court capture outputs completed successfully. Inspected fresh stroke and court renders. Close-up modes use fixed-step simulation and are not performance measurements. No physical camera input was tested or enabled.

The final real-time court run reached a 29-shot rally. At native 1080p with 777 spectators, the measured windows were:

| Scene | Average FPS | 95th-percentile frame time | Worst reported frame |
| --- | ---: | ---: | ---: |
| Grass | 19.7 | 135.1 ms | 213.0 ms |
| Night | 16.4 | 197.0 ms | 400.0 ms |
| Clay | 25.0 | 81.8 ms | 400.0 ms |
| Melbourne | 20.1 | 149.7 ms | 199.9 ms |

These shared-machine results are materially slower than an intermediate 1080p run (28–44 FPS) and do not establish the cause of the variation. Engine delta time can clamp at 400 ms, so the reported maximum does not bound wall-clock stalls. Current data is in `Captures/Performance.json`. Smooth native-resolution performance remains unfinished. F1 lowers rendering to 75%, reduces lighting/shadow quality and hides the crowd; that mode was not benchmarked in this final run.

## Completed checks

- Built the Unreal editor module and standalone Mac executable, cooked the game content, packaged the app, and verified its local code signature.
- Launched the standalone app and inspected the actual rendered court, humans, clothing, lighting and HUD. Saved screenshots are actual engine output.
- Reloaded the athlete in a separate Unreal editor process and verified all nine persisted material bindings. UE5.8's Python property setter is required; assigning the underlying transient property does not persist its material cache.
- Ran native game regression checks in both the editor executable and the packaged executable: timed serve, prevention of midflight serve restart, first fault, double fault, and second bounce.
- Ran deterministic C++ checks for drag trajectories, Magnus spin, net clearance, four surface responses, line calls, service boxes, deuce, tiebreak and best-of-three scoring.
- Built and locally signed the Swift companion; 1,212 assertions passed for filtering, calibration, movement, swing detection, tracking loss, event expiry and serialization.
- Tested the actual companion executable over UDP: 403 datagrams, three unique strokes, retained event snapshots with expiry, and neutral shutdown. Median packet interval was 20.6 ms; largest datagram was 638 bytes. These figures measure synthetic transport, not webcam recognition or end-to-end latency.
- Repeated the UDP test for left-handed input: 400 datagrams, three strokes, 20.9 ms median interval, 641-byte maximum packet and neutral shutdown.
- Visually inspected the companion's camera-off, simulated calibration, tracking and stopped states. Camera input was not activated during these automated checks.

- Rebuilt the menu with native Slate buttons and disabled menu mouse capture; verified match activation and surface selection in the packaged application.
- Ran the companion against the actual game on port 7779 for 30 seconds: 1,606 synthetic frames and 13 swing events. Observed body/racket response, a motion-triggered serve and opponent return, then confirmed the HUD returned to camera-offline after shutdown.

- Regenerated the Blender stadium to correct sideline lettering, reimported it and verified all 20 material bindings.
- Verified the final launcher opens the normal game menu and Motion companion with CAMERA OFF and zero swings.
- Rebuilt and signed the refined package. Ran `Tools/capture_native.sh` end to end: five fresh renders, a 41-shot rally across all four surface profiles, and clean automatic exit. Inspected the saved images.

## September 14 serve and stadium pass

- Authored a dedicated 1.2-second overhead serve in Blender, exported/imported the animation, and inspected its posed render. The ball toss uses gravity and meets the authored contact at 0.55 seconds. A new gesture cannot restart or change a pending toss, and pausing freezes it.
- Added three seated spectator variants derived from the licensed athlete, with randomized shirt colors and 777 deterministic placements in the existing seats. They are static and do not cast separate shadows. Initial native screenshots confirmed their orientation and seating alignment.
- Reduced each spectator from about 9,937 to about 6,950 triangles by dissolving hair-card subdivisions along safe UV boundaries and removing distant eyelashes; inspected the updated Blender render.
- Added original procedural stadium ambience, applause and footstep audio. Numerical checks found no clipping. These are synthetic sound-design assets, not recorded crowds.
- Fixed confirmed groundstroke contact to enter the authored 52% phase and continue into follow-through; added anticipatory opponent/demo backswing preparation.
- Rebuilt the editor module and passed runtime regression checks for the serve timeline, duplicate gestures, paused toss, single release, midflight restart prevention, first/double faults, second bounce, and both forehand/backhand contact and recovery. The animation check advances over separate engine frames because Unreal suppresses duplicate updates in one frame.
- Repeated the deterministic physics/scoring suite successfully, including 96 baseline returns with exactly one bounce before each contact across the four surfaces.
- Close-up inspection exposed a visible gap between the old assisted contact and racket. The racket now follows the hand socket directly; contact uses a reachable arm solve, a forward-facing string bed, adjusted baseline positioning and a shared rally-shot depth profile. Audio is preloaded before play to avoid first-impact asset loads.
- Corrected animation tick ordering and compensated the contact frame update so the first evaluated pose lands at contact. An editor runtime measurement reported 0.00 cm between racket sweet spot and the saved contact point for the inspected forehand (rounded to two decimals); the updated close-up was visually inspected. This is a controlled animation alignment check, not a claim about physical camera accuracy.

## September 20 packaged verification

- The final packaged executable passed the full native serve, scoring-event and forehand/backhand timing regression suite.
- All three packaged rally close-ups completed. The measured forehand contact gap was 0.00 cm (rounded), matching the editor result; contact and follow-through images were visually inspected.
- All three packaged serve close-ups completed and were visually inspected. Close-up capture modes use a fixed 60 Hz simulation step to prevent screenshot readback stalls from skipping the desired pose. Normal play and the court performance run use real time.
- The rally capture helper now checks a 5 cm tolerance for its scripted forehand contact measurement. This does not establish accuracy for every body position, stroke or physical camera input.

## Rendering observations

The editor playtest reached more than 80 shots. A grass gameplay sample was approximately 53 FPS. GPU profiling identified Lumen work as the largest night-scene cost; changing from Epic to High global illumination produced approximately 52 FPS in the sampled night scene. The game now starts with High Lumen. These are observed samples, not a sustained performance guarantee; first-use shader/pipeline work and other applications affect frame times. The F1 performance option lowers rendering resolution and shadow quality.

The September 13 visual pass reduced daylight exposure, adjusted the sun diameter, and expanded cached-lighting pre-exposure to cover the scene range. The packaged capture run at 1280 × 720 reported 71.7 FPS on grass, 52.0 at night, 74.0 on clay and 74.7 on Melbourne hard. No cached-lighting clipping warning appeared. These are individual samples from an automated rally, not physical webcam performance.

## September 20 capture and motion-input results

The updated capture helper passed its contact tolerance check and completed the menu/four-court capture run with clean automatic exit. The final run reached a 33-shot rally. The current local app signature was verified again. Actual screenshot files are in `Captures`, with historical frame-time data in `Captures/Performance_PreGrip.json`.

At 1280 × 720 with 777 spectators, the final real-time sampling windows produced:

| Scene | Average FPS | 95th-percentile frame time | Worst frame |
| --- | ---: | ---: | ---: |
| Grass | 35.9 | 81.5 ms | 235.0 ms |
| Night | 31.9 | 107.4 ms | 136.7 ms |
| Clay | 44.8 | 45.5 ms | 89.9 ms |
| Melbourne | 56.2 | 25.8 ms | 86.6 ms |

An earlier run of the same build averaged 53.6 / 40.3 / 60.3 / 53.6 FPS on grass / night / clay / Melbourne. The variation and long frames mean this is not a locked-60-FPS result. Night lighting remains costly; further profiling and frame-time optimization are required. These measurements are from the shared Mac with other applications running, not a controlled hardware benchmark.

The actual Motion companion sent 1,536 synthetic frames and 13 swings over 30 seconds to the packaged game. The HUD showed synthetic input; native logs recorded motion-triggered serves, opponent returns and points. After shutdown, the HUD returned to camera offline. The game was returned to its menu and the normal companion opened with CAMERA OFF and zero swings. No camera was accessed.

## Remaining production work

Physical webcam playtesting across people, camera placements and lighting is required to establish control accuracy, comfort and hand-to-display latency. The anatomical human and original procedural animation are not MetaHuman or tennis motion capture. Further foot-plant and split-step refinement, richer opponent tactics, spectator diversity and animation, richer sound design, replay, and separate stadium architecture remain production work. A dedicated procedural serve and static spectators are now present. Four surface/lighting profiles currently share one authored stadium. The selected real-world hand maps to the current right-handed avatar.

This build is not represented as finished AAA-quality tennis.


## September 26 arm clearance and running follow-up — 0.1.4 (5)

Player feedback exposed gaps in the previous motion acceptance criteria: rigid grip attachment did not prevent the arm crossing the torso, and steady planted feet did not establish a natural running gait. This follow-up changed those systems and retained recordings from rejected iterations.

- Added torso-aware wrist routing and arm clearance, bounded elbow continuity, consistent source poses for repeated corrections, and continuous free-arm solving through grip release/recovery.
- Added speed/direction-based running with narrower foot lanes, alternating knee/heel recovery, airborne ankle pitch, a taller moving pelvis and one-handed racket carry. Pelvis reach compensation includes running bob so planted feet are not displaced.
- Recorded fresh baseline and final versions. Reviewed complete ordered sequences for alternating strokes (6 s), directional footwork (12 s), running backhand (4 s), crowded ball (4 s), side-view rally (10 s) and full-quality gameplay rally (20 s). Inspected dense consecutive frames around preparation, contact, regripping and direction changes. Normal/quarter-speed playback and frame stepping remain available in the comparison page.
- A live-rally recovery intersection discovered in candidate 6 was corrected in candidate 7 and re-recorded. The exact interval, cause and before/after review are in `Captures/MotionPass/FOLLOWUP.md`.
- The final native self-test passes, including timed serve, duplicate/paused gestures, fault/second-bounce rules and forehand/backhand contact/recovery. Independent physics and new run-versus-shuffle checks pass.
- All 6,240 fixture and 1,200 live-rally bone frames are finite. The final rendered rally reaches 10 hits. The dominant racket socket gap and horizontal planted-foot displacement are zero at recorded precision.
- The approximate torso-envelope audit improves from 423 flagged fixture frames in the released baseline to 4; it still flags 1 frame in the final rally. This is not mesh collision proof. Tight preparation, compact strokes, toe-off, braking and running arm relaxation remain work.
- Verified all six final rendered clips have the expected frame counts, matching telemetry, and playable normal/quarter-speed encodings. Checked the packaged executable matches the built code UUID, verified its local signature, and launched the normal menu with camera offline.

Final evidence: `Captures/MotionPass/followup-final`. Video uses a fixed 60 Hz simulation captured slower than real time; these are not performance measurements. No physical webcam accuracy or latency claim is made. The motion is substantially improved in the reviewed cases, but remains procedural and is not production mocap quality.
