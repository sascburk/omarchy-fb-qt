#include "previewprovider.h"

#include "i18n.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMimeDatabase>
#include <QUrl>
#include <QVariantMap>
#include <QtConcurrent>

namespace {
const QStringList kImageSuffixes{QStringLiteral("png"),  QStringLiteral("jpg"),
                                 QStringLiteral("jpeg"), QStringLiteral("gif"),
                                 QStringLiteral("bmp"),  QStringLiteral("webp"),
                                 QStringLiteral("svg"),  QStringLiteral("ico")};

const QStringList kVideoSuffixes{QStringLiteral("mp4"),  QStringLiteral("mkv"),
                                 QStringLiteral("webm"), QStringLiteral("avi"),
                                 QStringLiteral("mov"),  QStringLiteral("m4v"),
                                 QStringLiteral("wmv"),  QStringLiteral("mpg"),
                                 QStringLiteral("mpeg"), QStringLiteral("ts"),
                                 QStringLiteral("m2ts"), QStringLiteral("flv"),
                                 QStringLiteral("ogv")};

const QStringList kAudioSuffixes{QStringLiteral("mp3"),  QStringLiteral("flac"),
                                 QStringLiteral("ogg"),  QStringLiteral("opus"),
                                 QStringLiteral("wav"),  QStringLiteral("m4a"),
                                 QStringLiteral("aac"),  QStringLiteral("wma"),
                                 QStringLiteral("ape"),  QStringLiteral("aiff")};

QString trCat(const QHash<QString, QString> &cat, const QString &key, const QStringList &args = {})
{
    QString text = cat.value(key, key);
    for (const QString &arg : args)
        text = text.arg(arg);
    return text;
}

QString formatBytes(qint64 bytes)
{
    constexpr qint64 kKiB = 1024;
    constexpr qint64 kMiB = 1024 * kKiB;
    constexpr qint64 kGiB = 1024 * kMiB;
    if (bytes >= kGiB)
        return QStringLiteral("%1 GiB").arg(bytes / double(kGiB), 0, 'f', 2);
    if (bytes >= kMiB)
        return QStringLiteral("%1 MiB").arg(bytes / double(kMiB), 0, 'f', 2);
    if (bytes >= kKiB)
        return QStringLiteral("%1 KiB").arg(bytes / double(kKiB), 0, 'f', 1);
    return QStringLiteral("%1 B").arg(bytes);
}
} // namespace

PreviewProvider::PreviewProvider(I18n *i18n, QObject *parent)
    : QObject(parent)
    , m_i18n(i18n)
{
}

void PreviewProvider::setMaxBytes(int bytes)
{
    m_maxBytes = qBound(1024, bytes, 10 * 1024 * 1024);
}

void PreviewProvider::clear()
{
    ++m_token;
    m_text.clear();
    m_kind.clear();
    m_imagePath.clear();
    m_mediaUrl.clear();
    m_truncated = false;
    m_path.clear();
    emit previewChanged();
}

void PreviewProvider::load(const QString &path)
{
    const int token = ++m_token;
    m_path = path;
    m_text = m_i18n ? m_i18n->t(QStringLiteral("preview.loading")) : QStringLiteral("Loading…");
    m_kind = QStringLiteral("loading");
    m_imagePath.clear();
    m_mediaUrl.clear();
    m_truncated = false;
    emit previewChanged();

    const int maxBytes = m_maxBytes;
    const QHash<QString, QString> catalog = m_i18n ? m_i18n->catalog() : QHash<QString, QString>{};
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, token, path]() {
        const QVariantMap result = watcher->result();
        watcher->deleteLater();
        if (token != m_token || path != m_path)
            return;
        m_text = result.value(QStringLiteral("text")).toString();
        m_kind = result.value(QStringLiteral("kind")).toString();
        m_imagePath = result.value(QStringLiteral("imagePath")).toString();
        m_mediaUrl = result.value(QStringLiteral("mediaUrl")).toString();
        m_truncated = result.value(QStringLiteral("truncated")).toBool();
        emit previewChanged();
    });

    watcher->setFuture(QtConcurrent::run([path, maxBytes, catalog]() {
        QVariantMap out;
        out.insert(QStringLiteral("truncated"), false);
        out.insert(QStringLiteral("imagePath"), QString());
        out.insert(QStringLiteral("mediaUrl"), QString());

        QFileInfo info(path);
        if (!info.exists()) {
            out.insert(QStringLiteral("kind"), QStringLiteral("error"));
            out.insert(QStringLiteral("text"), trCat(catalog, QStringLiteral("preview.not_found")));
            return out;
        }

        QMimeDatabase db;
        const QMimeType mime = db.mimeTypeForFile(info);
        const QString mimeName = mime.name();
        const QString modified =
            info.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        const QString sizeText = formatBytes(info.size());

        if (info.isDir()) {
            const int entries =
                QDir(path).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden).size();
            out.insert(QStringLiteral("kind"), QStringLiteral("directory"));
            out.insert(QStringLiteral("text"),
                       trCat(catalog, QStringLiteral("preview.dir"),
                             {path, QString::number(entries), sizeText, modified}));
            return out;
        }

        const QString suffix = info.suffix().toLower();
        const bool isImage = kImageSuffixes.contains(suffix) || mimeName.startsWith(QLatin1String("image/"));
        const bool isVideo = kVideoSuffixes.contains(suffix) || mimeName.startsWith(QLatin1String("video/"));
        const bool isAudio = kAudioSuffixes.contains(suffix) || mimeName.startsWith(QLatin1String("audio/"));

        if (isImage && !isVideo) {
            out.insert(QStringLiteral("kind"), QStringLiteral("image"));
            out.insert(QStringLiteral("imagePath"), QUrl::fromLocalFile(path).toString());
            out.insert(QStringLiteral("text"),
                       trCat(catalog, QStringLiteral("preview.image"),
                             {path, mimeName, sizeText, modified}));
            return out;
        }

        if (isVideo) {
            out.insert(QStringLiteral("kind"), QStringLiteral("video"));
            out.insert(QStringLiteral("mediaUrl"), QUrl::fromLocalFile(path).toString());
            out.insert(QStringLiteral("text"),
                       trCat(catalog, QStringLiteral("preview.video"),
                             {path, mimeName, sizeText, modified}));
            return out;
        }

        if (isAudio) {
            out.insert(QStringLiteral("kind"), QStringLiteral("audio"));
            out.insert(QStringLiteral("mediaUrl"), QUrl::fromLocalFile(path).toString());
            out.insert(QStringLiteral("text"),
                       trCat(catalog, QStringLiteral("preview.audio"),
                             {path, mimeName, sizeText, modified}));
            return out;
        }

        const bool isPdf =
            suffix == QLatin1String("pdf") || mimeName == QLatin1String("application/pdf");
        if (isPdf) {
            out.insert(QStringLiteral("kind"), QStringLiteral("pdf"));
            out.insert(QStringLiteral("mediaUrl"), QUrl::fromLocalFile(path).toString());
            out.insert(QStringLiteral("text"),
                       trCat(catalog, QStringLiteral("preview.pdf"),
                             {path, mimeName, sizeText, modified}));
            return out;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            out.insert(QStringLiteral("kind"), QStringLiteral("error"));
            out.insert(QStringLiteral("text"),
                       trCat(catalog, QStringLiteral("preview.no_read"), {info.fileName()}));
            return out;
        }

        const QByteArray probe = file.read(qMax(1024, 512));
        if (probe.contains('\0')) {
            // Binary / non-textual: metadata only, no hex dump.
            out.insert(QStringLiteral("kind"), QStringLiteral("binary"));
            out.insert(QStringLiteral("text"),
                       trCat(catalog, QStringLiteral("preview.binary"),
                             {path, mimeName, sizeText, modified}));
            return out;
        }

        file.seek(0);
        const QByteArray content = file.read(maxBytes);
        const bool truncated = file.read(1).size() > 0;
        const QString text = QString::fromUtf8(content);
        out.insert(QStringLiteral("kind"), QStringLiteral("text"));
        out.insert(QStringLiteral("truncated"), truncated);
        out.insert(QStringLiteral("text"),
                   trCat(catalog, QStringLiteral("preview.text"), {mimeName, text}));
        return out;
    }));
}
