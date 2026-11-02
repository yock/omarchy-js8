import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1180
    height: 760
    minimumWidth: 900
    minimumHeight: 620
    visible: true
    title: qsTr("JS8")
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius
    Material.background: "#17171b"
    Material.foreground: "#e8e8ec"

    property color textColor: "#e8e8ec"
    property color mutedColor: "#8b8b96"
    property color panelColor: "#17171b"
    property color borderColor: "#2b2b33"
    property bool helpVisible: false
    property bool editing: stationPanel.editing

    function formatNumber(value, decimals) {
        var language = translator.language
        return Number(value).toLocaleString(Qt.locale(), 'f', decimals)
    }

    function formatInteger(value) {
        return win.formatNumber(value, 0)
    }

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
        enabled: !win.editing && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.toggleMonitor()
    }
    Shortcut {
        sequence: "D"
        enabled: !win.editing && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.setDemo(!backend.demo)
    }
    Shortcut {
        sequence: "R"
        enabled: !win.editing && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.refreshDevices()
    }
    Shortcut {
        sequence: "Q"
        enabled: !win.editing && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: win.close()
    }
    Shortcut {
        sequences: [StandardKey.Quit]
        context: Qt.ApplicationShortcut
        onActivated: win.close()
    }

    onClosing: backend.saveSettings()

    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            TopBar {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                WaterfallPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                StationPanel {
                    id: stationPanel
                    Layout.preferredWidth: 304
                    Layout.fillHeight: true
                }
            }

            MessagesPanel {
                Layout.fillWidth: true
                Layout.preferredHeight: 190
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    width: 6
                    height: 6
                    radius: 3
                    color: backend.error ? "#ff6b6b"
                         : backend.monitoring ? theme.accent
                         : win.mutedColor
                }

                Text {
                    Layout.fillWidth: true
                    text: backend.status
                    color: backend.error ? "#ff8a8a" : win.mutedColor
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }

                Text {
                    text: qsTr("Data in ~/.config/js8")
                    color: win.mutedColor
                    font.pixelSize: 11
                }

                MouseArea {
                    width: shortcutHint.implicitWidth
                    height: shortcutHint.implicitHeight
                    cursorShape: Qt.PointingHandCursor
                    onClicked: win.helpVisible = true

                    Text {
                        id: shortcutHint
                        text: qsTr("? shortcuts")
                        color: win.mutedColor
                        font.pixelSize: 11
                    }
                }
            }
        }

        HelpOverlay {
            anchors.fill: parent
            visible: win.helpVisible
            onCloseRequested: win.helpVisible = false
        }
    }
}
