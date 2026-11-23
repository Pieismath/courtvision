import AppKit
import AVFoundation
import Vision
import SwiftUI

final class CameraController: NSObject, ObservableObject, AVCaptureVideoDataOutputSampleBufferDelegate {
    @Published var running = false
    @Published var synthetic = false
    @Published var busy = false
    @Published var status = "Your movement. Your game."
    @Published var detail = "Start your camera when you are ready. Everything stays on this Mac."
    @Published var deviceName = "Camera is off"
    @Published var packet = MotionPacket()
    @Published var joints: [String: Point2] = [:]
    @Published var calibrationProgress = 0.0
    @Published var isCalibrating = false
    @Published var hand = "right"
    @Published var framesPerSecond = 0.0
    @Published var processingMilliseconds = 0.0
    @Published var handDetailed = false
    @Published var transportError: String?
    let session = AVCaptureSession()
    private let queue = DispatchQueue(label: "com.courtvision.capture", qos: .userInteractive)
    private let engine = MotionEngine()
    private let transport = UDPTransport()
    private let bodyRequest = VNDetectHumanBodyPoseRequest()
    private let handRequest = VNDetectHumanHandPoseRequest()
    private var configured = false
    private var active = false
    private var syntheticActive = false
    private var timer: DispatchSourceTimer?
    private var simulationStart = 0.0
    private var previousFrameTime: Double?
    private var smoothedFPS = 0.0
    private var deviceLabel = "Camera"
    private var permissionGeneration = 0

    override init() {
        super.init()
        handRequest.maximumHandCount = 2
        let watchdog = DispatchSource.makeTimerSource(queue: queue)
        watchdog.schedule(deadline: .now(), repeating: .milliseconds(33))
        watchdog.setEventHandler { [weak self] in self?.tick() }
        watchdog.resume(); timer = watchdog
    }

    func startCamera() {
        guard !busy else { return }
        busy = true; permissionGeneration += 1
        let generation = permissionGeneration
        status = "Connecting camera…"
        switch AVCaptureDevice.authorizationStatus(for: .video) {
        case .authorized: activateCamera()
        case .notDetermined:
            AVCaptureDevice.requestAccess(for: .video) { [weak self] granted in
                DispatchQueue.main.async {
                    guard let self, self.permissionGeneration == generation else { return }
                    if granted { self.activateCamera() }
                    else { self.cameraDenied() }
                }
            }
        default: cameraDenied()
        }
    }

    private func cameraDenied() {
        busy = false; status = "Camera access is off"
        detail = "Enable CourtVision Motion in System Settings → Privacy & Security → Camera, then start again."
    }

    private func activateCamera() {
        queue.async {
            self.syntheticActive = false
            self.previousFrameTime = nil; self.smoothedFPS = 0
            self.engine.source = "camera"
            self.engine.clearCalibration()
            do {
                if !self.configured { try self.configureCamera() }
                self.session.startRunning()
                self.active = self.session.isRunning
                DispatchQueue.main.async {
                    self.busy = false; self.running = self.active; self.synthetic = false
                    self.deviceName = self.deviceLabel
                    self.status = self.active ? "Find your ready stance" : "Camera could not start"
                    self.detail = self.active ? "Step back until your shoulders, hips and racket hand are visible." : "Another app may be using the camera. Close it and try again."
                }
            } catch {
                self.active = false
                DispatchQueue.main.async {
                    self.busy = false; self.running = false; self.synthetic = false
                    self.status = "Camera unavailable"; self.detail = error.localizedDescription
                }
            }
        }
    }

    private func configureCamera() throws {
        guard let device = AVCaptureDevice.default(for: .video) else {
            throw NSError(domain: "CourtVision", code: 1, userInfo: [NSLocalizedDescriptionKey: "Connect a webcam, then try again. You can also preview the controls with a simulated swing."])
        }
        session.beginConfiguration()
        var configurationComplete = false
        defer {
            if !configurationComplete {
                session.inputs.forEach { session.removeInput($0) }
                session.outputs.forEach { session.removeOutput($0) }
            }
            session.commitConfiguration()
        }
        if session.canSetSessionPreset(.vga640x480) { session.sessionPreset = .vga640x480 }
        let input = try AVCaptureDeviceInput(device: device)
        guard session.canAddInput(input) else {
            throw NSError(domain: "CourtVision", code: 2, userInfo: [NSLocalizedDescriptionKey: "This camera input is not supported."])
        }
        session.addInput(input)
        let output = AVCaptureVideoDataOutput()
        output.alwaysDiscardsLateVideoFrames = true
        output.videoSettings = [kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_420YpCbCr8BiPlanarFullRange]
        output.setSampleBufferDelegate(self, queue: queue)
        guard session.canAddOutput(output) else {
            session.removeInput(input)
            throw NSError(domain: "CourtVision", code: 3, userInfo: [NSLocalizedDescriptionKey: "Could not configure the camera frame stream."])
        }
        session.addOutput(output)
        if let connection = output.connection(with: .video), connection.isVideoMirroringSupported {
            connection.automaticallyAdjustsVideoMirroring = false
            connection.isVideoMirrored = false
        }
        // Request the highest supported rate up to 60. The UI reports measured inference FPS.
        try device.lockForConfiguration()
        let maximum = device.activeFormat.videoSupportedFrameRateRanges.map(\.maxFrameRate).max() ?? 30
        let fps = min(60, maximum)
        if let range = device.activeFormat.videoSupportedFrameRateRanges.first(where: { $0.minFrameRate <= fps && $0.maxFrameRate >= fps }) {
            _ = range
            let duration = CMTime(value: 1, timescale: CMTimeScale(fps.rounded()))
            device.activeVideoMinFrameDuration = duration; device.activeVideoMaxFrameDuration = duration
        }
        device.unlockForConfiguration()
        deviceLabel = device.localizedName
        configured = true; configurationComplete = true
    }

    func stop() {
        permissionGeneration += 1; busy = false
        queue.async {
            self.active = false; self.syntheticActive = false
            if self.session.isRunning { self.session.stopRunning() }
            self.engine.clearCalibration()
            self.publish(self.engine.lost(at: ProcessInfo.processInfo.systemUptime), joints: [:], ms: 0)
            DispatchQueue.main.async {
                self.running = false; self.synthetic = false; self.deviceName = "Camera is off"
                self.status = "Your movement. Your game."
                self.detail = "Start your camera when you are ready. Everything stays on this Mac."
                self.framesPerSecond = 0
            }
        }
    }

    func startSynthetic() {
        permissionGeneration += 1; busy = true
        queue.async {
            if self.session.isRunning { self.session.stopRunning() }
            self.engine.source = "synthetic"; self.engine.beginCalibration()
            self.active = true; self.syntheticActive = true
            self.simulationStart = ProcessInfo.processInfo.systemUptime
            self.previousFrameTime = nil
            self.smoothedFPS = 0
            DispatchQueue.main.async {
                self.busy = false; self.running = true; self.synthetic = true
                self.deviceName = "Simulated motion • no camera"
            }
        }
    }

    func calibrate() { queue.async { self.engine.beginCalibration() } }
    func chooseHand(_ value: String) {
        hand = value
        queue.async { self.engine.hand = value }
    }

    private func tick() {
        let now = ProcessInfo.processInfo.systemUptime
        if syntheticActive {
            let pose = SyntheticPose.sample(time: now, start: simulationStart, hand: engine.hand)
            measureFPS(now)
            publish(engine.update(pose), joints: pose.joints, ms: 0)
        } else if active, let packet = engine.watchdog(at: now) {
            publish(packet, joints: [:], ms: 0)
        }
    }

    func captureOutput(_ output: AVCaptureOutput, didOutput sampleBuffer: CMSampleBuffer, from connection: AVCaptureConnection) {
        guard active && !syntheticActive else { return }
        let began = ProcessInfo.processInfo.systemUptime
        guard let pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer) else { return }
        do {
            let handler = VNImageRequestHandler(cvPixelBuffer: pixelBuffer, orientation: .up, options: [:])
            try handler.perform([bodyRequest, handRequest])
            let completed = ProcessInfo.processInfo.systemUptime
            let milliseconds = (completed-began)*1000
            measureFPS(completed)
            // Do not publish old inference as live control if the inference engine stalls.
            guard completed-began < 0.20, let sample = try extractPose(timestamp: began) else {
                publish(engine.lost(at: completed), joints: [:], ms: milliseconds)
                return
            }
            let packet = engine.update(sample)
            publish(packet, joints: sample.joints, ms: milliseconds, detailed: sample.handDetailed)
        } catch {
            publish(engine.lost(at: began), joints: [:], ms: 0)
        }
    }

    private func extractPose(timestamp: Double) throws -> PoseSample? {
        // Prefer the largest visible torso, making distant spectators less likely to take over.
        var candidates: [(VNHumanBodyPoseObservation, Double)] = []
        for body in bodyRequest.results ?? [] {
            let left = try body.recognizedPoint(.leftShoulder)
            let right = try body.recognizedPoint(.rightShoulder)
            guard min(left.confidence, right.confidence) > 0.35 else { continue }
            candidates.append((body, hypot(left.location.x-right.location.x, left.location.y-right.location.y)))
        }
        guard let observation = candidates.max(by: { $0.1 < $1.1 })?.0 else { return nil }
        let points = try observation.recognizedPoints(.all)
        func point(_ key: VNHumanBodyPoseObservation.JointName) -> Point2? {
            guard let value = points[key], value.confidence >= 0.35 else { return nil }
            return Point2(x: 1-value.location.x, y: value.location.y)
        }
        guard let left = point(.leftShoulder), let right = point(.rightShoulder),
              let wristBody = point(engine.hand == "right" ? .rightWrist : .leftWrist) else { return nil }
        let shoulders = (left+right)*0.5
        let hips: Point2
        if let leftHip = point(.leftHip), let rightHip = point(.rightHip) { hips = (leftHip+rightHip)*0.5 }
        else if let root = point(.root) { hips = root }
        else { return nil }
        let width = (right-left).length
        let center = (shoulders+hips)*0.5
        var wrist = wristBody
        var detailed = false
        var closest = min(0.15, width*0.7)
        for hand in handRequest.results ?? [] {
            let recognized = try hand.recognizedPoint(.wrist)
            guard recognized.confidence >= 0.45 else { continue }
            let handWrist = Point2(x: 1-recognized.location.x, y: recognized.location.y)
            let distance = (handWrist-wristBody).length
            if distance < closest { wrist = handWrist; closest = distance; detailed = true }
        }
        let keys: [(String, VNHumanBodyPoseObservation.JointName)] = [
            ("nose", .nose), ("neck", .neck), ("leftShoulder", .leftShoulder), ("rightShoulder", .rightShoulder),
            ("leftElbow", .leftElbow), ("rightElbow", .rightElbow), ("leftWrist", .leftWrist), ("rightWrist", .rightWrist),
            ("leftHip", .leftHip), ("rightHip", .rightHip), ("leftKnee", .leftKnee), ("rightKnee", .rightKnee),
            ("leftAnkle", .leftAnkle), ("rightAnkle", .rightAnkle)]
        var joints: [String: Point2] = [:]
        for (name, key) in keys { if let value = point(key) { joints[name] = value } }
        joints[engine.hand == "right" ? "rightWrist" : "leftWrist"] = wrist
        let confidenceKeys: [VNHumanBodyPoseObservation.JointName] = [.leftShoulder, .rightShoulder, engine.hand == "right" ? .rightWrist : .leftWrist]
        let confidence = confidenceKeys.compactMap { points[$0]?.confidence }.min() ?? 0
        return PoseSample(timestamp: timestamp, body: center, shoulderWidth: width, wrist: wrist, confidence: Double(confidence), joints: joints, handDetailed: detailed)
    }

    private func measureFPS(_ time: Double) {
        if let previousFrameTime, time > previousFrameTime {
            let fps = min(120, 1/(time-previousFrameTime))
            smoothedFPS = smoothedFPS == 0 ? fps : smoothedFPS*0.88+fps*0.12
        }
        previousFrameTime = time
    }

    private func publish(_ incoming: MotionPacket, joints: [String: Point2], ms: Double, detailed: Bool = false) {
        var packet = incoming
        packet.inferenceMs = ms
        transport.send(packet)
        let progress = engine.calibrationProgress
        let calibrating = engine.isCalibrating
        let message = engine.calibrationMessage
        let fps = smoothedFPS
        let error = transport.lastError
        DispatchQueue.main.async {
            self.packet = packet; self.joints = joints
            self.calibrationProgress = progress; self.isCalibrating = calibrating
            self.processingMilliseconds = ms; self.framesPerSecond = fps
            self.handDetailed = detailed; self.transportError = error
            if self.running {
                if !packet.tracking {
                    self.status = "Step into the frame"
                    self.detail = "Keep your shoulders, hips and selected hand visible. Controls are paused."
                } else if calibrating { self.status = "Finding your center…"; self.detail = message }
                else if packet.calibrated { self.status = "You're ready to rally"; self.detail = message }
                else { self.status = "Make yourself at home"; self.detail = message }
            }
        }
    }
}
