import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    height: theme.scaled(26)
    color: theme.accent

    Label {
        anchors.fill: parent
        anchors.leftMargin: theme.scaled(8)
        anchors.rightMargin: theme.scaled(8)
        text: fs.statusText
        color: theme.background
        font.family: "iA Writer Mono S"
        font.pixelSize: theme.scaled(11)
        elide: Text.ElideMiddle
        verticalAlignment: Text.AlignVCenter
    }
}
