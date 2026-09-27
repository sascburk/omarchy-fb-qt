import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    property string titleText: ""
    property string initialText: ""
    property int focusIndex: 0
    signal submitted(string value)

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    anchors.centerIn: Overlay.overlay
    parent: Overlay.overlay
    width: Math.min(500, Overlay.overlay ? Overlay.overlay.width - 40 : 500)
    title: titleText
    standardButtons: Dialog.NoButton

    onAboutToShow: {
        input.text = initialText
        input.selectAll()
        focusIndex = 0
        Qt.callLater(function() { input.forceActiveFocus() })
    }

    function applyFocus() {
        if (focusIndex === 0)
            input.forceActiveFocus()
        else if (focusIndex === 1)
            cancelBtn.forceActiveFocus()
        else
            acceptBtn.forceActiveFocus()
    }

    function moveFocus(delta) {
        focusIndex = (focusIndex + delta + 3) % 3
        applyFocus()
    }

    function acceptInput() {
        submitted(input.text)
        close()
    }

    ColumnLayout {
        width: parent.width
        spacing: theme.scaled(10)

        TextField {
            id: input
            Layout.fillWidth: true
            font.family: "iA Writer Mono S"
            font.pixelSize: theme.scaled(13)
            selectByMouse: true
            focusPolicy: Qt.StrongFocus

            Keys.onReturnPressed: root.acceptInput()
            Keys.onEnterPressed: root.acceptInput()
            Keys.onEscapePressed: root.reject()
            Keys.onDownPressed: {
                root.focusIndex = 2
                root.applyFocus()
            }
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Tab) {
                    root.moveFocus(1)
                    event.accepted = true
                } else if (event.key === Qt.Key_Backtab) {
                    root.moveFocus(-1)
                    event.accepted = true
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: (i18n.revision, i18n.t("ui.dialog_hint_input"))
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
            highlighted: root.focusIndex === 1
            onClicked: root.reject()
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Left || event.key === Qt.Key_Up) {
                    root.moveFocus(-1)
                    event.accepted = true
                } else if (event.key === Qt.Key_Right || event.key === Qt.Key_Down
                           || event.key === Qt.Key_Tab) {
                    root.moveFocus(1)
                    event.accepted = true
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                           || event.key === Qt.Key_Escape) {
                    root.reject()
                    event.accepted = true
                }
            }
        }
        Button {
            id: acceptBtn
            text: (i18n.revision, i18n.t("dialog.ok_enter"))
            focusPolicy: Qt.StrongFocus
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            highlighted: root.focusIndex === 2
            onClicked: root.acceptInput()
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Left || event.key === Qt.Key_Up
                        || event.key === Qt.Key_Backtab) {
                    root.moveFocus(-1)
                    event.accepted = true
                } else if (event.key === Qt.Key_Right || event.key === Qt.Key_Down
                           || event.key === Qt.Key_Tab) {
                    root.moveFocus(1)
                    event.accepted = true
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    root.acceptInput()
                    event.accepted = true
                } else if (event.key === Qt.Key_Escape) {
                    root.reject()
                    event.accepted = true
                }
            }
        }
    }
}
