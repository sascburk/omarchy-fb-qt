import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    property string filePath: ""
    property string mimeType: ""
    property var apps: []
    signal appChosen(string desktopPath)

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    anchors.centerIn: Overlay.overlay
    parent: Overlay.overlay
    width: Math.min(520, Overlay.overlay ? Overlay.overlay.width - 40 : 520)
    height: Math.min(480, Overlay.overlay ? Overlay.overlay.height - 40 : 480)
    title: (i18n.revision, i18n.t("dialog.open_with_title"))
    standardButtons: Dialog.NoButton

    onAboutToShow: {
        Qt.callLater(function() {
            if (list.count > 0)
                list.currentIndex = 0
            list.forceActiveFocus()
        })
    }

    function appAt(index) {
        if (index < 0 || !root.apps || index >= root.apps.length)
            return null
        return root.apps[index]
    }

    function activateCurrent() {
        const app = root.appAt(list.currentIndex)
        if (!app)
            return
        const desktopPath = app.desktopPath || app["desktopPath"] || ""
        if (!desktopPath)
            return
        root.appChosen(desktopPath)
        root.close()
    }

    // Dialog is not an Item — use Shortcuts instead of Keys on the Dialog root.
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: root.visible
        onActivated: root.activateCurrent()
    }
    Shortcut {
        sequence: "Escape"
        enabled: root.visible
        onActivated: root.reject()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: theme.scaled(8)

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: (i18n.revision, i18n.t("dialog.open_with_file", [root.filePath]))
            color: theme.foreground
            font.pixelSize: theme.scaled(12)
            font.family: "iA Writer Mono S"
            elide: Text.ElideMiddle
        }
        Label {
            Layout.fillWidth: true
            visible: root.mimeType.length > 0
            text: root.mimeType
            color: theme.mutedForeground
            font.pixelSize: theme.scaled(11)
            font.family: "iA Writer Mono S"
        }

        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true
            padding: 0

            ListView {
                id: list
                anchors.fill: parent
                clip: true
                model: root.apps
                currentIndex: 0
                keyNavigationEnabled: true
                focus: true
                activeFocusOnTab: true
                ScrollBar.vertical: ScrollBar { }

                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        root.activateCurrent()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Escape) {
                        root.reject()
                        event.accepted = true
                    }
                }

                delegate: ItemDelegate {
                    width: list.width
                    height: theme.scaled(32)
                    highlighted: ListView.isCurrentItem
                    focusPolicy: Qt.NoFocus
                    padding: theme.scaled(8)

                    contentItem: RowLayout {
                        spacing: theme.scaled(8)
                        Label {
                            text: (modelData && modelData.recommended) ? "★" : " "
                            color: theme.accent
                            font.pixelSize: theme.scaled(12)
                            Layout.preferredWidth: theme.scaled(14)
                        }
                        Label {
                            Layout.fillWidth: true
                            text: modelData ? (modelData.name || "") : ""
                            color: theme.foreground
                            font.pixelSize: theme.scaled(13)
                            elide: Text.ElideRight
                        }
                    }

                    onClicked: {
                        list.currentIndex = index
                        list.forceActiveFocus()
                    }
                    onDoubleClicked: {
                        list.currentIndex = index
                        root.activateCurrent()
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: (i18n.revision, i18n.t("ui.dialog_hint_open_with"))
            color: theme.mutedForeground
            font.pixelSize: theme.scaled(11)
            font.family: "iA Writer Mono S"
        }
    }

    footer: DialogButtonBox {
        alignment: Qt.AlignRight
        Button {
            text: (i18n.revision, i18n.t("dialog.cancel_esc"))
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: root.reject()
        }
        Button {
            text: (i18n.revision, i18n.t("dialog.open_enter"))
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            highlighted: true
            onClicked: root.activateCurrent()
        }
    }
}
