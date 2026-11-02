import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: bar
    color: "transparent"
    implicitHeight: 52

    readonly property color mutedColor: "#8b8b96"

    function formatNumber(value, decimals) {
        var language = translator.language
        return Number(value).toLocaleString(Qt.locale(), 'f', decimals)
    }

    RowLayout {
        anchors.fill: parent
        spacing: 14

        Rectangle {
            width: 34
            height: 34
            radius: Math.min(8, theme.radius)
            color: theme.accent

            Text {
                anchors.centerIn: parent
                text: "JS8"
                color: theme.accentForeground
                font.bold: true
                font.pixelSize: 12
            }
        }

        ColumnLayout {
            spacing: 0

            Text {
                text: "JS8"
                color: "white"
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }

            Text {
                text: qsTr("Weak-signal messaging · %1 MHz USB")
                      .arg(bar.formatNumber(backend.dialMhz, 3))
                color: bar.mutedColor
                font.pixelSize: 11
            }
        }

        Item { Layout.fillWidth: true }

        ColumnLayout {
            spacing: 2

            RowLayout {
                spacing: 6
                Rectangle {
                    width: 6
                    height: 6
                    radius: 3
                    color: backend.monitoring ? theme.accent : bar.mutedColor
                }
                Text {
                    text: backend.monitoring
                        ? (backend.demo ? qsTr("DEMO") : qsTr("LIVE"))
                        : qsTr("STOPPED")
                    color: backend.monitoring ? "white" : bar.mutedColor
                    font.pixelSize: 10
                    font.letterSpacing: 1
                }
            }

            Text {
                text: backend.utcClock + " UTC"
                color: bar.mutedColor
                font.family: "monospace"
                font.pixelSize: 11
            }
        }

        ColumnLayout {
            spacing: 3
            Layout.preferredWidth: 150
            Layout.maximumWidth: 150
            Layout.alignment: Qt.AlignVCenter

            Text {
                text: qsTr("15 s period")
                color: bar.mutedColor
                font.pixelSize: 10
            }

            ProgressBar {
                Layout.fillWidth: true
                from: 0
                to: 1
                value: backend.periodProgress
                indeterminate: false
            }
        }

        Button {
            text: backend.monitoring ? qsTr("Stop") : qsTr("Monitor")
            highlighted: !backend.monitoring
            onClicked: backend.toggleMonitor()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Space")
        }

        Button {
            text: qsTr("Demo")
            checkable: true
            checked: backend.demo
            onClicked: backend.setDemo(checked)
            ToolTip.visible: hovered
            ToolTip.text: qsTr("D")
        }
    }
}
