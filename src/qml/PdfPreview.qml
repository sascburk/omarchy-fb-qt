import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Pdf

// Optional PDF page preview (requires QtQuick.Pdf from qt6-webengine).
Item {
    id: root
    property url source
    property int page: 0

    onSourceChanged: page = 0

    PdfDocument {
        id: doc
        source: root.source
    }

    readonly property int pageCount: Math.max(0, doc.pageCount)
    readonly property bool ready: pageCount > 0 && doc.status === PdfDocument.Ready

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: theme.scaled(8)
        spacing: theme.scaled(6)

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            // Always white so dark-mode PDFs with transparent pages stay readable.
            color: "#ffffff"
            clip: true

            PdfPageImage {
                anchors.fill: parent
                anchors.margins: theme.scaled(4)
                document: doc
                currentFrame: root.page
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }

            Label {
                anchors.centerIn: parent
                visible: doc.status === PdfDocument.Error
                text: doc.error
                color: "#b91c1c"
                wrapMode: Text.Wrap
                width: parent.width - theme.scaled(24)
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: theme.scaled(12)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: theme.scaled(8)
            visible: root.ready

            Button {
                text: "←"
                enabled: root.page > 0
                onClicked: root.page = Math.max(0, root.page - 1)
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: (root.page + 1) + " / " + root.pageCount
                color: theme.foreground
                font.pixelSize: theme.scaled(12)
                font.family: "iA Writer Mono S"
            }
            Button {
                text: "→"
                enabled: root.page + 1 < root.pageCount
                onClicked: root.page = Math.min(root.pageCount - 1, root.page + 1)
            }
        }
    }
}
