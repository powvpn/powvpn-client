import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import QtCore

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"
import "../Config"

PageType {
    id: root

    property bool isRestoringBackup: false
    property bool pendingAccountKeyNavigation: false

    Connections {
        target: ImportController

        function onQrDecodingFinished() {
            if (Qt.platform.os === "ios") {
                PageController.closePage()
            }
            PageController.goToPage(PageEnum.PageSetupWizardViewConfig)
        }
    }

    ListViewType {
        id: listView

        anchors.fill: parent

        header: ColumnLayout {
            width: listView.width

            HeaderTypeWithButton {
                id: moreButton

                property bool isVisible: SettingsController.getInstallationUuid() !== "" || PageController.isStartPageVisible()
                
                Layout.fillWidth: true
                Layout.topMargin: 24 + PageController.safeAreaTopMargin
                Layout.rightMargin: 16
                Layout.leftMargin: 16

                headerText: qsTr("Connection")

                actionButtonImage: isVisible ? "qrc:/images/controls/more-vertical.svg" : ""
                actionButtonFunction: function() {
                    moreActionsDrawer.openTriggered()
                }

                DrawerType2 {
                    id: moreActionsDrawer

                    parent: root

                    anchors.fill: parent
                    expandedHeight: root.height * 0.5

                    expandedStateContent: ColumnLayout {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 0

                        BaseHeaderType {
                            Layout.fillWidth: true
                            Layout.topMargin: 32
                            Layout.leftMargin: 16
                            Layout.rightMargin: 16

                            headerText: qsTr("Settings")
                        }

                        SwitcherType {
                            id: switcher
                            Layout.fillWidth: true
                            Layout.topMargin: 16
                            Layout.leftMargin: 16
                            Layout.rightMargin: 16

                            text: qsTr("Enable logs")

                            visible: PageController.isStartPageVisible()
                            checked: SettingsController.isLoggingEnabled
                            onToggled: function() {
                                if (checked !== SettingsController.isLoggingEnabled) {
                                    SettingsController.isLoggingEnabled = checked
                                }
                            }
                        }

                        LabelWithButtonType {
                            Layout.fillWidth: true

                            text: qsTr("Export client logs")
                            rightImageSource: "qrc:/images/controls/chevron-right.svg"

                            visible: PageController.isStartPageVisible()

                            clickedFunction: function() {
                                var fileName = ""
                                if (GC.isMobile()) {
                                    fileName = "PowVPN.log"
                                } else {
                                    fileName = SystemController.getFileName(qsTr("Save"),
                                                                            qsTr("Logs files (*.log)"),
                                                                            StandardPaths.standardLocations(StandardPaths.DocumentsLocation) + "/PowVPN",
                                                                            true,
                                                                            ".log")
                                }
                                if (fileName !== "") {
                                    PageController.showBusyIndicator(true)
                                    SettingsController.exportLogsFile(fileName)
                                    PageController.showBusyIndicator(false)
                                    PageController.showNotificationMessage(qsTr("Logs file saved"))
                                }
                            }
                        }

                        LabelWithButtonType {
                            id: supportUuid
                            Layout.fillWidth: true
                            Layout.topMargin: 16

                            text: qsTr("Support tag")
                            descriptionText: SettingsController.getInstallationUuid()

                            descriptionOnTop: true

                            rightImageSource: "qrc:/images/controls/copy.svg"
                            rightImageColor: AmneziaStyle.color.paleGray

                            visible: SettingsController.getInstallationUuid() !== ""
                            clickedFunction: function() {
                                GC.copyToClipBoard(descriptionText)
                                PageController.showNotificationMessage(qsTr("Copied"))
                                if (!GC.isMobile()) {
                                    this.rightButton.forceActiveFocus()
                                }
                            }
                        }
                    }
                }
            }

            ParagraphTextType {
                objectName: "insertKeyLabel"

                Layout.fillWidth: true
                Layout.topMargin: 32
                Layout.rightMargin: 16
                Layout.leftMargin: 16
                Layout.bottomMargin: 24

                text: qsTr("Insert the key, add a configuration file or scan the QR-code")
            }

            TextFieldWithHeaderType {
                id: textKey

                Layout.fillWidth: true
                Layout.rightMargin: 16
                Layout.leftMargin: 16

                headerText: qsTr("Insert key")
                buttonText: qsTr("Insert")

                clickedFunc: function() {
                    textField.text = ""
                    textField.paste()
                }
            }

            BasicButtonType {
                id: continueButton

                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.rightMargin: 16
                Layout.leftMargin: 16

                visible: textKey.textField.text !== ""

                text: qsTr("Continue")

                clickedFunc: function() {
                    if (ManagedServiceController.importAccountKey(textKey.textField.text)) {
                        root.pendingAccountKeyNavigation = true
                    } else if (ImportController.extractConfigFromData(textKey.textField.text)) {
                        PageController.goToPage(PageEnum.PageSetupWizardViewConfig)
                    }
                }
            }

            ParagraphTextType {
                Layout.fillWidth: true
                Layout.topMargin: 32
                Layout.rightMargin: 16
                Layout.leftMargin: 16
                Layout.bottomMargin: 24

                color: AmneziaStyle.color.charcoalGray
                text: qsTr("Other connection options")
            }
        }

        model: variants

        delegate: ColumnLayout {
            width: listView.width

            CardWithIconsType {
                Layout.fillWidth: true
                Layout.rightMargin: 16
                Layout.leftMargin: 16
                Layout.bottomMargin: 16

                visible: isVisible

                headerText: title
                bodyText: description

                showRecommendedBadge: featuredAmneziaConnection
                recommendedText: featuredAmneziaConnection ? qsTr("Recommended") : ""

                rightImageSource: "qrc:/images/controls/chevron-right.svg"
                leftImageSource: imageSource

                enabled: !root.isRestoringBackup

                onClicked: { handler() }

                Keys.onEnterPressed: this.clicked()
                Keys.onReturnPressed: this.clicked()
            }
        }

        footer: ColumnLayout {
            width: listView.width

            BasicButtonType {
                id: siteLink2
                Layout.topMargin: 24
                Layout.bottomMargin: 16
                Layout.alignment: Qt.AlignHCenter
                implicitHeight: 32

                visible: Qt.platform.os !== "ios" && !IsMacOsNeBuild

                defaultColor: AmneziaStyle.color.transparent
                hoveredColor: AmneziaStyle.color.translucentWhite
                pressedColor: AmneziaStyle.color.sheerWhite
                disabledColor: AmneziaStyle.color.mutedGray
                textColor: AmneziaStyle.color.goldenApricot

                text: qsTr("PowVPN")

                rightImageSource: "qrc:/images/controls/external-link.svg"

                clickedFunc: function() {
                    Qt.openUrlExternally("https://powvpn.com")
                }
            }
        }
    }

    property list<QtObject> variants: [
        accountLogin,
        fileOpen,
        qrScan,
        restorePurchases
    ]
    
    QtObject {
        id: accountLogin

        property bool featuredAmneziaConnection: false
        property string title: qsTr("Authorization")
        property string description: ManagedServiceController.accountEmail.length > 0
                                     ? qsTr("Signed in as %1").arg(ManagedServiceController.accountEmail)
                                     : qsTr("")
        property string imageSource: "qrc:/images/controls/mail.svg"
        property bool isVisible: true
        property var handler: function() {
            PageController.goToPage(PageEnum.PageAccountLogin)
        }
    }

    QtObject {
        id: fileOpen

        property bool featuredAmneziaConnection: false
        property string title: qsTr("File with connection settings")
        property string description: qsTr("")
        property string imageSource: "qrc:/images/controls/folder-search-2.svg"
        property bool isVisible: true
        property var handler: function() {
            var nameFilter = "Config files (*.vpn *.ovpn *.conf *.json)"
            var fileName = SystemController.getFileName(qsTr("Open config file"), nameFilter)
            if (fileName !== "") {
                if (ImportController.extractConfigFromFile(fileName)) {
                    PageController.goToPage(PageEnum.PageSetupWizardViewConfig)
                }
            }
        }
    }

    QtObject {
        id: qrScan

        property bool featuredAmneziaConnection: false
        property string title: qsTr("QR code")
        property string description: qsTr("")
        property string imageSource: "qrc:/images/controls/scan-line.svg"
        property bool isVisible: SettingsController.isCameraPresent()
        property var handler: function() {
            ImportController.startDecodingQr()
            if (Qt.platform.os === "ios") {
                PageController.goToPage(PageEnum.PageSetupWizardQrReader)
            }
        }
    }

    QtObject {
        id: restorePurchases

        property bool featuredAmneziaConnection: false
        property string title: qsTr("Restore purchases")
        property string description: qsTr("")
        property string imageSource: "qrc:/images/controls/refresh-cw.svg"
        property bool isVisible: Qt.platform.os === "ios" || IsMacOsNeBuild || IsPlayBuild
        property var handler: function() {
            PageController.showBusyIndicator(true)
            SubscriptionUiController.restoreServiceFromStore()
            PageController.showBusyIndicator(false)
        }
    }

    Connections {
        target: ManagedConnectionAdapter

        function onManagedProfilesReady() {
            if (root.pendingAccountKeyNavigation) {
                root.pendingAccountKeyNavigation = false
                PageController.goToPageHome()
            }
        }
    }
}
