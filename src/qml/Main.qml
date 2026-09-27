import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: win
    width: 1100
    height: 720
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: (i18n.revision, i18n.t("app.window_title", [fs.currentPath]))
    color: theme.background

    Material.theme: theme.darkMode ? Material.Dark : Material.Light
    Material.accent: theme.accent
    Material.background: theme.background
    Material.foreground: theme.foreground

    readonly property real textScale: theme.textScale
    readonly property int langRev: i18n.revision
    function scaled(px) { return theme.scaled(px) }
    function tr(key, args) { return (win.langRev, args === undefined ? i18n.t(key) : i18n.t(key, args)) }

    property string pendingAction: ""
    property bool showFavorites: false
    readonly property bool dialogOpen: textDialog.visible || confirmDialog.visible
                                      || choiceDialog.visible || helpDialog.visible
                                      || openWithDialog.visible
    readonly property bool shortcutsArmed: !dialogOpen

    function showMessage(msg) {
        toast.text = msg
        toastOpen.restart()
    }

    function enterFavorites() {
        showFavorites = true
        Qt.callLater(function() { favorites.focusList() })
    }

    function leaveFavorites() {
        showFavorites = false
        Qt.callLater(function() { tree.forceActiveFocus() })
    }

    function toggleFavoritesView() {
        if (showFavorites)
            leaveFavorites()
        else
            enterFavorites()
    }

    function openFavoritePath(path) {
        leaveFavorites()
        fs.goTo(path)
    }

    Connections {
        target: fs
        function onMessageChanged() { win.showMessage(fs.message) }
        function onAskTextInput(title, initial, action) {
            win.pendingAction = action
            textDialog.titleText = title
            textDialog.initialText = initial
            textDialog.open()
        }
        function onAskConfirm(message, action, danger, okLabel) {
            win.pendingAction = action
            confirmDialog.message = message
            confirmDialog.danger = danger
            confirmDialog.okLabel = okLabel
            confirmDialog.open()
        }
        function onAskChoice(message, action) {
            win.pendingAction = action
            choiceDialog.message = message
            choiceDialog.open()
        }
        function onAskOpenWith(path, mime, apps) {
            openWithDialog.filePath = path
            openWithDialog.mimeType = mime
            openWithDialog.apps = apps
            openWithDialog.open()
        }
    }

    header: ToolBar {
        id: toolBar
        Material.background: theme.lighterBackground
        contentHeight: theme.scaled(40)

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: theme.scaled(8)
            anchors.rightMargin: theme.scaled(8)
            spacing: theme.scaled(6)

            ToolButton {
                text: "↑"
                onClicked: win.showFavorites ? win.leaveFavorites() : fs.goParent()
                ToolTip.text: win.tr("ui.parent_tooltip")
                ToolTip.visible: hovered
            }
            ToolButton {
                text: "↻"
                onClicked: fs.reload()
                ToolTip.text: win.tr("ui.reload_tooltip")
                ToolTip.visible: hovered
            }
            ToolButton {
                text: win.showFavorites ? "★" : "☆"
                checkable: true
                checked: win.showFavorites
                onClicked: win.toggleFavoritesView()
                ToolTip.text: win.tr("ui.favorites_tooltip")
                ToolTip.visible: hovered
            }
            Label {
                Layout.fillWidth: true
                text: win.showFavorites ? win.tr("ui.favorites") : fs.currentPath
                elide: Text.ElideMiddle
                font.family: "iA Writer Mono S"
                font.pixelSize: theme.scaled(13)
                color: theme.foreground
            }
            ToolButton {
                text: "?"
                onClicked: helpDialog.open()
            }
        }
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        Item {
            SplitView.preferredWidth: parent.width * 0.42
            SplitView.minimumWidth: 220

            FileTreeView {
                id: tree
                anchors.fill: parent
                visible: !win.showFavorites
                enabled: !win.showFavorites
            }

            FavoritesView {
                id: favorites
                anchors.fill: parent
                visible: win.showFavorites
                enabled: win.showFavorites
                onOpenFavorite: function(path) { win.openFavoritePath(path) }
            }
        }

        PreviewPane {
            id: preview
            SplitView.fillWidth: true
            SplitView.minimumWidth: 280
        }
    }

    footer: StatusBar { }

    ConfirmDialog {
        id: confirmDialog
        onConfirmed: fs.completeAction(win.pendingAction)
    }

    TextInputDialog {
        id: textDialog
        onSubmitted: function(value) { fs.completeAction(win.pendingAction, value) }
    }

    ChoiceDialog {
        id: choiceDialog
        onTrashChosen: fs.completeAction("deleteTrash")
        onPermanentChosen: fs.completeAction("deletePermanent")
    }

    OpenWithDialog {
        id: openWithDialog
        onAppChosen: function(desktopPath) { fs.openWithDesktop(desktopPath) }
    }

    HelpDialog { id: helpDialog }

    Rectangle {
        id: toast
        property alias text: toastLabel.text
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: theme.scaled(48)
        width: Math.min(parent.width - theme.scaled(40), toastLabel.implicitWidth + theme.scaled(24))
        height: toastLabel.implicitHeight + theme.scaled(12)
        radius: 4
        color: theme.accent
        opacity: 0
        visible: opacity > 0

        Label {
            id: toastLabel
            anchors.centerIn: parent
            color: theme.background
            font.pixelSize: theme.scaled(12)
            wrapMode: Text.Wrap
            width: parent.width - theme.scaled(16)
            horizontalAlignment: Text.AlignHCenter
        }
    }

    SequentialAnimation {
        id: toastOpen
        NumberAnimation { target: toast; property: "opacity"; to: 0.95; duration: 120 }
        PauseAnimation { duration: 2200 }
        NumberAnimation { target: toast; property: "opacity"; to: 0; duration: 250 }
    }


    // Shortcuts — ApplicationShortcut so ListView focus does not block them
    Shortcut {
        sequence: "Return"
        enabled: win.shortcutsArmed
        context: Qt.ApplicationShortcut
        onActivated: win.showFavorites ? favorites.activateCurrent() : tree.activateCurrent()
    }
    Shortcut {
        sequence: "Enter"
        enabled: win.shortcutsArmed
        context: Qt.ApplicationShortcut
        onActivated: win.showFavorites ? favorites.activateCurrent() : tree.activateCurrent()
    }
    Shortcut { sequence: "q"; enabled: win.shortcutsArmed; context: Qt.ApplicationShortcut; onActivated: win.close() }
    Shortcut { sequence: "Ctrl+Q"; context: Qt.ApplicationShortcut; onActivated: win.close() }
    Shortcut { sequence: "?"; enabled: win.shortcutsArmed; context: Qt.ApplicationShortcut; onActivated: helpDialog.open() }
    Shortcut {
        sequence: "o"
        enabled: win.shortcutsArmed && !win.showFavorites
        context: Qt.ApplicationShortcut
        onActivated: tree.openCurrent()
    }
    Shortcut {
        sequence: "Shift+O"
        enabled: win.shortcutsArmed && !win.showFavorites
        context: Qt.ApplicationShortcut
        onActivated: fs.requestOpenWith()
    }
    Shortcut {
        sequence: "e"
        enabled: win.shortcutsArmed && !win.showFavorites
        context: Qt.ApplicationShortcut
        onActivated: fs.editSelected()
    }
    Shortcut {
        sequence: "r"
        enabled: win.shortcutsArmed && !win.showFavorites
        context: Qt.ApplicationShortcut
        onActivated: fs.requestRename()
    }
    Shortcut {
        sequence: "d"
        enabled: win.shortcutsArmed && !win.showFavorites
        context: Qt.ApplicationShortcut
        onActivated: fs.requestDelete(false)
    }
    Shortcut {
        sequence: "Shift+D"
        enabled: win.shortcutsArmed && !win.showFavorites
        context: Qt.ApplicationShortcut
        onActivated: fs.requestDelete(true)
    }
    Shortcut {
        sequence: "n"
        enabled: win.shortcutsArmed && !win.showFavorites
        context: Qt.ApplicationShortcut
        onActivated: fs.requestNewFile()
    }
    Shortcut {
        sequence: "Shift+N"
        enabled: win.shortcutsArmed && !win.showFavorites
        context: Qt.ApplicationShortcut
        onActivated: fs.requestNewFolder()
    }
    Shortcut {
        sequence: "c"
        enabled: win.shortcutsArmed && !win.showFavorites
        context: Qt.ApplicationShortcut
        onActivated: fs.copySelected()
    }
    Shortcut {
        sequence: "x"
        enabled: win.shortcutsArmed && !win.showFavorites
        context: Qt.ApplicationShortcut
        onActivated: fs.cutSelected()
    }
    Shortcut {
        sequence: "p"
        enabled: win.shortcutsArmed && !win.showFavorites
        context: Qt.ApplicationShortcut
        onActivated: fs.paste()
    }
    Shortcut {
        sequence: "h"
        enabled: win.shortcutsArmed && !win.showFavorites
        context: Qt.ApplicationShortcut
        onActivated: fs.setShowHidden(!fs.showHidden)
    }
    Shortcut {
        sequence: "s"
        enabled: win.shortcutsArmed && !win.showFavorites
        context: Qt.ApplicationShortcut
        onActivated: fs.computeSelectedSize()
    }
    Shortcut {
        sequence: "Shift+S"
        enabled: win.shortcutsArmed && !win.showFavorites
        context: Qt.ApplicationShortcut
        onActivated: fs.cycleSort()
    }
    Shortcut {
        sequence: "R"
        enabled: win.shortcutsArmed && !win.showFavorites
        context: Qt.ApplicationShortcut
        onActivated: fs.reload()
    }
    Shortcut {
        sequence: "Backspace"
        enabled: win.shortcutsArmed
        context: Qt.ApplicationShortcut
        onActivated: win.showFavorites ? win.leaveFavorites() : fs.goParent()
    }
    Shortcut {
        sequence: "-"
        enabled: win.shortcutsArmed
        context: Qt.ApplicationShortcut
        onActivated: win.showFavorites ? win.leaveFavorites() : fs.goParent()
    }
    Shortcut {
        sequence: "Left"
        enabled: win.shortcutsArmed
        context: Qt.ApplicationShortcut
        onActivated: win.showFavorites ? win.leaveFavorites() : fs.goParent()
    }
    Shortcut {
        sequence: "Right"
        enabled: win.shortcutsArmed
        context: Qt.ApplicationShortcut
        onActivated: win.showFavorites ? favorites.activateCurrent() : tree.activateCurrent()
    }
    Shortcut {
        sequence: "Space"
        enabled: win.shortcutsArmed && !win.showFavorites
        context: Qt.ApplicationShortcut
        onActivated: fs.toggleMark()
    }
    Shortcut {
        sequence: "Ctrl+A"
        enabled: win.shortcutsArmed && !win.showFavorites
        context: Qt.ApplicationShortcut
        onActivated: fs.markAll()
    }
    Shortcut {
        sequence: "Escape"
        enabled: win.shortcutsArmed
        context: Qt.ApplicationShortcut
        onActivated: {
            if (win.showFavorites)
                win.leaveFavorites()
            else
                fs.clearMarksAndFilter()
        }
    }
    Shortcut {
        sequence: "f"
        enabled: win.shortcutsArmed
        context: Qt.ApplicationShortcut
        onActivated: win.toggleFavoritesView()
    }
    Shortcut {
        sequence: "Ctrl+F"
        enabled: win.shortcutsArmed
        context: Qt.ApplicationShortcut
        onActivated: fs.toggleFavoriteSelected()
    }
    Shortcut {
        sequence: "/"
        enabled: win.shortcutsArmed && !win.showFavorites
        context: Qt.ApplicationShortcut
        onActivated: {
            win.pendingAction = "filter"
            textDialog.titleText = win.tr("prompt.filter")
            textDialog.initialText = fs.nameFilter
            textDialog.open()
        }
    }
    Shortcut {
        sequence: "g"
        enabled: win.shortcutsArmed && !win.showFavorites
        context: Qt.ApplicationShortcut
        onActivated: {
            win.pendingAction = "goto"
            textDialog.titleText = win.tr("prompt.goto")
            textDialog.initialText = fs.selectedPath
            textDialog.open()
        }
    }
    Shortcut {
        sequence: "l"
        enabled: win.shortcutsArmed
        context: Qt.ApplicationShortcut
        onActivated: fs.cycleLanguage()
    }
}
