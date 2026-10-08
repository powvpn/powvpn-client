import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"

PageType {
    id: root

    property bool authenticationAccepted: false

    function submit() {
        var email = emailField.textField.text.trim()
        var password = passwordField.textField.text
        if (email.length === 0 || email.indexOf("@") < 0 || password.length === 0) {
            PageController.showNotificationMessage(qsTr("Enter your email and password"))
            return
        }
        root.authenticationAccepted = false
        ManagedServiceController.signIn(email, password)
    }

    ColumnLayout {
        id: pageHeader
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 20 + PageController.safeAreaTopMargin

        BackButtonType {
            id: backButton
        }

        BaseHeaderType {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            headerText: qsTr("Authorization")
        }
    }

    ColumnLayout {
        id: loginForm

        width: Math.min(420, parent.width - 32)
        anchors.top: pageHeader.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 40
        spacing: 16

        ParagraphTextType {
            Layout.fillWidth: true
            text: qsTr("Sign in to sync all available PowVPN locations with this device.")
            color: AmneziaStyle.color.mutedGray
            wrapMode: Text.WordWrap
        }

        TextFieldWithHeaderType {
            id: emailField
            Layout.fillWidth: true
            Layout.topMargin: 4
            headerText: qsTr("Email")
            textField.placeholderText: qsTr("Email")
            textField.inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
            textField.text: ManagedServiceController.accountEmail

            Connections {
                target: emailField.textField
                function onAccepted() { passwordField.textField.forceActiveFocus() }
            }
        }

        TextFieldWithHeaderType {
            id: passwordField
            Layout.fillWidth: true
            headerText: qsTr("Password")
            textField.placeholderText: qsTr("Password")
            textField.echoMode: TextInput.Password

            Connections {
                target: passwordField.textField
                function onAccepted() { root.submit() }
            }
        }

        ParagraphTextType {
            Layout.fillWidth: true
            visible: ManagedServiceController.lastError.length > 0
            text: ManagedServiceController.lastError
            color: AmneziaStyle.color.vibrantRed
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            visible: ManagedServiceController.busy
            running: visible
        }

        BasicButtonType {
            Layout.fillWidth: true
            enabled: !ManagedServiceController.busy
                     && emailField.textField.text.trim().length > 0
                     && passwordField.textField.text.length > 0
            text: ManagedServiceController.busy ? qsTr("Signing in…") : qsTr("Sign in")
            clickedFunc: function() { root.submit() }
        }

        BasicButtonType {
            Layout.topMargin: 20
            Layout.alignment: Qt.AlignHCenter
            implicitHeight: 32
            defaultColor: AmneziaStyle.color.transparent
            hoveredColor: AmneziaStyle.color.translucentWhite
            pressedColor: AmneziaStyle.color.sheerWhite
            disabledColor: AmneziaStyle.color.mutedGray
            textColor: AmneziaStyle.color.goldenApricot
            text: qsTr("No account yet?")
            rightImageSource: "qrc:/images/controls/external-link.svg"
            clickedFunc: function() { Qt.openUrlExternally("https://powvpn.com/register") }
        }
    }

    Connections {
        target: ManagedServiceController

        function onAccountAuthenticated(email) {
            root.authenticationAccepted = true
        }
    }

    Connections {
        target: ManagedConnectionAdapter

        function onManagedProfilesReady() {
            if (root.authenticationAccepted) {
                passwordField.textField.text = ""
                PageController.goToPageHome()
            }
        }
    }

    Component.onCompleted: Qt.callLater(function() { emailField.textField.forceActiveFocus() })
}
