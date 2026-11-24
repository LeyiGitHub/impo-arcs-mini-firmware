// Optional acoustic bench recorder. Build with swiftc; requires microphone access.
import AVFoundation
import Foundation

guard CommandLine.arguments.count == 2,
      AVCaptureDevice.authorizationStatus(for: .audio) == .authorized else {
    fputs("Usage: macos_voice_capture output.wav (microphone access required)\n", stderr)
    exit(1)
}
let url = URL(fileURLWithPath: CommandLine.arguments[1])
do {
    let recorder = try AVAudioRecorder(url: url, settings: [
        AVFormatIDKey: kAudioFormatLinearPCM, AVSampleRateKey: 48000,
        AVNumberOfChannelsKey: 1, AVLinearPCMBitDepthKey: 16,
        AVLinearPCMIsBigEndianKey: false, AVLinearPCMIsFloatKey: false
    ])
    guard recorder.prepareToRecord(), recorder.record() else { exit(2) }
    try FileManager.default.setAttributes([.posixPermissions: 0o600], ofItemAtPath: url.path)
    print("READY \(ProcessInfo.processInfo.systemUptime)")
    fflush(stdout)
    _ = readLine()
    recorder.stop()
} catch {
    fputs("Recorder failed: \(error.localizedDescription)\n", stderr)
    exit(3)
}
