import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    Rectangle {
        anchors.fill: parent
        color: theme.darkerBackground

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: theme.scaled(28)
                color: theme.muted
                Label {
                    anchors.fill: parent
                    anchors.leftMargin: theme.scaled(8)
                    text: (i18n.revision, i18n.t("ui.files"))
                          + (fs.nameFilter.length ? "  [/ " + fs.nameFilter + "]" : "")
                          + "  ·  " + i18n.t("ui.sort", [fs.sortBy])
                          + (fs.showHidden ? "  ·  " + i18n.t("ui.hidden") : "")
                    color: theme.foreground
                    font.pixelSize: theme.scaled(11)
                    font.family: "iA Writer Mono S"
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }

            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: fs.model
                currentIndex: -1
                focus: true
                activeFocusOnTab: true
                keyNavigationEnabled: true
                highlightMoveDuration: 0
                ScrollBar.vertical: ScrollBar { }

                // Keep backend selection in sync with keyboard navigation
                onCurrentIndexChanged: {
                    if (currentIndex >= 0 && currentIndex < count)
                        fs.selectIndex(currentIndex)
                }

                delegate: ItemDelegate {
                    id: row
                    width: list.width
                    height: theme.scaled(28)
                    highlighted: ListView.isCurrentItem
                    padding: 0
                    // Keep focus on the ListView so Enter/shortcuts are not swallowed
                    focusPolicy: Qt.NoFocus
                    activeFocusOnTab: false

                    background: Item {
                        // Mark wash (behind focus)
                        Rectangle {
                            anchors.fill: parent
                            color: model.marked ? theme.markFill : "transparent"
                        }
                        // Focus wash always on top so the current row stays obvious
                        Rectangle {
                            anchors.fill: parent
                            color: row.highlighted ? theme.focusFill : "transparent"
                        }
                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: theme.scaled(3)
                            color: theme.accent
                            visible: row.highlighted
                        }
                    }

                    contentItem: RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: theme.scaled(8)
                        anchors.rightMargin: theme.scaled(8)
                        spacing: theme.scaled(8)

                        Label {
                            text: model.marked ? "●" : (model.isDir ? "▸" : " ")
                            color: model.marked ? theme.mark : theme.mutedForeground
                            font.pixelSize: theme.scaled(12)
                            font.family: "iA Writer Mono S"
                            Layout.preferredWidth: theme.scaled(14)
                        }
                        Label {
                            Layout.fillWidth: true
                            text: model.name + (model.isSymLink ? " →" : "")
                            color: model.isDir ? theme.accent : theme.foreground
                            font.bold: model.isDir
                            font.pixelSize: theme.scaled(13)
                            font.family: "iA Writer Mono S"
                            elide: Text.ElideRight
                        }
                        Label {
                            text: model.displaySize
                            color: theme.mutedForeground
                            font.pixelSize: theme.scaled(11)
                            font.family: "iA Writer Mono S"
                            Layout.preferredWidth: theme.scaled(72)
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    onClicked: {
                        list.currentIndex = index
                        fs.selectIndex(index)
                        list.forceActiveFocus()
                    }
                    onDoubleClicked: {
                        list.currentIndex = index
                        root.activateCurrent()
                        list.forceActiveFocus()
                    }
                }

                Component.onCompleted: {
                    syncSelectionFromController()
                    forceActiveFocus()
                }

                function syncSelectionFromController() {
                    const idx = fs.selectedIndex
                    if (idx >= 0 && idx < count) {
                        currentIndex = idx
                        positionViewAtIndex(idx, ListView.Contain)
                    } else if (count > 0) {
                        currentIndex = 0
                    } else {
                        currentIndex = -1
                    }
                }

                Connections {
                    target: fs.model
                    function onModelReset() {
                        Qt.callLater(list.syncSelectionFromController)
                    }
                }

                Connections {
                    target: fs
                    function onSelectedPathChanged() {
                        if (list.currentIndex !== fs.selectedIndex)
                            list.syncSelectionFromController()
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 1
        color: theme.accent
        opacity: 0.6
    }

    // Always use the visibly highlighted row (not a stale backend index)
    function activateCurrent() {
        if (list.currentIndex < 0)
            return
        fs.selectIndex(list.currentIndex)
        fs.activateIndex(list.currentIndex)
    }

    function openCurrent() {
        if (list.currentIndex < 0)
            return
        fs.selectIndex(list.currentIndex)
        fs.openSelected()
    }
}
