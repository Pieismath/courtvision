import AppKit
import SwiftUI

final class AppDelegate: NSObject, NSApplicationDelegate {
    var window: NSWindow?
    let controller = CameraController()
    func applicationDidFinishLaunching(_ notification: Notification) {
        let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 1120, height: 760), styleMask: [.titled, .closable, .miniaturizable, .resizable], backing: .buffered, defer: false)
        window.title = "CourtVision Motion"
        window.titlebarAppearsTransparent = true
        window.backgroundColor = NSColor(calibratedRed: 0.055, green: 0.09, blue: 0.081, alpha: 1)
        window.contentView = NSHostingView(rootView: CompanionView(camera: controller))
        window.minSize = NSSize(width: 1060, height: 750)
        window.center(); window.makeKeyAndOrderFront(nil)
        self.window = window
        NSApp.activate(ignoringOtherApps: true)
        if CommandLine.arguments.contains("--ui-synthetic") { controller.startSynthetic() }
    }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
    func applicationWillTerminate(_ notification: Notification) { controller.stop() }
}

@main enum CourtVisionMain {
    static func main() {
        if CommandLine.arguments.contains("--synthetic") {
            let args = CommandLine.arguments
            let duration = args.firstIndex(of: "--seconds").flatMap { $0+1 < args.count ? Double(args[$0+1]) : nil } ?? 8
            let port = args.firstIndex(of: "--port").flatMap { $0+1 < args.count ? UInt16(args[$0+1]) : nil } ?? 7779
            let hand = args.firstIndex(of: "--hand").flatMap { $0+1 < args.count ? args[$0+1] : nil } ?? "right"
            let transport = UDPTransport(port: port)
            let engine = MotionEngine(); engine.hand = hand == "left" ? "left" : "right"
            engine.source = "synthetic"; engine.beginCalibration()
            let start = ProcessInfo.processInfo.systemUptime
            var packets = 0
            while ProcessInfo.processInfo.systemUptime-start < min(60, max(0.1, duration)) {
                let now = ProcessInfo.processInfo.systemUptime
                let packet = engine.update(SyntheticPose.sample(time: now, start: start, hand: engine.hand))
                transport.send(packet); packets += 1
                if packet.swing { print("Swing \(packet.swingId): power \(String(format: "%.2f", packet.speed)), aim \(String(format: "%.2f", packet.aimX)), spin \(String(format: "%.2f", packet.spin))") }
                Thread.sleep(forTimeInterval: 1/60)
            }
            transport.send(engine.lost(at: ProcessInfo.processInfo.systemUptime))
            print("Synthetic validation: \(packets) frames, \(engine.swingId) swings, UDP 127.0.0.1:\(port). No camera accessed.")
            return
        }
        let application = NSApplication.shared
        application.setActivationPolicy(.regular)
        let delegate = AppDelegate()
        application.delegate = delegate
        let mainMenu = NSMenu()
        let applicationItem = NSMenuItem()
        let applicationMenu = NSMenu()
        applicationMenu.addItem(withTitle: "Quit CourtVision Motion", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        applicationItem.submenu = applicationMenu; mainMenu.addItem(applicationItem)
        application.mainMenu = mainMenu
        withExtendedLifetime(delegate) { application.run() }
    }
}
