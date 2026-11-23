# Publication verification — November 23, 2026

The following checks were rerun on the source copied into this repository.

## C++ simulation and game rules

From the repository root on macOS with Xcode command-line tools:

```sh
zsh Tests/run_physics.sh
```

Result: PASS for planted-foot invariance, stepping in four directions, directional run versus shuffle, short-ball approach, legal net volleys, drag trajectories, spin, net clearance, four surface bounces, 96 baseline returns after one bounce, line calls, service boxes, deuce, tiebreak, and best-of-three scoring.

## Swift motion-control core

```sh
mkdir -p .build/module-cache
xcrun swiftc -swift-version 5 -O -module-cache-path .build/module-cache \
  VisionBridge/Sources/MotionCore.swift \
  VisionBridge/Tests/MotionCoreTests.swift -o .build/motion-tests
.build/motion-tests
```

Result: PASS, 1,212 assertions across filtering, calibration, movement, swing, loss, and wire format.

These checks exercise deterministic simulation and synthetic pose inputs. They do not measure physical webcam accuracy, hand-to-display latency, live rendering frame rate, or the visual quality of the full native game. The Unreal integration was not rebuilt for this publication; its scene assets are outside this repository.

## Media provenance

`media/gameplay-4k.png` is an existing 3840 × 2160 CourtVision engine capture prepared on November 5, 2026. `media/gameplay.png` is its smaller viewing copy. Neither is an AI-generated concept image.

## Repository scope

This is a curated source showcase assembled from the native project. It preserves the C++ module and Swift source alongside standalone tests. Packaged binaries, Unreal content, engine files, and large source art are excluded. The earlier Baseline browser game is separately hosted and is not the native version pictured here.
