import QtQuick
import QtQuick.Window
import Js8

Window {
    id: win
    width: 1180
    height: 760
    minimumWidth: 800
    minimumHeight: 520
    visible: true
    visibility: Window.FullScreen
    color: "#05070b"
    title: qsTr("JS8")

    property bool helpVisible: false

    Item {
        anchors.fill: parent

        Shortcut {
            sequence: "?"
            context: Qt.ApplicationShortcut
            onActivated: win.helpVisible = !win.helpVisible
        }
        Shortcut {
            sequence: "Escape"
            enabled: win.helpVisible
            context: Qt.ApplicationShortcut
            onActivated: win.helpVisible = false
        }
        Shortcut {
            sequence: "Space"
            context: Qt.ApplicationShortcut
            onActivated: backend.toggleMonitor()
        }
        Shortcut {
            sequence: "D"
            context: Qt.ApplicationShortcut
            onActivated: backend.setDemo(!backend.demo)
        }
        Shortcut {
            sequence: "I"
            context: Qt.ApplicationShortcut
            onActivated: backend.cycleInputDevice()
        }
        Shortcut {
            sequence: "R"
            context: Qt.ApplicationShortcut
            onActivated: backend.refreshDevices()
        }
        Shortcut {
            sequence: "Q"
            context: Qt.ApplicationShortcut
            onActivated: win.close()
        }
        Shortcut {
            sequences: [StandardKey.Quit]
            context: Qt.ApplicationShortcut
            onActivated: win.close()
        }

        TacticalDisplay {
            anchors.fill: parent
            source: backend
            accent: theme.accent
            background: "#05070b"
            helpVisible: win.helpVisible
            status: backend.status
            error: backend.error
            monitoring: backend.monitoring
            demo: backend.demo
            level: backend.level
            callsign: backend.callsign
            grid: backend.grid
            dialMhz: backend.dialMhz
            utcClock: backend.utcClock
            secondsToPeriod: backend.secondsToPeriod
            periodProgress: backend.periodProgress
            inputDeviceName: backend.inputDeviceName
            ticker: backend.ticker
        }
    }

    onClosing: backend.saveSettings()
}
