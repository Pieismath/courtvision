import Foundation

@main enum MotionCoreTests {
    static var assertions = 0
    static func expect(_ condition: @autoclosure () -> Bool, _ description: String) {
        assertions += 1
        guard condition() else { fputs("FAIL: \(description)\n", stderr); exit(1) }
    }
    static func pose(_ time: Double, bodyX: Double = 0.5, wristX: Double = 0.63, confidence: Double = 0.95) -> PoseSample {
        PoseSample(timestamp: time, body: Point2(x: bodyX, y: 0.49), shoulderWidth: 0.2,
                   wrist: Point2(x: wristX, y: 0.50), confidence: confidence)
    }
    static func calibratedEngine() -> MotionEngine {
        let engine = MotionEngine(); engine.beginCalibration()
        for i in 0...60 { _ = engine.update(pose(Double(i)/60)) }
        expect(engine.calibrated, "A stable ready stance calibrates")
        return engine
    }
    static func main() {
        testFiltering()
        testCalibration()
        testBodyMovementDoesNotStrike()
        testSwingsAndRefractory()
        testEventFreshnessAndSnapshots()
        testLeftHandSimulation()
        testLossAndReacquisition()
        testMalformedInput()
        testJSON()
        print("PASS: \(assertions) assertions across filtering, calibration, movement, swing, loss, and wire format.")
    }
    static func testFiltering() {
        var filter = OneEuroFilter()
        for i in 0...100 { expect(abs(filter.filter(0.42, at: Double(i)/60)-0.42) < 1e-9, "Constant input stays constant") }
        filter.reset()
        var rawEnergy = 0.0, outputEnergy = 0.0
        for i in 0...180 {
            let raw = 0.5+(i%2 == 0 ? 0.009 : -0.009)
            let output = filter.filter(raw, at: Double(i)/60)
            if i > 60 { rawEnergy += pow(raw-0.5, 2); outputEnergy += pow(output-0.5, 2) }
        }
        expect(outputEnergy < rawEnergy*0.1, "Filter removes at least 90% of stationary jitter energy")
        let held = filter.filter(1, at: 2.9)
        expect(abs(held-0.5) < 0.02, "Out-of-order timestamps cannot move the filter")
        let resumed = filter.filter(0.85, at: 10)
        expect(abs(resumed-0.85) < 1e-9, "Long gaps reset stale filter state")
        filter.reset(); _ = filter.filter(0, at: 0)
        var stepped = 0.0
        for i in 1...6 { stepped = filter.filter(1, at: Double(i)/60) }
        expect(stepped > 0.92, "Fast movement reaches 92% of a step within 100ms")
    }
    static func testCalibration() {
        let engine = MotionEngine()
        let before = engine.update(pose(0))
        expect(before.tracking && !before.calibrated && before.bodyX == 0 && !before.swing, "Uncalibrated pose never moves gameplay")
        engine.beginCalibration()
        for i in 0...30 { _ = engine.update(pose(Double(i)/60)) }
        expect(!engine.calibrated, "Half a second is not enough for calibration")
        _ = engine.update(pose(0.52, bodyX: 0.65, wristX: 0.78))
        expect(engine.calibrationProgress == 0, "Moving body restarts calibration")
        for i in 32...100 { _ = engine.update(pose(Double(i)/60, bodyX: 0.65, wristX: 0.78)) }
        expect(engine.calibrated, "Calibration succeeds after the new stance settles")
        let center = engine.update(pose(1.7, bodyX: 0.65, wristX: 0.78))
        expect(abs(center.bodyX) < 1e-6 && abs(center.racketX) < 1e-6, "Calibrated neutral stance maps to center")
        engine.hand = "left"
        expect(!engine.calibrated, "Changing racket hand requires new neutral calibration")
    }
    static func testBodyMovementDoesNotStrike() {
        let engine = calibratedEngine()
        var final = MotionPacket()
        for i in 61...150 {
            let shift = min(0.24, Double(i-61)*0.006)
            final = engine.update(pose(Double(i)/60, bodyX: 0.5+shift, wristX: 0.63+shift))
            expect(!final.swing, "Sidesteps without relative hand movement never strike")
        }
        expect(final.bodyX > 0.65, "Sidestep moves the normalized player right")
        expect(abs(final.racketX) < 1e-6, "Torso-relative racket remains neutral during sidestep")
    }
    static func testSwingsAndRefractory() {
        let engine = MotionEngine(); engine.source = "synthetic"; engine.beginCalibration()
        var events: [MotionPacket] = []
        var lastSeq: UInt64 = 0
        for i in 0...480 {
            let packet = engine.update(SyntheticPose.sample(time: Double(i)/60, start: 0))
            expect(packet.seq > lastSeq, "Sequence strictly increases")
            lastSeq = packet.seq
            if packet.swing { events.append(packet) }
        }
        expect(events.count == 3, "Three separated synthetic strokes produce exactly three hits")
        for (index, event) in events.enumerated() {
            expect(event.swingId == UInt64(index+1), "Swing IDs remain monotonic")
            expect(event.velocityX > 0 && event.spin > 0, "Rightward rising swing reports correct direction and topspin")
            expect(event.speed > 0.30 && event.speed <= 1, "Swing power is useful and bounded")
        }
        let final = engine.update(SyntheticPose.sample(time: 8.02, start: 0))
        expect(!final.swing && final.swingId == 3, "Event flag clears; event ID persists for packet-loss recovery")
        expect(final.swingSpeed > 0.3, "Stroke power survives later quiet frames for packet-loss recovery")
        expect(final.swingAim.isFinite && final.swingSpin.isFinite, "Recovered stroke payload stays finite")
    }
    static func testLossAndReacquisition() {
        let engine = calibratedEngine()
        let active = engine.update(pose(1.05, bodyX: 0.65, wristX: 0.81))
        expect(active.bodyX > 0, "Player is moving before loss")
        let missing = engine.lost(at: 1.07)
        expect(!missing.tracking && missing.bodyX == 0 && missing.velocityX == 0 && !missing.swing, "Tracking loss immediately neutralizes every control")
        let resumed = engine.update(pose(1.09, bodyX: 0.65, wristX: 0.96))
        expect(resumed.velocityX == 0 && !resumed.swing, "Reacquisition cannot generate phantom swings")
        expect(engine.watchdog(at: 1.15) == nil, "Watchdog allows a fresh frame")
        let stale = engine.watchdog(at: 1.31)
        expect(stale != nil && stale?.tracking == false && stale?.bodyX == 0, "A 200ms camera stall neutralizes input")
        let longGap = engine.update(pose(4.0, wristX: 0.2))
        expect(longGap.velocityX == 0 && !longGap.swing, "Long frame gaps reset motion derivatives")
        let lowConfidence = engine.update(pose(4.02, confidence: 0.1))
        expect(!lowConfidence.tracking && lowConfidence.speed == 0, "Low confidence never moves the player")
    }
    static func testEventFreshnessAndSnapshots() {
        let engine = MotionEngine(); engine.beginCalibration()
        var event = MotionPacket()
        for i in 0...150 {
            let packet = engine.update(SyntheticPose.sample(time: Double(i)/60, start: 0))
            if packet.swing { event = packet; break }
        }
        expect(event.swingId == 1 && event.swingValid, "A new stroke has a fresh recovery snapshot")
        expect(event.swingTimestamp == event.timestamp, "Event timestamp marks the actual detected stroke")
        let near = engine.update(SyntheticPose.sample(time: event.timestamp+0.04, start: 0))
        expect(near.swingValid && !near.swing, "Snapshot remains eligible across coalesced renderer frames")
        expect(near.swingSpeed == event.speed && near.swingAim == event.aimX && near.swingSpin == event.spin, "Snapshot preserves exact stroke payload as the wrist continues moving")
        let after = engine.update(SyntheticPose.sample(time: event.timestamp+0.18, start: 0))
        expect(after.swingValid, "A recent stroke survives an ordinary short renderer delay")
        let expired = engine.update(SyntheticPose.sample(time: event.timestamp+0.27, start: 0))
        expect(!expired.swingValid && expired.swingId == event.swingId, "A stroke older than 250ms cannot be replayed")
        expect(expired.swingSpeed == event.swingSpeed, "Expired snapshot remains immutable for telemetry")
        let lost = engine.lost(at: event.timestamp+0.28)
        expect(!lost.swingValid && lost.swingId == event.swingId, "Loss preserves ID but disallows event recovery")
        let resumed = engine.update(SyntheticPose.sample(time: event.timestamp+0.30, start: 0))
        expect(!resumed.swingValid && !resumed.swing, "Reacquisition never revives an old snapshot")
        let reverse = engine.update(pose(event.timestamp-1))
        expect(reverse.timestamp > resumed.timestamp && !reverse.tracking, "Rejected old sample cannot regress output timestamps")
        expect(reverse.streamId == event.streamId && reverse.seq > resumed.seq, "A stream retains its identity and increasing sequence across loss")
        expect(MotionEngine().streamId != engine.streamId, "A restarted engine uses a fresh stream identity")
        let gap = MotionEngine(); gap.beginCalibration()
        for i in 0...25 { _ = gap.update(pose(Double(i)/60)) }
        _ = gap.update(pose(3))
        expect(!gap.calibrated && gap.calibrationProgress == 0, "A camera gap cannot count as holding a stable calibration stance")
    }
    static func testLeftHandSimulation() {
        let engine = MotionEngine(); engine.hand = "left"; engine.beginCalibration()
        var events = 0
        for i in 0...480 {
            let sample = SyntheticPose.sample(time: Double(i)/60, start: 0, hand: "left")
            expect(sample.joints["leftWrist"] == sample.wrist, "Left-handed preview and control use the same wrist")
            let packet = engine.update(sample)
            if packet.swing {
                events += 1
                expect(packet.hand == "left" && packet.velocityX < 0 && packet.spin > 0, "Left forehand mirrors lateral direction while preserving topspin")
            }
        }
        expect(events == 3, "Three left-handed strokes produce exactly three events")
    }
    static func testMalformedInput() {
        let engine = calibratedEngine()
        var invalid = pose(1.1); invalid.wrist.x = .nan
        expect(!engine.update(invalid).tracking, "Nonfinite points are rejected")
        invalid = pose(1.2); invalid.shoulderWidth = 0
        expect(!engine.update(invalid).tracking, "Zero shoulder width cannot divide controls by zero")
        invalid = pose(1.3); invalid.confidence = .nan
        expect(!engine.update(invalid).tracking, "Nonfinite confidence is rejected")
    }
    static func testJSON() {
        let engine = calibratedEngine()
        let packet = engine.update(pose(1.1))
        let encoded = try! JSONEncoder().encode(packet)
        let roundTrip = try! JSONDecoder().decode(MotionPacket.self, from: encoded)
        expect(roundTrip.version == 1 && roundTrip.tracking && roundTrip.calibrated && roundTrip.hand == "right", "Wire schema round-trips without losing control state")
        expect(encoded.count < 1500, "Each control datagram fits a typical network MTU")
        expect(roundTrip.timestamp == 1.1 && roundTrip.seq == packet.seq, "Timestamps and sequence survive serialization")
    }
}
