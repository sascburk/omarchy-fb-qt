import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    anchors.centerIn: Overlay.overlay
    parent: Overlay.overlay
    width: Math.min(520, Overlay.overlay ? Overlay.overlay.width - 40 : 520)
    title: (i18n.revision, i18n.t("dialog.help_title"))
    standardButtons: Dialog.NoButton

    onAboutToShow: closeBtn.forceActiveFocus()

    ScrollView {
        width: parent.width
        height: Math.min(420, contentLabel.implicitHeight + 20)
        clip: true

        Label {
            id: contentLabel
            width: root.availableWidth
            wrapMode: Text.Wrap
            color: theme.foreground
            font.family: "iA Writer Mono S"
            font.pixelSize: theme.scaled(12)
            text: (i18n.revision, i18n.t("help.body"))
        }
    }

    footer: DialogButtonBox {
        Button {
            id: closeBtn
            text: (i18n.revision, i18n.t("dialog.close_esc"))
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            focusPolicy: Qt.StrongFocus
            onClicked: root.close()
            Keys.onEscapePressed: root.close()
            Keys.onReturnPressed: root.close()
            Keys.onEnterPressed: root.close()
        }
    }
}
