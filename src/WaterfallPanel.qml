import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Js8

Rectangle {
    id: panel
    color: "#17171b"
    radius: Math.min(6, theme.radius)
    border.width: 1
    border.color: "#2b2b33"
    clip: true

    // A spectrum is read left-to-right even in a right-to-left language.
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: false

    readonly property int maxHz: 3000
    readonly property color mutedColor: "#8b8b96"
    readonly property var ticks: [0, 500, 1000, 1500, 2000, 2500, 3000]

    function formatInteger(value) {
        var language = translator.language
        return Number(value).toLocaleString(Qt.locale(), 'f', 0)
    }

    WaterfallItem {
        id: waterfall
        anchors.fill: parent
        anchors.margins: 1
        source: backend
        accent: theme.accent
        background: "#0b0b0e"
    }

    Item {
        id: grid
        anchors.fill: waterfall
        Repeater {
            model: panel.ticks
            Rectangle {
                x: Math.round(index * grid.width / (panel.ticks.length - 1))
                width: 1
                height: grid.height
                color: "#26ffffff"
            }
        }
    }

    Rectangle {
        anchors.left: waterfall.left
        anchors.right: waterfall.right
        anchors.top: waterfall.top
        height: 1
        color: theme.accent
        opacity: 0.55
    }

    Item {
        id: labels
        anchors.left: waterfall.left
        anchors.right: waterfall.right
        anchors.bottom: waterfall.bottom
        anchors.bottomMargin: 4
        height: 14

        Repeater {
            model: panel.ticks
            Text {
                x: Math.max(0, Math.min(labels.width - width,
                        index * labels.width / (panel.ticks.length - 1) - width / 2))
                text: panel.formatInteger(modelData)
                color: panel.mutedColor
                font.pixelSize: 10
            }
        }
    }

    Text {
        anchors.right: waterfall.right
        anchors.top: waterfall.top
        anchors.margins: 8
        text: qsTr("Audio offset (Hz)")
        color: panel.mutedColor
        font.pixelSize: 10
    }

    Rectangle {
        visible: backend.demo
        anchors.left: waterfall.left
        anchors.top: waterfall.top
        anchors.margins: 8
        width: demoLabel.implicitWidth + 20
        height: 20
        radius: Math.min(4, theme.radius)
        color: "#cc000000"
        border.color: theme.accent

        Text {
            id: demoLabel
            anchors.centerIn: parent
            text: qsTr("DEMO SIGNAL")
            color: theme.accent
            font.pixelSize: 9
            font.letterSpacing: 1
        }
    }

    Rectangle {
        anchors.fill: waterfall
        visible: !backend.monitoring
        color: "#99000000"

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 6
            width: Math.min(parent.width - 40, 420)

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Receiver stopped")
                color: "white"
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Press Space to monitor the radio, or D to watch the demo signal.")
                color: panel.mutedColor
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }
    }
}
