import AppKit
import CoreServices

func shellQuote(_ value: String) -> String { "'" + value.replacingOccurrences(of: "'", with: "'\\''") + "'" }
if CommandLine.arguments.contains("--register") {
    let result = LSSetDefaultRoleHandlerForContentType("dev.tappyweb.source" as CFString, .all, "dev.tappyweb.launcher" as CFString)
    exit(result == noErr ? 0 : 1)
}
final class Launcher: NSObject, NSApplicationDelegate {
    func application(_ sender: NSApplication, openFiles filenames: [String]) {
        do {
            let folder = URL(fileURLWithPath: NSTemporaryDirectory()).appendingPathComponent("Tappyweb-Launchers", isDirectory: true)
            try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
            for name in filenames {
                let file = URL(fileURLWithPath: name).standardizedFileURL
                guard file.pathExtension.lowercased() == "tappy" else { continue }
                let command = folder.appendingPathComponent(UUID().uuidString + ".command")
                let script = "#!/bin/zsh\nexec /usr/local/bin/tappy " + shellQuote(file.path) + "\n"
                try script.write(to: command, atomically: true, encoding: .utf8)
                try FileManager.default.setAttributes([.posixPermissions: 0o700], ofItemAtPath: command.path)
                NSWorkspace.shared.open([command], withApplicationAt: URL(fileURLWithPath: "/System/Applications/Utilities/Terminal.app"), configuration: NSWorkspace.OpenConfiguration()) { _, error in
                    if let error { NSLog("Tappyweb: %@", error.localizedDescription) }
                    NSApp.terminate(nil)
                }
            }
            sender.reply(toOpenOrPrint: .success)
        } catch {
            sender.reply(toOpenOrPrint: .failure)
            let alert = NSAlert(error: error); alert.runModal(); NSApp.terminate(nil)
        }
    }
}
let application = NSApplication.shared
application.setActivationPolicy(.accessory)
let delegate = Launcher(); application.delegate = delegate
application.run()
