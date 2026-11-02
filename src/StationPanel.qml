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
    readonly property bool editing: callsignField.activeFocus
                                        || gridField.activeFocus
                                        || dialField.activeFocus

    ScrollView {
        id: scroll
        anchors.fill: parent
        anchors.margins: 12
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 8

        Text {
            text: qsTr("Station")
            color: "white"
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }

        TextField {
            id: callsignField
            Layout.fillWidth: true
            placeholderText: qsTr("Callsign")
            text: backend.callsign
            selectByMouse: true
            onTextEdited: backend.setCallsign(text)
            onEditingFinished: text = backend.callsign
        }

        TextField {
            id: gridField
            Layout.fillWidth: true
            placeholderText: qsTr("Grid square")
            text: backend.grid
            selectByMouse: true
            onTextEdited: backend.setGrid(text)
            onEditingFinished: text = backend.grid
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: dialField
                Layout.fillWidth: true
                placeholderText: qsTr("Dial (MHz)")
                text: backend.dialMhz.toFixed(3)
                selectByMouse: true
                validator: DoubleValidator {
                    bottom: 0.1
                    top: 6000.0
                    decimals: 3
                    notation: DoubleValidator.StandardNotation
                }
                onEditingFinished: {
                    const value = Number.fromLocaleString(Qt.locale(), text)
                    if (!isNaN(value))
                        backend.setDialMhz(value)
                    text = backend.dialMhz.toFixed(3)
                }
            }

            Text {
                text: qsTr("MHz")
                color: panel.mutedColor
                font.pixelSize: 11
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 4
            height: 1
            color: "#2b2b33"
        }

        Text {
            text: qsTr("Input")
            color: panel.mutedColor
            font.pixelSize: 11
        }

        ComboBox {
            Layout.fillWidth: true
            model: backend.inputDevices
            currentIndex: backend.inputDeviceIndex
            enabled: backend.inputDevices.length > 0
            displayText: backend.inputDevices.length > 0 ? currentText : qsTr("No input devices")
            onActivated: backend.selectInputDevice(index)
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("Input level")
                    color: panel.mutedColor
                    font.pixelSize: 11
                    Layout.fillWidth: true
                }
                Text {
                    text: Math.round(backend.level * 100) + "%"
                    color: panel.mutedColor
                    font.pixelSize: 11
                    font.family: "monospace"
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 6
                radius: Math.min(3, theme.radius)
                color: "#0e0e12"

                Rectangle {
                    height: parent.height
                    width: Math.max(0, Math.min(parent.width, parent.width * backend.level))
                    radius: Math.min(3, theme.radius)
                    color: backend.level > 0.85 ? "#ff6b6b" : theme.accent

                    Behavior on width {
                        NumberAnimation { duration: 70 }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.topMargin: 2
            text: qsTr("JS8 Normal · next decode in %1 s").arg(backend.secondsToPeriod)
            color: panel.mutedColor
            font.pixelSize: 10
            wrapMode: Text.WordWrap
        }
        }
    }
}
