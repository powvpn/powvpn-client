import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Style 1.0

import "../Controls2"

Dialog {
    id: root
    title: qsTr("Sign in to Pow VPN")
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: Math.min(420, (parent ? parent.width : 452) - 32)
    padding: 20

    function openForSignIn() {
        open()
        Qt.callLater(function() { emailField.forceActiveFocus() })
    }

    function submit() {
        ManagedServiceController.signIn(emailField.text, passwordField.text)
    }

    contentItem: ColumnLayout {
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: qsTr("Use your Pow VPN account to sync locations and connect.")
            wrapMode: Text.WordWrap
            opacity: 0.8
        }

        TextField {
            id: emailField
            Layout.fillWidth: true
            placeholderText: qsTr("Email")
            inputMethodHints: Qt.ImhEmailCharactersOnly
            selectByMouse: true
            onAccepted: passwordField.forceActiveFocus()
        }

        TextField {
            id: passwordField
            Layout.fillWidth: true
            placeholderText: qsTr("Password")
            echoMode: TextInput.Password
            selectByMouse: true
            onAccepted: root.submit()
        }

        Label {
            Layout.fillWidth: true
            visible: ManagedServiceController.lastError.length > 0
            text: ManagedServiceController.lastError
            color: "#d34a4a"
            wrapMode: Text.WordWrap
        }

        Button {
            Layout.fillWidth: true
            text: ManagedServiceController.busy ? qsTr("Signing in…") : qsTr("Sign in")
            enabled: !ManagedServiceController.busy && emailField.text.trim().length > 0 && passwordField.text.length > 0
            onClicked: root.submit()
        }

        BasicButtonType {
            Layout.topMargin: 4
            Layout.alignment: Qt.AlignHCenter
            implicitHeight: 32

            defaultColor: AmneziaStyle.color.transparent
            hoveredColor: AmneziaStyle.color.translucentWhite
            pressedColor: AmneziaStyle.color.sheerWhite
            disabledColor: AmneziaStyle.color.mutedGray
            textColor: AmneziaStyle.color.goldenApricot

            text: qsTr("No account yet?")
            rightImageSource: "qrc:/images/controls/external-link.svg"

            clickedFunc: function() {
                Qt.openUrlExternally("https://powvpn.com/register")
            }
        }
    }

    Connections {
        target: ManagedServiceController
        function onRegisteredChanged() {
            if (ManagedServiceController.registered) {
                passwordField.text = ""
                root.close()
            }
        }
        function onAccountEmailChanged() {
            if (ManagedServiceController.accountEmail.length > 0) {
                emailField.text = ManagedServiceController.accountEmail
                passwordField.text = ""
                root.close()
            }
        }
    }
}
