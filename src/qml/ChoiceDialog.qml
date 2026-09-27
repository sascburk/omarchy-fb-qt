import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    property string message: ""
    property int focusIndex: 1
    signal trashChosen()
    signal permanentChosen()

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    anchors.centerIn: Overlay.overlay
    parent: Overlay.overlay
    width: Math.min(500, Overlay.overlay ? Overlay.overlay.width - 40 : 500)
    title: (i18n.revision, i18n.t("dialog.delete_title"))
    standardButtons: Dialog.NoButton

    onAboutToShow: {
        focusIndex = 1
        Qt.callLater(applyButtonFocus)
    }

    function applyButtonFocus() {
        const buttons = [cancelBtn, trashBtn, permanentBtn]
        if (focusIndex >= 0 && focusIndex < buttons.length)
            buttons[focusIndex].forceActiveFocus()
    }

    function moveFocus(delta) {
        focusIndex = (focusIndex + delta + 3) % 3
        applyButtonFocus()
    }

    function activateFocused() {
        if (focusIndex === 0) {
            reject()
        } else if (focusIndex === 1) {
            trashChosen()
            close()
        } else {
            permanentChosen()
            close()
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: theme.scaled(10)

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: root.message
            color: theme.foreground
            font.pixelSize: theme.scaled(13)
        }

        Label {
            Layout.fillWidth: true
            text: (i18n.revision, i18n.t("ui.dialog_hint_choice"))
            color: theme.mutedForeground
            font.pixelSize: theme.scaled(11)
            font.family: "iA Writer Mono S"
        }
    }

    footer: DialogButtonBox {
        alignment: Qt.AlignRight

        Button {
            id: cancelBtn
            text: (i18n.revision, i18n.t("dialog.cancel_esc"))
            focusPolicy: Qt.StrongFocus
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            highlighted: root.focusIndex === 0
            onClicked: root.reject()
            Keys.onPressed: function(event) { root.handleButtonKeys(event) }
        }
        Button {
            id: trashBtn
            text: (i18n.revision, i18n.t("dialog.trash_enter"))
            focusPolicy: Qt.StrongFocus
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            highlighted: root.focusIndex === 1
            onClicked: {
                root.trashChosen()
                root.close()
            }
            Keys.onPressed: function(event) { root.handleButtonKeys(event) }
        }
        Button {
            id: permanentBtn
            text: (i18n.revision, i18n.t("dialog.permanent_d"))
            focusPolicy: Qt.StrongFocus
            DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole
            highlighted: root.focusIndex === 2
            onClicked: {
                root.permanentChosen()
                root.close()
            }
            Keys.onPressed: function(event) { root.handleButtonKeys(event) }
        }
    }

    function handleButtonKeys(event) {
        if (event.key === Qt.Key_Left || event.key === Qt.Key_Up) {
            moveFocus(-1)
            event.accepted = true
        } else if (event.key === Qt.Key_Right || event.key === Qt.Key_Down) {
            moveFocus(1)
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            activateFocused()
            event.accepted = true
        } else if (event.key === Qt.Key_Escape) {
            reject()
            event.accepted = true
        } else if (event.key === Qt.Key_D) {
            permanentChosen()
            close()
            event.accepted = true
        } else if (event.key === Qt.Key_P || event.key === Qt.Key_Y) {
            trashChosen()
            close()
            event.accepted = true
        }
    }
}
