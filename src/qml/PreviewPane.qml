import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

Item {
    id: root

    readonly property bool showImage: fs.preview.kind === "image" && fs.preview.imagePath.length > 0
    readonly property bool showVideo: fs.preview.kind === "video" && fs.preview.mediaUrl.length > 0
    readonly property bool showAudio: fs.preview.kind === "audio" && fs.preview.mediaUrl.length > 0
    readonly property bool showPdf: fs.preview.kind === "pdf" && fs.preview.mediaUrl.length > 0
    readonly property bool showPlayer: showVideo || showAudio
    readonly property bool pdfLoaderOk: pdfLoader.status === Loader.Ready
    readonly property bool pdfUnavailable: showPdf && pdfLoader.status === Loader.Error

    // Keep sink attached; VideoOutput inside Flickable/clip often renders black.
    MediaPlayer {
        id: player
        audioOutput: AudioOutput { }
        videoOutput: videoOut
        onErrorOccurred: function(error, errorString) {
            console.warn("MediaPlayer error:", error, errorString)
        }
    }

    function stopPlayer() {
        player.stop()
        player.source = ""
    }

    function loadPlayer() {
        if (!root.showPlayer) {
            stopPlayer()
            return
        }
        const url = fs.preview.mediaUrl
        if (player.source.toString() === url && player.mediaStatus !== MediaPlayer.NoMedia)
            return
        player.stop()
        player.source = url
        // Decode first frame so the surface is not left black before Play.
        Qt.callLater(function() {
            if (player.source.toString() === url)
                player.play()
        })
    }

    Connections {
        target: fs.preview
        function onPreviewChanged() { root.loadPlayer() }
    }

    Component.onCompleted: root.loadPlayer()
    Component.onDestruction: root.stopPlayer()

    Rectangle {
        anchors.fill: parent
        color: theme.background

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
                    text: (i18n.revision, i18n.t("ui.preview", [fs.preview.kind || ""]))
                    color: theme.foreground
                    font.pixelSize: theme.scaled(11)
                    font.family: "iA Writer Mono S"
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }

            // Video surface outside Flickable. Keep the item in the scene graph
            // (height 0 when unused) so the video sink stays valid.
            Item {
                id: videoSurface
                Layout.fillWidth: true
                Layout.preferredHeight: root.showVideo ? Math.max(theme.scaled(200), root.height * 0.42) : 0
                Layout.maximumHeight: root.showVideo ? root.height * 0.6 : 0
                opacity: root.showVideo ? 1 : 0
                enabled: root.showVideo
                clip: false

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: root.showVideo ? theme.scaled(8) : 0
                    color: "#101010"

                    VideoOutput {
                        id: videoOut
                        anchors.fill: parent
                        fillMode: VideoOutput.PreserveAspectFit
                        endOfStreamPolicy: VideoOutput.KeepLastFrame
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: root.showVideo && player.error !== MediaPlayer.NoError
                        text: player.errorString
                        color: "#ffaaaa"
                        font.pixelSize: theme.scaled(12)
                        wrapMode: Text.Wrap
                        width: parent.width - theme.scaled(24)
                        horizontalAlignment: Text.AlignHCenter
                        z: 2
                    }
                }

                onHeightChanged: {
                    // Re-bind sink after the surface gets a real size (first video).
                    if (height > 1 && player.videoOutput !== videoOut)
                        player.videoOutput = videoOut
                }
            }

            // Optional PDF preview (QtQuick.Pdf / qt6-webengine). Falls back to metadata.
            Loader {
                id: pdfLoader
                Layout.fillWidth: true
                Layout.preferredHeight: (root.showPdf && root.pdfLoaderOk)
                                        ? Math.max(theme.scaled(220), root.height * 0.45) : 0
                Layout.maximumHeight: root.showPdf ? root.height * 0.65 : 0
                active: root.showPdf
                source: root.showPdf ? Qt.resolvedUrl("PdfPreview.qml") : ""
                onLoaded: {
                    if (item)
                        item.source = fs.preview.mediaUrl
                }

                Connections {
                    target: fs.preview
                    enabled: root.showPdf && pdfLoader.status === Loader.Ready
                    function onPreviewChanged() {
                        if (pdfLoader.item)
                            pdfLoader.item.source = fs.preview.mediaUrl
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: theme.scaled(10)
                Layout.rightMargin: theme.scaled(10)
                visible: root.pdfUnavailable
                text: (i18n.revision, i18n.t("ui.preview_pdf_unavailable"))
                color: theme.mutedForeground
                font.pixelSize: theme.scaled(12)
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: theme.scaled(10)
                Layout.rightMargin: theme.scaled(10)
                Layout.topMargin: root.showPlayer ? theme.scaled(4) : 0
                Layout.preferredHeight: root.showPlayer ? theme.scaled(36) : 0
                visible: root.showPlayer
                spacing: theme.scaled(8)

                Button {
                    text: player.playbackState === MediaPlayer.PlayingState
                          ? (i18n.revision, i18n.t("ui.preview_pause"))
                          : (i18n.revision, i18n.t("ui.preview_play"))
                    onClicked: {
                        if (player.playbackState === MediaPlayer.PlayingState)
                            player.pause()
                        else
                            player.play()
                    }
                }
                Button {
                    text: (i18n.revision, i18n.t("ui.preview_stop"))
                    onClicked: {
                        player.stop()
                        // After stop, seek+play briefly can restore a frame on some backends.
                        if (root.showVideo) {
                            player.play()
                            player.pause()
                        }
                    }
                }
                Slider {
                    Layout.fillWidth: true
                    from: 0
                    to: Math.max(1, player.duration)
                    value: player.position
                    enabled: player.duration > 0
                    onMoved: player.position = value
                }
            }

            Flickable {
                id: flick
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: width
                contentHeight: contentCol.implicitHeight
                clip: true
                ScrollBar.vertical: ScrollBar { }
                ScrollBar.horizontal: ScrollBar { }

                Column {
                    id: contentCol
                    width: flick.width
                    padding: theme.scaled(10)
                    spacing: theme.scaled(10)

                    Image {
                        visible: root.showImage
                        source: root.showImage ? fs.preview.imagePath : ""
                        width: Math.min(parent.width - theme.scaled(20),
                                        implicitWidth > 0 ? implicitWidth : parent.width - theme.scaled(20))
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }

                    Label {
                        visible: root.showAudio && player.error === MediaPlayer.NoError
                        width: parent.width - theme.scaled(20)
                        text: (i18n.revision, i18n.t("ui.preview_audio_hint"))
                        color: theme.mutedForeground
                        font.pixelSize: theme.scaled(12)
                        font.family: "iA Writer Mono S"
                    }

                    Label {
                        visible: player.error !== MediaPlayer.NoError && root.showAudio
                        width: parent.width - theme.scaled(20)
                        text: player.errorString
                        color: theme.mutedForeground
                        font.pixelSize: theme.scaled(12)
                        wrapMode: Text.Wrap
                    }

                    Text {
                        width: parent.width - theme.scaled(20)
                        text: fs.preview.text
                        color: theme.foreground
                        wrapMode: Text.WrapAnywhere
                        font.family: "iA Writer Mono S"
                        font.pixelSize: theme.scaled(12)
                        textFormat: Text.PlainText
                    }

                    Label {
                        visible: fs.preview.truncated
                        width: parent.width - theme.scaled(20)
                        text: (i18n.revision, i18n.t("ui.preview_truncated"))
                        color: theme.mutedForeground
                        font.italic: true
                        font.pixelSize: theme.scaled(11)
                    }
                }
            }
        }
    }
}
