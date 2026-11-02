import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: panel
    color: "#17171b"
    radius: Math.min(6, theme.radius)
    border.width: 1
    border.color: "#2b2b33"

    readonly property color mutedColor: "#8b8b96"

    function formatInteger(value) {
        var language = translator.language
        return Number(value).toLocaleString(Qt.locale(), 'f', 0)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: qsTr("Decoded")
                color: "white"
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }

            Rectangle {
                visible: backend.messages.count > 0
                width: countLabel.implicitWidth + 12
                height: 18
                radius: Math.min(4, theme.radius)
                color: "#24242c"

                Text {
                    id: countLabel
                    anchors.centerIn: parent
                    text: backend.messages.count
                    color: panel.mutedColor
                    font.pixelSize: 10
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Clear")
                flat: true
                enabled: backend.messages.count > 0
                onClicked: backend.clearMessages()
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: list
                anchors.fill: parent
                clip: true
                model: backend.messages
                spacing: 1
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                delegate: Rectangle {
                    width: ListView.view.width
                    height: 30
                    color: index % 2 === 0 ? "#1b1b20" : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        spacing: 10

                        Text {
                            Layout.preferredWidth: 58
                            text: model.time
                            color: panel.mutedColor
                            font.pixelSize: 11
                            font.family: "monospace"
                        }

                        Text {
                            Layout.preferredWidth: 74
                            text: panel.formatInteger(model.frequency) + " Hz"
                            color: panel.mutedColor
                            font.pixelSize: 11
                            font.family: "monospace"
                        }

                        Text {
                            Layout.preferredWidth: 54
                            text: (model.snr >= 0 ? "+" : "") + model.snr + " dB"
                            color: model.snr > -12 ? theme.accent : panel.mutedColor
                            font.pixelSize: 11
                            font.family: "monospace"
                        }

                        Text {
                            Layout.preferredWidth: 76
                            text: model.submode
                            color: panel.mutedColor
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: model.lowConfidence ? "[" + model.message + "]" : model.message
                            color: model.lowConfidence ? panel.mutedColor : "#e8e8ec"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            visible: model.demo
                            width: demoTag.implicitWidth + 10
                            height: 16
                            radius: Math.min(3, theme.radius)
                            color: "transparent"
                            border.width: 1
                            border.color: theme.accent

                            Text {
                                id: demoTag
                                anchors.centerIn: parent
                                text: "DEMO"
                                color: theme.accent
                                font.pixelSize: 8
                                font.letterSpacing: 1
                            }
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: backend.messages.count === 0
                width: Math.min(parent.width - 40, 520)
                text: backend.monitoring
                    ? qsTr("Listening for JS8 traffic. Decodes arrive after each 15-second period.")
                    : qsTr("Receiver stopped.")
                color: panel.mutedColor
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }
    }
}
