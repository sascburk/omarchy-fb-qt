#include "appconfig.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QStandardPaths>

namespace {
const QStringList kSortModes{QStringLiteral("name"), QStringLiteral("mtime"),
                             QStringLiteral("size")};
}

AppConfig::AppConfig(QObject *parent)
    : QObject(parent)
    , m_settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("omarchy-fb-qt"),
                 QStringLiteral("config"))
{
    load();
}

void AppConfig::setStartPath(const QString &path)
{
    const QString resolved = resolveExistingDir(path);
    if (m_startPath == resolved)
        return;
    m_startPath = resolved;
    save();
    emit configChanged();
}

void AppConfig::setShowHidden(bool show)
{
    if (m_showHidden == show)
        return;
    m_showHidden = show;
    save();
    emit configChanged();
}

void AppConfig::setPreviewMaxBytes(int bytes)
{
    bytes = qBound(1024, bytes, 10 * 1024 * 1024);
    if (m_previewMaxBytes == bytes)
        return;
    m_previewMaxBytes = bytes;
    save();
    emit configChanged();
}

void AppConfig::setUseTrash(bool use)
{
    if (m_useTrash == use)
        return;
    m_useTrash = use;
    save();
    emit configChanged();
}

void AppConfig::setSortBy(const QString &sortBy)
{
    const QString normalized = sortBy.trimmed().toLower();
    if (!kSortModes.contains(normalized) || m_sortBy == normalized)
        return;
    m_sortBy = normalized;
    save();
    emit configChanged();
}

void AppConfig::setLanguage(const QString &language)
{
    const QString normalized = language.trimmed().toLower();
    if (normalized.isEmpty() || m_language == normalized)
        return;
    m_language = normalized;
    save();
    emit configChanged();
}

void AppConfig::cycleSortBy()
{
    const int idx = kSortModes.indexOf(m_sortBy);
    setSortBy(kSortModes[(idx + 1) % kSortModes.size()]);
}

bool AppConfig::addBookmark(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists())
        return false;
    const QString resolved = info.canonicalFilePath();
    if (resolved.isEmpty() || m_bookmarks.contains(resolved))
        return false;
    m_bookmarks.append(resolved);
    save();
    emit configChanged();
    return true;
}

bool AppConfig::removeBookmark(const QString &path)
{
    const QString resolved = QFileInfo(path).canonicalFilePath();
    const int before = m_bookmarks.size();
    m_bookmarks.removeAll(resolved);
    m_bookmarks.removeAll(path);
    if (m_bookmarks.size() == before)
        return false;
    save();
    emit configChanged();
    return true;
}

bool AppConfig::isBookmark(const QString &path) const
{
    const QFileInfo info(path);
    const QString resolved = info.exists() ? info.canonicalFilePath() : QFileInfo(path).absoluteFilePath();
    if (resolved.isEmpty())
        return false;
    if (m_bookmarks.contains(resolved) || m_bookmarks.contains(path))
        return true;
    for (const QString &bookmark : m_bookmarks) {
        if (QFileInfo(bookmark).canonicalFilePath() == resolved)
            return true;
    }
    return false;
}

bool AppConfig::toggleBookmark(const QString &path)
{
    if (isBookmark(path)) {
        removeBookmark(path);
        return false; // removed
    }
    addBookmark(path);
    return true; // added
}

QString AppConfig::openWithHandler(const QString &mime) const
{
    return m_openWith.value(mime.trimmed().toLower());
}

void AppConfig::setOpenWithHandler(const QString &mime, const QString &desktopId)
{
    const QString key = mime.trimmed().toLower();
    const QString id = desktopId.trimmed();
    if (key.isEmpty() || id.isEmpty())
        return;
    if (m_openWith.value(key) == id)
        return;
    m_openWith.insert(key, id);
    save();
    emit configChanged();
}

void AppConfig::save()
{
    m_settings.beginGroup(QStringLiteral("browser"));
    m_settings.setValue(QStringLiteral("start_path"), m_startPath);
    m_settings.setValue(QStringLiteral("show_hidden"), m_showHidden);
    m_settings.setValue(QStringLiteral("preview_max_bytes"), m_previewMaxBytes);
    m_settings.setValue(QStringLiteral("use_trash"), m_useTrash);
    m_settings.setValue(QStringLiteral("sort_by"), m_sortBy);
    m_settings.setValue(QStringLiteral("language"), m_language);
    m_settings.setValue(QStringLiteral("bookmarks"), m_bookmarks.join(QLatin1Char(',')));
    m_settings.endGroup();

    m_settings.beginGroup(QStringLiteral("open_with"));
    m_settings.remove(QString()); // clear group
    for (auto it = m_openWith.constBegin(); it != m_openWith.constEnd(); ++it)
        m_settings.setValue(it.key(), it.value());
    m_settings.endGroup();
    m_settings.sync();
}

void AppConfig::reload()
{
    load();
    emit configChanged();
}

void AppConfig::load()
{
    m_settings.beginGroup(QStringLiteral("browser"));
    m_startPath = resolveExistingDir(
        m_settings.value(QStringLiteral("start_path"), QDir::homePath()).toString());
    m_showHidden = m_settings.value(QStringLiteral("show_hidden"), false).toBool();
    m_previewMaxBytes = m_settings.value(QStringLiteral("preview_max_bytes"), 65536).toInt();
    m_useTrash = m_settings.value(QStringLiteral("use_trash"), true).toBool();
    m_sortBy = m_settings.value(QStringLiteral("sort_by"), QStringLiteral("name")).toString().toLower();
    if (!kSortModes.contains(m_sortBy))
        m_sortBy = QStringLiteral("name");
    m_language = m_settings.value(QStringLiteral("language"), QStringLiteral("auto")).toString().toLower();
    if (m_language.isEmpty())
        m_language = QStringLiteral("auto");

    m_bookmarks.clear();
    const QStringList raw =
        m_settings.value(QStringLiteral("bookmarks")).toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &part : raw) {
        const QFileInfo info(QDir::cleanPath(part.trimmed()));
        if (info.exists())
            m_bookmarks.append(info.canonicalFilePath());
    }
    m_settings.endGroup();

    m_openWith.clear();
    m_settings.beginGroup(QStringLiteral("open_with"));
    const QStringList keys = m_settings.childKeys();
    for (const QString &key : keys) {
        const QString value = m_settings.value(key).toString().trimmed();
        if (!key.isEmpty() && !value.isEmpty())
            m_openWith.insert(key.toLower(), value);
    }
    m_settings.endGroup();
}

QString AppConfig::resolveExistingDir(const QString &candidate) const
{
    QFileInfo info(QDir::cleanPath(candidate.isEmpty() ? QDir::homePath() : candidate));
    if (!info.exists() || !info.isDir())
        info.setFile(QDir::homePath());
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? QDir::homePath() : canonical;
}
