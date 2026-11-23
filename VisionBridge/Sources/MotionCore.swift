import Foundation

struct Point2: Codable, Equatable {
    var x: Double
    var y: Double
    static let zero = Point2(x: 0, y: 0)
    static func + (a: Self, b: Self) -> Self { .init(x: a.x+b.x, y: a.y+b.y) }
    static func - (a: Self, b: Self) -> Self { .init(x: a.x-b.x, y: a.y-b.y) }
    static func * (a: Self, b: Double) -> Self { .init(x: a.x*b, y: a.y*b) }
    var length: Double { hypot(x, y) }
    var isFinite: Bool { x.isFinite && y.isFinite }
}

func clamp(_ value: Double, _ low: Double = -1, _ high: Double = 1) -> Double {
    min(high, max(low, value))
}

/// The One Euro filter increases cutoff while moving, reducing lag without stationary jitter.
/// State and timestamps belong to one stream, processed serially by the capture queue.
struct OneEuroFilter {
    var minimumCutoff = 2.2
    var beta = 6.0
    var derivativeCutoff = 1.0
    private var value: Double?
    private var rawValue: Double?
    private var derivative = 0.0
    private var timestamp: Double?
    mutating func reset() { value = nil; rawValue = nil; timestamp = nil; derivative = 0 }
    mutating func filter(_ sample: Double, at time: Double) -> Double {
        guard sample.isFinite, time.isFinite else { return value ?? 0 }
        guard let previous = value, let raw = rawValue, let lastTime = timestamp else {
            value = sample; rawValue = sample; timestamp = time
            return sample
        }
        let elapsed = time-lastTime
        guard elapsed > 0 else { return previous }
        // A camera interruption must never turn reacquisition into a huge velocity.
        guard elapsed < 0.25 else { reset(); return filter(sample, at: time) }
        let dt = max(0.001, elapsed)
        func alpha(_ cutoff: Double) -> Double { 1 / (1 + 1 / (2 * .pi * cutoff * dt)) }
        let derivativeAlpha = alpha(derivativeCutoff)
        derivative += derivativeAlpha * ((sample-raw)/dt-derivative)
        let cutoff = minimumCutoff + beta * abs(derivative)
        let next = previous + alpha(cutoff) * (sample-previous)
        value = next; rawValue = sample; timestamp = time
        return next
    }
}

struct PoseSample {
    var timestamp: Double
    var body: Point2
    var shoulderWidth: Double
    var wrist: Point2
    var confidence: Double
    var joints: [String: Point2] = [:]
    var handDetailed = false
    var valid: Bool {
        timestamp.isFinite && body.isFinite && wrist.isFinite && shoulderWidth.isFinite &&
        shoulderWidth > 0.055 && shoulderWidth < 0.85 && confidence.isFinite && confidence >= 0.35
    }
}

struct MotionPacket: Codable {
    var version = 1
    var streamId = ""
    var seq: UInt64 = 0
    var timestamp: Double = 0
    var source = "camera"
    var tracking = false
    var calibrated = false
    var hand = "right"
    var confidence: Double = 0
    var inferenceMs: Double = 0
    var bodyX: Double = 0
    var racketX: Double = 0
    var racketY: Double = 0
    var velocityX: Double = 0
    var velocityY: Double = 0
    var swing = false
    var swingId: UInt64 = 0
    var swingTimestamp: Double = 0
    var swingValid = false
    var swingSpeed: Double = 0
    var swingAim: Double = 0
    var swingSpin: Double = 0
    var speed: Double = 0
    var aimX: Double = 0
    var spin: Double = 0
}

final class MotionEngine {
    let streamId = UUID().uuidString
    private(set) var calibrated = false
    private(set) var calibrationProgress = 0.0
    private(set) var isCalibrating = false
    private(set) var swingId: UInt64 = 0
    private var strokeSpeed: Double = 0
    private var strokeAim: Double = 0
    private var strokeSpin: Double = 0
    private(set) var calibrationMessage = "Stand 2–3 m away, then calibrate your ready stance."
    var hand = "right" {
        didSet { if hand != oldValue { clearCalibration() } }
    }
    var source = "camera"
    private var seq: UInt64 = 0
    private var calibrationSamples: [PoseSample] = []
    private var neutralBody = Point2(x: 0.5, y: 0.5)
    private var neutralWristOffset = Point2.zero
    private var referenceWidth = 0.2
    private var filters = [OneEuroFilter(), OneEuroFilter(), OneEuroFilter(), OneEuroFilter()]
    private var previousWrist: Point2?
    private var previousTime: Double?
    private var lastGoodTime: Double?
    private var lastSwingTime = -Double.infinity
    private var eventEligible = false
    private var lastPacketTime: Double?
    private var quietSince: Double?
    private var armed = true
    private var travel = 0.0
    private var previousPacket = MotionPacket()

    func beginCalibration() {
        clearCalibration()
        isCalibrating = true
        calibrationMessage = "Hold your ready stance still for one second."
    }

    func clearCalibration() {
        calibrated = false; isCalibrating = false; calibrationProgress = 0
        calibrationSamples.removeAll()
        calibrationMessage = "Stand 2–3 m away, then calibrate your ready stance."
        eventEligible = false
        resetMotion()
    }

    private func resetMotion() {
        for i in filters.indices { filters[i].reset() }
        previousWrist = nil; previousTime = nil; quietSince = nil
        travel = 0; armed = true
    }

    private func basePacket(time: Double) -> MotionPacket {
        seq &+= 1
        var packet = MotionPacket()
        let safeTime = time.isFinite && time >= 0 ? time : ProcessInfo.processInfo.systemUptime
        let packetTime = lastPacketTime.map { max(safeTime, $0.nextUp) } ?? safeTime
        packet.seq = seq; packet.timestamp = packetTime; packet.streamId = streamId
        lastPacketTime = packetTime
        packet.calibrated = calibrated; packet.hand = hand
        packet.source = source; packet.swingId = swingId
        packet.swingTimestamp = lastSwingTime.isFinite ? lastSwingTime : 0
        packet.swingValid = eventEligible && time >= lastSwingTime && time-lastSwingTime <= 0.25
        packet.swingSpeed = strokeSpeed; packet.swingAim = strokeAim; packet.swingSpin = strokeSpin
        return packet
    }

    /// Called immediately for missing/low-confidence pose and periodically by the watchdog.
    /// No last-known motion is held during loss. Calibration may survive short occlusions.
    func lost(at timestamp: Double) -> MotionPacket {
        resetMotion()
        eventEligible = false
        if isCalibrating {
            calibrationSamples.removeAll(); calibrationProgress = 0
            calibrationMessage = "Keep shoulders and your selected hand in view."
        }
        let packet = basePacket(time: timestamp)
        previousPacket = packet
        return packet
    }

    func watchdog(at timestamp: Double) -> MotionPacket? {
        guard let lastGoodTime, timestamp-lastGoodTime <= 0.20 else { return lost(at: timestamp) }
        return nil
    }

    func update(_ pose: PoseSample) -> MotionPacket {
        guard pose.valid else { return lost(at: pose.timestamp.isFinite ? pose.timestamp : ProcessInfo.processInfo.systemUptime) }
        if let lastGoodTime, pose.timestamp <= lastGoodTime { return lost(at: pose.timestamp) }
        if let lastGoodTime, pose.timestamp-lastGoodTime > 0.20 {
            resetMotion(); eventEligible = false
            if isCalibrating { calibrationSamples.removeAll(); calibrationProgress = 0 }
        }
        lastGoodTime = pose.timestamp
        if isCalibrating { collectCalibration(pose) }
        var packet = basePacket(time: pose.timestamp)
        packet.tracking = true; packet.confidence = clamp(pose.confidence, 0, 1)
        guard calibrated else { previousPacket = packet; return packet }

        let body = Point2(x: filters[0].filter(pose.body.x, at: pose.timestamp),
                          y: filters[1].filter(pose.body.y, at: pose.timestamp))
        let wrist = Point2(x: filters[2].filter(pose.wrist.x, at: pose.timestamp),
                           y: filters[3].filter(pose.wrist.y, at: pose.timestamp))
        let normalized = (wrist-body-neutralWristOffset) * (1/(referenceWidth*1.5))
        packet.bodyX = clamp((body.x-neutralBody.x)/(referenceWidth*1.65))
        packet.racketX = clamp(normalized.x)
        packet.racketY = clamp(normalized.y)
        if let previousWrist, let previousTime {
            let dt = pose.timestamp-previousTime
            if dt > 0.001, dt < 0.20 {
                // Racket velocity is relative to torso, so a sidestep alone does not strike.
                let velocity = (normalized-previousWrist) * (1/dt)
                packet.velocityX = clamp(velocity.x, -12, 12)
                packet.velocityY = clamp(velocity.y, -12, 12)
                let magnitude = velocity.length
                packet.speed = clamp(magnitude/5.5, 0, 1)
                packet.aimX = clamp(0.58*packet.racketX + 0.16*packet.velocityX)
                packet.spin = clamp(packet.velocityY/3.8)
                if magnitude < 0.65 {
                    if quietSince == nil { quietSince = pose.timestamp }
                    if pose.timestamp-(quietSince ?? pose.timestamp) > 0.09 { armed = true; travel = 0 }
                } else { quietSince = nil }
                if magnitude > 1.25 { travel += magnitude*dt } else { travel = max(0, travel-dt*2) }
                if armed && magnitude > 1.65 && travel > 0.09 && pose.timestamp-lastSwingTime > 0.25 {
                    swingId &+= 1
                    packet.swing = true; packet.swingId = swingId
                    packet.swingTimestamp = pose.timestamp; packet.swingValid = true; eventEligible = true
                    strokeSpeed = packet.speed; strokeAim = packet.aimX; strokeSpin = packet.spin
                    packet.swingSpeed = strokeSpeed; packet.swingAim = strokeAim; packet.swingSpin = strokeSpin
                    lastSwingTime = pose.timestamp; armed = false; travel = 0
                }
            }
        }
        previousWrist = normalized; previousTime = pose.timestamp
        previousPacket = packet
        return packet
    }

    private func collectCalibration(_ pose: PoseSample) {
        if let initial = calibrationSamples.first {
            let bodyDrift = (pose.body-initial.body).length/initial.shoulderWidth
            let wristDrift = ((pose.wrist-pose.body)-(initial.wrist-initial.body)).length/initial.shoulderWidth
            if bodyDrift > 0.15 || wristDrift > 0.28 {
                calibrationSamples.removeAll(); calibrationProgress = 0
                calibrationMessage = "Hold still — keep your racket hand comfortably in front."
            }
        }
        calibrationSamples.append(pose)
        let elapsed = pose.timestamp-(calibrationSamples.first?.timestamp ?? pose.timestamp)
        calibrationProgress = min(1, elapsed/0.85)
        guard elapsed >= 0.85, calibrationSamples.count >= 12 else { return }
        let count = Double(calibrationSamples.count)
        neutralBody = calibrationSamples.reduce(Point2.zero) { $0+$1.body } * (1/count)
        neutralWristOffset = calibrationSamples.reduce(Point2.zero) { $0+($1.wrist-$1.body) } * (1/count)
        referenceWidth = calibrationSamples.reduce(0) { $0+$1.shoulderWidth }/count
        calibrated = true; isCalibrating = false; calibrationProgress = 1
        calibrationMessage = "Ready. Sidestep to move; sweep your hand to strike."
        calibrationSamples.removeAll(); resetMotion()
    }
}

enum SyntheticPose {
    static func sample(time: Double, start: Double, hand: String = "right") -> PoseSample {
        let t = max(0, time-start)
        let moving = max(0, t-1.5)
        let bodyX = 0.5+0.055*sin(moving*0.75)
        let phase = moving.truncatingRemainder(dividingBy: 2.2)
        // Slow preparation, a decisive lateral stroke, and an unhurried recovery.
        let stroke: Double
        if phase < 0.65 { stroke = -0.085*phase/0.65 }
        else if phase < 0.84 { stroke = -0.085+(phase-0.65)/0.19*0.25 }
        else if phase < 1.65 { stroke = 0.165*(1-(phase-0.84)/0.81) }
        else { stroke = 0 }
        let side = hand == "left" ? -1.0 : 1.0
        let wrist = Point2(x: bodyX+side*(0.13+(t > 1.5 ? stroke : 0)), y: 0.50+(t > 1.5 ? stroke*0.30 : 0))
        let body = Point2(x: bodyX, y: 0.49)
        let joints = ["neck": Point2(x: bodyX, y: 0.72), "nose": Point2(x: bodyX, y: 0.80),
                      "leftShoulder": Point2(x: bodyX-0.10, y: 0.68), "rightShoulder": Point2(x: bodyX+0.10, y: 0.68),
                      "leftElbow": Point2(x: bodyX-0.15, y: 0.54), "rightElbow": Point2(x: bodyX+0.15, y: 0.57),
                      "leftWrist": hand == "left" ? wrist : Point2(x: bodyX-0.10, y: 0.49),
                      "rightWrist": hand == "right" ? wrist : Point2(x: bodyX+0.10, y: 0.49),
                      "leftHip": Point2(x: bodyX-0.065, y: 0.40), "rightHip": Point2(x: bodyX+0.065, y: 0.40),
                      "leftKnee": Point2(x: bodyX-0.08, y: 0.24), "rightKnee": Point2(x: bodyX+0.08, y: 0.24),
                      "leftAnkle": Point2(x: bodyX-0.10, y: 0.09), "rightAnkle": Point2(x: bodyX+0.10, y: 0.09)]
        return PoseSample(timestamp: time, body: body, shoulderWidth: 0.2, wrist: wrist, confidence: 0.98, joints: joints)
    }
}
