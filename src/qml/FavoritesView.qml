import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    signal openFavorite(string path)
    signal requestFocus()

    function activateCurrent() {
        if (list.currentIndex < 0 || list.currentIndex >= list.count)
            return
        const path = config.bookmarks[list.currentIndex]
        if (path)
            root.openFavorite(path)
    }

    function focusList() {
        list.forceActiveFocus()
        if (list.count > 0 && list.currentIndex < 0)
            list.currentIndex = 0
    }

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
                    text: (i18n.revision, i18n.t("ui.favorites"))
                          + "  ·  " + config.bookmarks.length
                          + "  ·  Ctrl+F"
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
                model: config.bookmarks
                currentIndex: count > 0 ? 0 : -1
                focus: true
                activeFocusOnTab: true
                keyNavigationEnabled: true
                highlightMoveDuration: 0
                ScrollBar.vertical: ScrollBar { }

                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        root.activateCurrent()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Delete || event.key === Qt.Key_D) {
                        if (currentIndex >= 0 && currentIndex < count) {
                            fs.removeBookmark(config.bookmarks[currentIndex])
                            event.accepted = true
                        }
                    }
                }

                delegate: ItemDelegate {
                    id: row
                    width: list.width
                    height: theme.scaled(32)
                    highlighted: ListView.isCurrentItem
                    padding: 0
                    focusPolicy: Qt.NoFocus

                    background: Rectangle {
                        color: row.highlighted ? theme.accent : "transparent"
                        opacity: row.highlighted ? 0.35 : 1.0
                    }

                    contentItem: RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: theme.scaled(8)
                        anchors.rightMargin: theme.scaled(8)
                        spacing: theme.scaled(8)

                        Label {
                            text: "★"
                            color: theme.accent
                            font.pixelSize: theme.scaled(13)
                            Layout.preferredWidth: theme.scaled(16)
                        }
                        Label {
                            Layout.fillWidth: true
                            text: modelData
                            color: theme.foreground
                            font.pixelSize: theme.scaled(13)
                            font.family: "iA Writer Mono S"
                            elide: Text.ElideMiddle
                        }
                    }

                    onClicked: {
                        list.currentIndex = index
                        list.forceActiveFocus()
                    }
                    onDoubleClicked: {
                        list.currentIndex = index
                        root.openFavorite(modelData)
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: list.count === 0
                    text: (i18n.revision, i18n.t("ui.favorites_empty"))
                    color: theme.mutedForeground
                    font.pixelSize: theme.scaled(12)
                    font.family: "iA Writer Mono S"
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    width: parent.width - theme.scaled(24)
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

    Connections {
        target: config
        function onConfigChanged() {
            if (list.currentIndex >= list.count)
                list.currentIndex = list.count > 0 ? list.count - 1 : -1
        }
    }
}
