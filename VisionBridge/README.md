# CourtVision Motion

Open **CourtVision Motion.app**. The camera begins off. Press **Start camera**, allow the macOS camera prompt, stand where your shoulders, hips and selected hand are visible, then press **Calibrate stance** and hold still until calibration completes. Choose your racket hand before calibrating. Keep this companion open while playing CourtVision.

The app processes frames on this Mac using AVFoundation and Apple Vision. It sends only normalized pose/control numbers to `127.0.0.1:7779`; it does not save or transmit webcam images. The preview is mirrored. Moving right in the preview moves the tennis player right. Swinging the selected hand relative to the torso generates a stroke, so a sidestep alone should not hit the ball.

Body and hand inference share a serial queue and discard late frames. One Euro filtering reduces stationary jitter while relaxing smoothing during fast motion. Calibration establishes shoulder scale and neutral wrist/torso positions. Confidence rejection, travel thresholds, rearming and a refractory period suppress spurious strokes. Missing tracking neutralizes controls, and the receiver applies a 250 ms arrival timeout. The camera watchdog uses 200 ms.

The on-screen inference time measures Vision processing, not the complete hand-to-display latency. End-to-end latency, recognition accuracy and comfort require physical playtesting under real lighting; synthetic tests cannot establish them.

## Protocol

JSON UDP packets, version 1. `seq` and `timestamp` increase monotonically (timestamp is sender uptime, not wall time). `tracking`, `calibrated`, `confidence`, `hand`, `source`, `inferenceMs`, `bodyX`, `racketX`, `racketY`, `velocityX`, `velocityY`, `swing`, `swingId`, `speed`, `aimX`, `spin`. Axes are mirrored preview X-right/Y-up. Positions, aim and spin are normalized to −1…1; power is 0…1. `source` distinguishes camera from synthetic input.

`SwingId` advances once per detected stroke. The additive fields `swingSpeed`, `swingAim`, and `swingSpin` retain that event's payload for recovery when the renderer drains several packets in one frame. A receiver must deduplicate by `swingId`, gate on tracking/calibration/confidence, validate finite/bounded numbers, discard stale sequences, and never replay an event when tracking is reacquired.

## Build and verify

`./build.sh` compiles and locally signs the app, then runs 1,212 assertions covering filtering, calibration, movement, swing detection, loss, reacquisition and serialization. `python3 Tests/validate_udp.py` runs the actual compiled executable with synthetic motion against a random local UDP port. The verified run delivered 403 packets, three unique strokes, neutral shutdown, median packet interval 20.6 ms and maximum datagram 638 bytes. This is synthetic transport evidence, not a camera latency benchmark.

`CourtVision Motion.app/Contents/MacOS/CourtVisionMotion --synthetic --seconds 8 --port 7779` sends synthetic control input without opening a camera. The companion also has a clearly labeled simulated-motion preview.

Stroke recovery uses a stable stream UUID and an immutable event snapshot that expires after 250 ms. Tracking loss, calibration and hand changes invalidate it immediately. Unreal baselines the event ID on reconnect instead of replaying previous movement.
