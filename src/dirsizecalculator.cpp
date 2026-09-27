#include "dirsizecalculator.h"

#include "i18n.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent>

DirSizeCalculator::DirSizeCalculator(I18n *i18n, QObject *parent)
    : QObject(parent)
    , m_i18n(i18n)
{
}

bool DirSizeCalculator::getCached(const QString &path, qint64 *size, int *files, int *dirs) const
{
    const auto it = m_cache.constFind(path);
    if (it == m_cache.cend())
        return false;

    const QFileInfo info(path);
    if (!info.exists() || info.lastModified().toSecsSinceEpoch() != it->mtime)
        return false;

    if (size)
        *size = it->size;
    if (files)
        *files = it->files;
    if (dirs)
        *dirs = it->dirs;
    return true;
}

void DirSizeCalculator::invalidate(const QString &path)
{
    m_cache.remove(path);
    QFileInfo info(path);
    QString current = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    while (!current.isEmpty() && current != QLatin1String("/")) {
        m_cache.remove(current);
        const QString parent = QFileInfo(current).absolutePath();
        if (parent == current)
            break;
        current = parent;
    }
}

void DirSizeCalculator::compute(const QString &path)
{
    if (m_busy)
        return;

    QFileInfo info(path);
    if (!info.exists() || !info.isDir()) {
        emit failed(path, m_i18n ? m_i18n->t(QStringLiteral("error.size_dirs_only"))
                                 : QStringLiteral("Size calculation is for folders only."));
        return;
    }

    m_busy = true;
    emit busyChanged();

    const QString target = info.absoluteFilePath();
    auto *watcher = new QFutureWatcher<CacheEntry>(this);
    connect(watcher, &QFutureWatcher<CacheEntry>::finished, this, [this, watcher, target]() {
        const CacheEntry entry = watcher->result();
        watcher->deleteLater();
        m_cache.insert(target, entry);
        m_busy = false;
        emit busyChanged();
        emit computed(target, entry.size, entry.files, entry.dirs);
    });

    watcher->setFuture(QtConcurrent::run([target]() {
        CacheEntry entry;
        QFileInfo root(target);
        entry.mtime = root.lastModified().toSecsSinceEpoch();

        QDirIterator it(target,
                        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden
                            | QDir::System,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QFileInfo fi = it.fileInfo();
            if (fi.isDir()) {
                ++entry.dirs;
                continue;
            }
            if (fi.isSymLink())
                continue;
            entry.size += fi.size();
            ++entry.files;
        }
        return entry;
    }));
}
