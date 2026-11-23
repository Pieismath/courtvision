import Foundation
import Darwin

final class UDPTransport {
    private let descriptor: Int32
    private var address = sockaddr_in()
    private let encoder = JSONEncoder()
    let port: UInt16
    private(set) var packetsSent: UInt64 = 0
    private(set) var lastError: String?
    init(port: UInt16 = 7779) {
        self.port = port
        descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
        address.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        address.sin_family = sa_family_t(AF_INET)
        address.sin_port = port.bigEndian
        address.sin_addr.s_addr = inet_addr("127.0.0.1")
        if descriptor < 0 { lastError = String(cString: strerror(errno)) }
        if descriptor >= 0 {
            let flags = fcntl(descriptor, F_GETFL)
            _ = fcntl(descriptor, F_SETFL, flags | O_NONBLOCK)
        }
    }
    deinit { if descriptor >= 0 { close(descriptor) } }
    func send(_ packet: MotionPacket) {
        guard descriptor >= 0, let data = try? encoder.encode(packet) else { return }
        let result = data.withUnsafeBytes { bytes in
            withUnsafePointer(to: &address) { pointer in
                pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { destination in
                    sendto(descriptor, bytes.baseAddress, bytes.count, 0, destination, socklen_t(MemoryLayout<sockaddr_in>.size))
                }
            }
        }
        if result >= 0 { packetsSent &+= 1; lastError = nil }
        else { lastError = String(cString: strerror(errno)) }
    }
}
