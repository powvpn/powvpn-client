import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ManagedServiceController and ManagedConnectionAdapter are global QML context
// properties registered by CoreController (see client-overlay/INTEGRATION.md),
// the same way ConnectionController/ImportController are -- no manual property
// injection needed to open this page from anywhere in the app.
Page {
    id: root
    title: qsTr("Choose country")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: qsTr("Plan: %1 · Access: %2").arg(ManagedServiceController.subscriptionPlan).arg(ManagedServiceController.accessMode)
            font.pixelSize: 16
        }
        Label {
            visible: ManagedServiceController.trafficLimitBytes > 0
            text: qsTr("Traffic: %1 GB of %2 GB used")
                  .arg((ManagedServiceController.trafficUsedBytes / 1000000000).toFixed(2))
                  .arg((ManagedServiceController.trafficLimitBytes / 1000000000).toFixed(0))
            opacity: 0.75
        }
        Label {
            visible: !ManagedServiceController.accessAllowed
            text: qsTr("Traffic limit reached. Choose any paid plan to continue.")
            color: "#d34a4a"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Rectangle {
            visible: ManagedConnectionAdapter.stateText !== "disconnected"
            Layout.fillWidth: true
            Layout.preferredHeight: connectionStatusColumn.implicitHeight + 24
            radius: 10
            color: ManagedConnectionAdapter.connected ? "#1f7a3d22" : "#7a5a1f22"
            border.color: ManagedConnectionAdapter.connected ? "#1f7a3d" : "#7a5a1f"
            border.width: 1

            ColumnLayout {
                id: connectionStatusColumn
                anchors.fill: parent
                anchors.margins: 12
                spacing: 4

                Label {
                    text: qsTr("Status: %1").arg(ManagedConnectionAdapter.stateText)
                    font.bold: true
                }
                Label {
                    visible: ManagedConnectionAdapter.activeCity.length > 0
                    text: qsTr("Server: %1 · Protocol: %2")
                          .arg(ManagedConnectionAdapter.activeCity)
                          .arg(ManagedConnectionAdapter.activeProtocol)
                    opacity: 0.8
                }
                Button {
                    text: qsTr("Disconnect")
                    visible: ManagedConnectionAdapter.stateText !== "disconnected"
                    onClicked: ManagedConnectionAdapter.disconnectManaged()
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Choose a country. The service automatically selects the best available city and server.")
            wrapMode: Text.WordWrap
            opacity: 0.7
        }
        BusyIndicator {
            visible: ManagedServiceController.busy
            running: visible
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            visible: ManagedServiceController.lastError.length > 0
            text: ManagedServiceController.lastError
            color: "#d34a4a"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            visible: ManagedConnectionAdapter.stateText === "error"
            text: qsTr("Could not connect. Pick a country to retry, or Pow VPN will try a backup server automatically.")
            color: "#d34a4a"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: ManagedServiceController.countries
            spacing: 8
            delegate: Button {
                required property var modelData
                width: ListView.view.width
                enabled: modelData.available === true
                text: modelData.country_name + "   " + modelData.country_code
                      + (ManagedConnectionAdapter.connected && modelData.country_code === ManagedConnectionAdapter.activeCountryCode
                         ? qsTr("  (connected)") : "")
                // Selecting a country while already connected re-allocates and
                // reconnects; ManagedConnectionAdapter closes the previous
                // tunnel itself before opening the new one.
                onClicked: ManagedServiceController.selectCountry(modelData.country_code)
            }
        }
    }
}
