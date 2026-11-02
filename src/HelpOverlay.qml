import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: overlay
    signal closeRequested()

    readonly property color mutedColor: "#8b8b96"
    readonly property var shortcuts: [
        ["Space", qsTr("Start or stop the receiver")],
        ["D", qsTr("Toggle the demo signal")],
        ["R", qsTr("Reload audio input devices")],
        ["?", qsTr("Show or hide this help")],
        ["Q", qsTr("Quit")],
        ["Esc", qsTr("Close this help")]
    ]

    Rectangle {
        anchors.fill: parent
        color: "#b3000000"

        MouseArea {
            anchors.fill: parent
            onClicked: overlay.closeRequested()
        }
    }

    Rectangle {
        id: dialog
        anchors.centerIn: parent
        width: Math.min(560, parent.width - 40)
        height: content.implicitHeight + 48
        radius: Math.min(8, theme.radius)
        color: "#17171b"
        border.width: 1
        border.color: "#3a3a44"

        MouseArea { anchors.fill: parent }

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 24
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("Keyboard shortcuts")
                    color: "white"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                }
                Button {
                    text: qsTr("Close")
                    onClicked: overlay.closeRequested()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#2b2b33"
            }

            Repeater {
                model: overlay.shortcuts
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 62
                        height: 24
                        radius: Math.min(4, theme.radius)
                        color: "#22222a"
                        border.width: 1
                        border.color: "#34343e"

                        Text {
                            anchors.centerIn: parent
                            text: modelData[0]
                            color: "white"
                            font.pixelSize: 11
                            font.family: "monospace"
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: modelData[1]
                        color: overlay.mutedColor
                        font.pixelSize: 12
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#2b2b33"
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("JS8 Normal decodes at each 15-second UTC boundary. Settings and the last selected input are remembered; decoded text is logged to ~/.local/share/js8/decodes.log.")
                color: overlay.mutedColor
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
    }
}
