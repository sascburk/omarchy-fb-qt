import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    property string message: ""
    property bool danger: false
    property string okLabel: "OK"
    property int focusIndex: 0
    signal confirmed()

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    anchors.centerIn: Overlay.overlay
    parent: Overlay.overlay
    width: Math.min(440, Overlay.overlay ? Overlay.overlay.width - 40 : 440)
    title: danger ? (i18n.revision, i18n.t("dialog.confirm")) : (i18n.revision, i18n.t("dialog.notice"))
    standardButtons: Dialog.NoButton

    onAboutToShow: {
        focusIndex = danger ? 0 : 1
        Qt.callLater(applyButtonFocus)
    }

    function applyButtonFocus() {
        if (focusIndex === 0)
            cancelBtn.forceActiveFocus()
        else
            okBtn.forceActiveFocus()
    }

    function moveFocus(delta) {
        const count = 2
        focusIndex = (focusIndex + delta + count) % count
        applyButtonFocus()
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
            text: (i18n.revision, i18n.t("ui.dialog_hint_confirm"))
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
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Left || event.key === Qt.Key_Right
                        || event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                    root.moveFocus(event.key === Qt.Key_Left || event.key === Qt.Key_Up ? -1 : 1)
                    event.accepted = true
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    root.reject()
                    event.accepted = true
                }
            }
        }
        Button {
            id: okBtn
            text: root.okLabel + "  [Enter]"
            focusPolicy: Qt.StrongFocus
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            highlighted: root.focusIndex === 1
            onClicked: {
                root.confirmed()
                root.close()
            }
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Left || event.key === Qt.Key_Right
                        || event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                    root.moveFocus(event.key === Qt.Key_Left || event.key === Qt.Key_Up ? -1 : 1)
                    event.accepted = true
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    root.confirmed()
                    root.close()
                    event.accepted = true
                }
            }
        }
    }
}
