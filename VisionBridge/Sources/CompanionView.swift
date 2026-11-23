import AppKit
import SwiftUI
import AVFoundation

private let ink = Color(red: 0.055, green: 0.090, blue: 0.081)
private let lime = Color(red: 0.83, green: 0.96, blue: 0.38)
private let muted = Color(red: 0.56, green: 0.63, blue: 0.58)

struct CompanionView: View {
    @ObservedObject var camera: CameraController
    var body: some View {
        VStack(alignment: .leading, spacing: 25) {
            header
            HStack(alignment: .top, spacing: 28) {
                VStack(alignment: .leading, spacing: 18) {
                    preview
                    HStack(spacing: 0) {
                        metric("TRACKING RATE", camera.running ? String(format: "%.0f", camera.framesPerSecond) : "—", "fps")
                        Divider().overlay(Color.white.opacity(0.1)).frame(height: 30)
                        metric("POSE PROCESSING", camera.running ? String(format: "%.0f", camera.processingMilliseconds) : "—", "ms")
                        Divider().overlay(Color.white.opacity(0.1)).frame(height: 30)
                        metric("TRACK CONFIDENCE", camera.packet.tracking ? String(format: "%.0f", camera.packet.confidence*100) : "—", "%")
                    }
                    .padding(.vertical, 17).background(Color.white.opacity(0.035)).clipShape(RoundedRectangle(cornerRadius: 12))
                    HStack(spacing: 7) {
                        Image(systemName: "lock.shield").foregroundStyle(lime)
                        Text("Private by design. Video is processed on this Mac and is never saved.")
                            .font(.system(size: 11)).foregroundStyle(muted)
                    }
                }
                .frame(maxWidth: .infinity)
                controls.frame(width: 298)
            }
            Spacer(minLength: 0)
            HStack {
                Text("MOTION BRIDGE  /  01").font(.system(size: 10, weight: .medium, design: .monospaced)).tracking(1.6)
                Spacer()
                Circle().fill(camera.transportError == nil ? lime.opacity(0.8) : Color.orange).frame(width: 5, height: 5)
                Text(camera.transportError == nil ? "LOCAL OUTPUT  127.0.0.1:7779" : "OUTPUT ERROR  \(camera.transportError!)")
                    .font(.system(size: 10, design: .monospaced)).tracking(0.7)
            }.foregroundStyle(muted)
        }
        .padding(32).frame(minWidth: 1060, minHeight: 720)
        .background(ink).preferredColorScheme(.dark)
    }

    private var header: some View {
        HStack(alignment: .center) {
            HStack(spacing: 11) {
                ZStack {
                    RoundedRectangle(cornerRadius: 11).fill(lime).frame(width: 39, height: 39)
                    Image(systemName: "tennis.racket").font(.system(size: 24, weight: .medium)).foregroundStyle(ink)
                }
                Text("COURTVISION").font(.system(size: 19, weight: .semibold)).tracking(3)
            }
            Spacer()
            Text("THE GAME BEGINS WITH YOU").font(.system(size: 10, weight: .medium)).tracking(2.1).foregroundStyle(muted)
            Rectangle().fill(Color.white.opacity(0.14)).frame(width: 1, height: 22).padding(.horizontal, 13)
            HStack(spacing: 7) {
                Circle().fill(camera.packet.tracking ? lime : muted).frame(width: 6, height: 6)
                Text(camera.synthetic ? "SIMULATION" : camera.running ? "CAMERA LIVE" : "CAMERA OFF")
                    .font(.system(size: 10, weight: .semibold)).tracking(1.2)
            }.foregroundStyle(camera.packet.tracking ? lime : muted)
        }
        .padding(.bottom, 8)
    }

    private var preview: some View {
        ZStack {
            RoundedRectangle(cornerRadius: 16).fill(Color(red: 0.075, green: 0.12, blue: 0.108))
            if camera.running && !camera.synthetic {
                CameraPreview(session: camera.session)
            } else {
                GeometryReader { geometry in
                    Path { path in
                        let w = geometry.size.width, h = geometry.size.height
                        path.move(to: CGPoint(x: w*0.17, y: h))
                        path.addLine(to: CGPoint(x: w*0.35, y: h*0.6))
                        path.addLine(to: CGPoint(x: w*0.65, y: h*0.6))
                        path.addLine(to: CGPoint(x: w*0.83, y: h))
                        path.move(to: CGPoint(x: w*0.28, y: h*0.77))
                        path.addLine(to: CGPoint(x: w*0.72, y: h*0.77))
                        path.move(to: CGPoint(x: w*0.5, y: h*0.77))
                        path.addLine(to: CGPoint(x: w*0.5, y: h))
                    }.stroke(lime.opacity(0.12), lineWidth: 1.1)
                }
                if !camera.running {
                    VStack(spacing: 18) {
                        ZStack {
                            Circle().stroke(lime.opacity(0.10), lineWidth: 1).frame(width: 112, height: 112)
                            Circle().fill(lime.opacity(0.06)).frame(width: 86, height: 86)
                            Image(systemName: "figure.tennis").font(.system(size: 43, weight: .ultraLight)).foregroundStyle(lime)
                        }
                        Text("Step onto the court.").font(.system(size: 29, weight: .medium, design: .serif))
                        Text("Your body moves the player.\nYour hand controls the racket.")
                            .font(.system(size: 13)).lineSpacing(5).multilineTextAlignment(.center).foregroundStyle(muted)
                    }.offset(y: -18)
                }
            }
            PoseOverlay(joints: camera.joints, hand: camera.hand, flash: camera.packet.swing)
            VStack {
                HStack {
                    Label(camera.synthetic ? "SIMULATED INPUT" : "MIRRORED PLAYER VIEW", systemImage: camera.synthetic ? "waveform.path" : "viewfinder")
                        .font(.system(size: 9, weight: .semibold)).tracking(1.4)
                        .padding(.horizontal, 11).padding(.vertical, 8)
                        .background(.black.opacity(0.35)).clipShape(Capsule())
                    Spacer()
                    if camera.running {
                        Text(camera.packet.tracking ? "●  POSE LOCKED" : "○  FINDING PLAYER")
                            .font(.system(size: 9, weight: .semibold)).tracking(1)
                            .foregroundStyle(camera.packet.tracking ? lime : Color.white)
                            .padding(.horizontal, 11).padding(.vertical, 8)
                            .background(.black.opacity(0.35)).clipShape(Capsule())
                    }
                }
                Spacer()
                if camera.running {
                    HStack {
                        Text(camera.deviceName).font(.system(size: 10)).foregroundStyle(Color.white.opacity(0.8))
                        Spacer()
                        Text(camera.handDetailed ? "BODY + HAND" : "BODY POSE")
                            .font(.system(size: 9, weight: .medium, design: .monospaced)).foregroundStyle(lime)
                    }.padding(10).background(.black.opacity(0.3)).clipShape(RoundedRectangle(cornerRadius: 8))
                }
            }.padding(16)
        }
        .aspectRatio(4/3, contentMode: .fit)
        .clipShape(RoundedRectangle(cornerRadius: 16))
        .overlay(RoundedRectangle(cornerRadius: 16).stroke(Color.white.opacity(0.06), lineWidth: 1))
    }

    private var controls: some View {
        VStack(alignment: .leading, spacing: 20) {
            VStack(alignment: .leading, spacing: 11) {
                Text("PLAY WITH INSTINCT").font(.system(size: 10, weight: .medium)).tracking(2).foregroundStyle(lime)
                Text(camera.status).font(.system(size: 33, weight: .medium, design: .serif)).lineSpacing(0).fixedSize(horizontal: false, vertical: true)
                Text(camera.detail).font(.system(size: 12)).lineSpacing(5).foregroundStyle(muted).frame(minHeight: 51, alignment: .topLeading)
            }
            Button(action: { camera.running ? camera.stop() : camera.startCamera() }) {
                HStack {
                    Image(systemName: camera.running ? "stop.fill" : "video")
                    Text(camera.busy ? "Connecting…" : camera.running ? "Stop session" : "Start camera").fontWeight(.semibold)
                    Spacer()
                    Image(systemName: "arrow.up.right")
                }.font(.system(size: 13)).padding(.horizontal, 17).frame(height: 46)
                .background(lime).foregroundStyle(ink).clipShape(RoundedRectangle(cornerRadius: 9))
            }.buttonStyle(.plain).disabled(camera.busy)
            VStack(alignment: .leading, spacing: 11) {
                Text("RACKET HAND").font(.system(size: 9, weight: .medium)).tracking(1.5).foregroundStyle(muted)
                HStack(spacing: 5) {
                    handButton("left", title: "Left hand")
                    handButton("right", title: "Right hand")
                }.padding(4).background(Color.white.opacity(0.045)).clipShape(RoundedRectangle(cornerRadius: 9))
            }
            Button(action: { camera.calibrate() }) {
                HStack {
                    Image(systemName: camera.packet.calibrated ? "checkmark.viewfinder" : "scope")
                    Text(camera.isCalibrating ? "Calibrating…" : camera.packet.calibrated ? "Recalibrate stance" : "Calibrate ready stance")
                    Spacer()
                }.font(.system(size: 12, weight: .medium)).padding(14)
                    .background(Color.white.opacity(0.035)).clipShape(RoundedRectangle(cornerRadius: 9))
                    .overlay(RoundedRectangle(cornerRadius: 9).stroke(Color.white.opacity(0.12), lineWidth: 1))
            }.buttonStyle(.plain).disabled(!camera.running || camera.isCalibrating)
            if camera.isCalibrating { ProgressView(value: camera.calibrationProgress).tint(lime) }
            Divider().overlay(Color.white.opacity(0.05))
            VStack(alignment: .leading, spacing: 15) {
                instruction("01", "Find some room", "Stand 2–3 m back, with space to swing.")
                instruction("02", "Set your stance", "Face the camera and hold your hand ready.")
                instruction("03", "Make your move", "Sidestep to run. Sweep to hit. Lift for topspin.")
            }
            HStack {
                Text("SWINGS").font(.system(size: 9, weight: .medium)).tracking(1.5).foregroundStyle(muted)
                Spacer()
                Text(String(format: "%03d", camera.packet.swingId)).font(.system(size: 23, weight: .light, design: .monospaced)).foregroundStyle(lime)
            }
            Button(action: { camera.startSynthetic() }) {
                HStack(spacing: 7) {
                    Image(systemName: "play.circle")
                    Text("Try simulated motion")
                    Spacer()
                    Image(systemName: "arrow.right").font(.system(size: 10))
                }.font(.system(size: 11)).foregroundStyle(muted)
            }.buttonStyle(.plain).disabled(camera.busy)
        }
    }

    private func handButton(_ hand: String, title: String) -> some View {
        Button(action: { camera.chooseHand(hand) }) {
            Text(title).font(.system(size: 11, weight: camera.hand == hand ? .semibold : .regular))
                .frame(maxWidth: .infinity).padding(.vertical, 9)
                .background(camera.hand == hand ? Color.white.opacity(0.12) : .clear)
                .foregroundStyle(camera.hand == hand ? Color.white : muted)
                .clipShape(RoundedRectangle(cornerRadius: 6))
        }.buttonStyle(.plain)
    }
    private func metric(_ title: String, _ value: String, _ unit: String) -> some View {
        VStack(alignment: .leading, spacing: 7) {
            Text(title).font(.system(size: 8, weight: .medium)).tracking(1.3).foregroundStyle(muted)
            HStack(alignment: .firstTextBaseline, spacing: 5) {
                Text(value).font(.system(size: 24, weight: .light, design: .monospaced))
                Text(unit).font(.system(size: 10)).foregroundStyle(muted)
            }
        }.frame(maxWidth: .infinity, alignment: .leading).padding(.leading, 18)
    }
    private func instruction(_ number: String, _ title: String, _ subtitle: String) -> some View {
        HStack(alignment: .top, spacing: 12) {
            Text(number).font(.system(size: 10, design: .monospaced)).foregroundStyle(lime.opacity(0.75)).padding(.top, 2)
            VStack(alignment: .leading, spacing: 4) {
                Text(title).font(.system(size: 11, weight: .medium))
                Text(subtitle).font(.system(size: 10)).foregroundStyle(muted).lineSpacing(3)
            }
        }
    }
}

struct PoseOverlay: View {
    var joints: [String: Point2]
    var hand: String
    var flash: Bool
    private let bones = [("nose", "neck"), ("neck", "leftShoulder"), ("neck", "rightShoulder"),
        ("leftShoulder", "leftElbow"), ("leftElbow", "leftWrist"), ("rightShoulder", "rightElbow"), ("rightElbow", "rightWrist"),
        ("leftShoulder", "leftHip"), ("rightShoulder", "rightHip"), ("leftHip", "rightHip"),
        ("leftHip", "leftKnee"), ("rightHip", "rightKnee"), ("leftKnee", "leftAnkle"), ("rightKnee", "rightAnkle")]
    var body: some View {
        Canvas { context, size in
            func map(_ p: Point2) -> CGPoint { CGPoint(x: p.x*size.width, y: (1-p.y)*size.height) }
            var path = Path()
            for (a,b) in bones {
                if let p = joints[a], let q = joints[b] { path.move(to: map(p)); path.addLine(to: map(q)) }
            }
            context.stroke(path, with: .color(lime.opacity(0.72)), style: StrokeStyle(lineWidth: 2, lineCap: .round))
            for (_, p) in joints {
                let position = map(p)
                context.fill(Path(ellipseIn: CGRect(x: position.x-3, y: position.y-3, width: 6, height: 6)), with: .color(lime))
            }
            if let wrist = joints[hand == "right" ? "rightWrist" : "leftWrist"] {
                let p = map(wrist)
                let radius = flash ? 26.0 : 16.0
                context.stroke(Path(ellipseIn: CGRect(x: p.x-radius, y: p.y-radius, width: radius*2, height: radius*2)), with: .color(flash ? .white : lime), lineWidth: 2)
            }
        }.allowsHitTesting(false)
    }
}

final class PreviewView: NSView {
    let previewLayer = AVCaptureVideoPreviewLayer()
    override init(frame: NSRect) {
        super.init(frame: frame)
        wantsLayer = true; layer = CALayer()
        previewLayer.videoGravity = .resizeAspectFill
        layer?.addSublayer(previewLayer)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
    override func layout() {
        super.layout(); previewLayer.frame = bounds
        if let connection = previewLayer.connection, connection.isVideoMirroringSupported {
            connection.automaticallyAdjustsVideoMirroring = false; connection.isVideoMirrored = true
        }
    }
}

struct CameraPreview: NSViewRepresentable {
    let session: AVCaptureSession
    func makeNSView(context: Context) -> PreviewView {
        let view = PreviewView(frame: .zero); view.previewLayer.session = session; return view
    }
    func updateNSView(_ nsView: PreviewView, context: Context) { nsView.needsLayout = true }
}
