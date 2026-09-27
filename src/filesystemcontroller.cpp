#include "filesystemcontroller.h"

#include "appconfig.h"
#include "dirsizecalculator.h"
#include "fileoperations.h"
#include "previewprovider.h"
#include "i18n.h"

#include <QDir>
#include <QFileInfo>
#include <algorithm>

DirectoryListModel::DirectoryListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DirectoryListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant DirectoryListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const FileEntry &entry = m_entries.at(index.row());
    switch (role) {
    case NameRole:
        return entry.name;
    case PathRole:
        return entry.path;
    case IsDirRole:
        return entry.isDir;
    case IsSymLinkRole:
        return entry.isSymLink;
    case IsHiddenRole:
        return entry.isHidden;
    case SizeRole:
        return entry.size;
    case MTimeRole:
        return entry.mtime;
    case MarkedRole:
        return m_marked.contains(entry.path);
    case DisplaySizeRole:
        return entry.isDir ? QStringLiteral("<DIR>") : FileOperations::humanSize(entry.size);
    default:
        return {};
    }
}

QHash<int, QByteArray> DirectoryListModel::roleNames() const
{
    return {{NameRole, "name"},
            {PathRole, "path"},
            {IsDirRole, "isDir"},
            {IsSymLinkRole, "isSymLink"},
            {IsHiddenRole, "isHidden"},
            {SizeRole, "size"},
            {MTimeRole, "mtime"},
            {MarkedRole, "marked"},
            {DisplaySizeRole, "displaySize"}};
}

void DirectoryListModel::setEntries(QVector<FileEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

void DirectoryListModel::setMarked(const QSet<QString> &marked)
{
    m_marked = marked;
    if (!m_entries.isEmpty())
        emit dataChanged(index(0), index(m_entries.size() - 1), {MarkedRole});
}

QString DirectoryListModel::pathAt(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return {};
    return m_entries.at(row).path;
}

bool DirectoryListModel::isDirAt(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return false;
    return m_entries.at(row).isDir;
}

FileSystemController::FileSystemController(AppConfig *config, FileOperations *ops,
                                           DirSizeCalculator *sizes, PreviewProvider *preview,
                                           I18n *i18n, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_ops(ops)
    , m_sizes(sizes)
    , m_preview(preview)
    , m_i18n(i18n)
    , m_model(new DirectoryListModel(this))
{
    connect(m_sizes, &DirSizeCalculator::computed, this,
            [this](const QString &path, qint64 size, int files, int dirs) {
                if (path == m_selectedPath) {
                    refreshStatus();
                    notify(m_i18n->t(QStringLiteral("notify.size_result"),
                                   {QFileInfo(path).fileName(), FileOperations::humanSize(size),
                                    QString::number(files), QString::number(dirs)}));
                }
            });
    connect(m_sizes, &DirSizeCalculator::failed, this,
            [this](const QString &, const QString &error) { notify(error); });
}

QString FileSystemController::selectedName() const
{
    return QFileInfo(m_selectedPath).fileName();
}

bool FileSystemController::selectedIsDir() const
{
    return QFileInfo(m_selectedPath).isDir();
}

QString FileSystemController::clipboardSummary() const
{
    if (m_clipboardPaths.isEmpty())
        return {};
    if (m_clipboardPaths.size() == 1)
        return QStringLiteral("%1: %2")
            .arg(m_clipboardMode, QFileInfo(m_clipboardPaths.first()).fileName());
    return QStringLiteral("%1: %2").arg(m_clipboardMode).arg(m_clipboardPaths.size());
}

void FileSystemController::initialize(const QString &cliPath)
{
    m_showHidden = m_config->showHidden();
    m_sortBy = m_config->sortBy();
    m_preview->setMaxBytes(m_config->previewMaxBytes());

    QString warning;
    const QString start = resolveStartPath(cliPath, &warning);
    setCurrentPath(start);
    setSelectedPath(start);
    refreshListing(start);
    if (!warning.isEmpty())
        notify(warning);
}

void FileSystemController::selectIndex(int index)
{
    const QString path = m_model->pathAt(index);
    if (path.isEmpty())
        return;
    m_selectedIndex = index;
    setSelectedPath(path);
    refreshPreview();
    refreshStatus();
}

void FileSystemController::activateIndex(int index)
{
    selectIndex(index);
    const QString path = m_model->pathAt(index);
    if (path.isEmpty())
        return;

    const QFileInfo info(path);
    if (m_model->isDirAt(index) || info.isDir()) {
        const QString dir = info.absoluteFilePath();
        if (dir == QFileInfo(m_currentPath).absoluteFilePath())
            return;
        setCurrentPath(dir);
        refreshListing(dir);
        return;
    }

    openPath(path);
}

void FileSystemController::openPath(const QString &path)
{
    m_pendingOpenPath = path;
    const QFileInfo info(path);
    if (!info.exists() || info.isDir())
        return;

    const QString mime = m_ops->mimeForPath(path);
    const QString savedId = m_config->openWithHandler(mime);
    if (!savedId.isEmpty()) {
        const QString desktop = m_ops->desktopPathForId(savedId);
        QString error;
        if (!desktop.isEmpty() && m_ops->openWithDesktop(path, desktop, &error))
            return;
    }

    if (m_ops->shouldAskOpenWith(path)) {
        requestOpenWithFor(path);
        return;
    }

    QString error;
    if (!m_ops->openWithDefault(path, &error) && !error.isEmpty())
        notify(error);
}

void FileSystemController::goParent()
{
    // Flat list browser: always leave the directory we are currently listing.
    // (Selected child entries must not trap us in the same folder.)
    const QFileInfo current(m_currentPath);
    const QString here = current.absoluteFilePath();
    const QString parent = current.absolutePath();
    if (parent.isEmpty() || parent == here) {
        notify(m_i18n->t(QStringLiteral("notify.at_fs_root")));
        return;
    }

    setCurrentPath(parent);
    setSelectedPath(here); // Focus the folder we just left
    refreshListing(here);
}

void FileSystemController::goTo(const QString &path)
{
    QFileInfo info(QDir::cleanPath(path.trimmed()));
    if (!info.exists()) {
        notify(m_i18n->t(QStringLiteral("notify.not_exists"), {path}));
        return;
    }
    const QString dest = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    setCurrentPath(dest);
    setSelectedPath(info.absoluteFilePath());
    refreshListing(info.absoluteFilePath());
    notify(m_i18n->t(QStringLiteral("notify.goto"), {info.absoluteFilePath()}));
}

void FileSystemController::reload()
{
    refreshListing(m_selectedPath);
    notify(m_i18n->t(QStringLiteral("notify.reloaded")));
}

void FileSystemController::setShowHidden(bool show)
{
    if (m_showHidden == show)
        return;
    m_showHidden = show;
    m_config->setShowHidden(show);
    emit showHiddenChanged();
    refreshListing(m_selectedPath);
    notify(m_i18n->t(QStringLiteral("notify.hidden"),
                 {show ? m_i18n->t(QStringLiteral("notify.hidden_on"))
                       : m_i18n->t(QStringLiteral("notify.hidden_off"))}));
}

void FileSystemController::setNameFilter(const QString &filter)
{
    if (m_nameFilter == filter)
        return;
    m_nameFilter = filter;
    emit nameFilterChanged();
    refreshListing(m_selectedPath);
    if (!filter.isEmpty())
        notify(m_i18n->t(QStringLiteral("notify.filter"), {filter}));
}

void FileSystemController::setSortBy(const QString &sortBy)
{
    if (m_sortBy == sortBy)
        return;
    m_sortBy = sortBy;
    m_config->setSortBy(sortBy);
    emit sortByChanged();
    refreshListing(m_selectedPath);
}

void FileSystemController::cycleSort()
{
    m_config->cycleSortBy();
    m_sortBy = m_config->sortBy();
    emit sortByChanged();
    refreshListing(m_selectedPath);
    notify(m_i18n->t(QStringLiteral("notify.sort"), {m_sortBy}));
}

void FileSystemController::cycleLanguage()
{
    m_i18n->cycleLanguage();
    m_config->setLanguage(m_i18n->language());
    notify(m_i18n->t(QStringLiteral("notify.language"), {m_i18n->languageName()}));
    refreshStatus();
    refreshPreview();
}

void FileSystemController::toggleMark()
{
    if (m_selectedPath.isEmpty())
        return;
    if (m_marked.contains(m_selectedPath)) {
        m_marked.remove(m_selectedPath);
        notify(m_i18n->t(QStringLiteral("notify.unmarked"), {selectedName()}));
    } else {
        m_marked.insert(m_selectedPath);
        notify(m_i18n->t(QStringLiteral("notify.marked"), {selectedName(), QString::number(m_marked.size())}));
    }
    m_model->setMarked(m_marked);
    emit markedChanged();
    refreshStatus();
}

void FileSystemController::markAll()
{
    for (int i = 0; i < m_model->rowCount(); ++i)
        m_marked.insert(m_model->pathAt(i));
    m_model->setMarked(m_marked);
    emit markedChanged();
    refreshStatus();
    notify(m_i18n->t(QStringLiteral("notify.marked_all"), {QString::number(m_marked.size())}));
}

void FileSystemController::clearMarksAndFilter()
{
    bool cleared = false;
    if (!m_nameFilter.isEmpty()) {
        m_nameFilter.clear();
        emit nameFilterChanged();
        refreshListing(m_selectedPath);
        cleared = true;
        notify(m_i18n->t(QStringLiteral("notify.filter_cleared")));
    }
    if (!m_marked.isEmpty()) {
        m_marked.clear();
        m_model->setMarked(m_marked);
        emit markedChanged();
        refreshStatus();
        cleared = true;
        notify(m_i18n->t(QStringLiteral("notify.marks_cleared")));
    }
    Q_UNUSED(cleared);
}

void FileSystemController::copySelected()
{
    m_clipboardPaths = targetPaths();
    m_clipboardMode = QStringLiteral("copy");
    emit clipboardChanged();
    refreshStatus();
    notify(m_i18n->t(QStringLiteral("notify.copy_staged"), {QString::number(m_clipboardPaths.size())}));
}

void FileSystemController::cutSelected()
{
    m_clipboardPaths = targetPaths();
    m_clipboardMode = QStringLiteral("cut");
    emit clipboardChanged();
    refreshStatus();
    notify(m_i18n->t(QStringLiteral("notify.cut_staged"), {QString::number(m_clipboardPaths.size())}));
}

void FileSystemController::paste()
{
    if (m_clipboardPaths.isEmpty()) {
        notify(m_i18n->t(QStringLiteral("notify.clipboard_empty")));
        return;
    }

    QString error;
    QStringList pending;
    m_ops->pastePaths(m_clipboardPaths, m_clipboardMode, targetDir(), false, &pending, &error);
    if (!pending.isEmpty()) {
        emit askConfirm(
            m_i18n->t(QStringLiteral("notify.overwrite_ask"),
                     {QFileInfo(pending.first()).fileName()}),
            QStringLiteral("pasteOverwrite"), true,
            m_i18n->t(QStringLiteral("dialog.overwrite")));
        m_clipboardPaths = pending;
        return;
    }
    if (!error.isEmpty())
        notify(error);
    if (m_clipboardMode == QLatin1String("cut")) {
        m_clipboardPaths.clear();
        m_clipboardMode.clear();
        emit clipboardChanged();
    }
    m_sizes->invalidate(targetDir());
    refreshListing(targetDir());
    notify(m_i18n->t(QStringLiteral("notify.paste_done")));
}

void FileSystemController::openSelected()
{
    if (m_selectedIndex >= 0 && m_selectedIndex < m_model->rowCount()) {
        activateIndex(m_selectedIndex);
        return;
    }
    if (m_selectedPath.isEmpty())
        return;

    const QFileInfo info(m_selectedPath);
    if (info.isDir()) {
        const QString dir = info.absoluteFilePath();
        if (dir == QFileInfo(m_currentPath).absoluteFilePath())
            return;
        setCurrentPath(dir);
        refreshListing(dir);
        return;
    }

    QString error;
    openPath(m_selectedPath);
}

void FileSystemController::requestOpenWith()
{
    if (m_selectedPath.isEmpty() || selectedIsDir()) {
        notify(m_i18n->t(QStringLiteral("notify.open_with_files_only")));
        return;
    }
    requestOpenWithFor(m_selectedPath);
}

void FileSystemController::requestOpenWithFor(const QString &path)
{
    m_pendingOpenPath = path;
    QVariantList apps = m_ops->openWithCandidates(path);
    if (apps.isEmpty()) {
        notify(m_i18n->t(QStringLiteral("notify.open_with_none")));
        return;
    }

    // Put remembered handler first.
    const QString savedId = m_config->openWithHandler(m_ops->mimeForPath(path));
    if (!savedId.isEmpty()) {
        const QString want = savedId.endsWith(QLatin1String(".desktop"))
            ? savedId
            : savedId + QStringLiteral(".desktop");
        for (int i = 0; i < apps.size(); ++i) {
            const QVariantMap app = apps.at(i).toMap();
            if (app.value(QStringLiteral("id")).toString() == want) {
                QVariantMap marked = app;
                marked.insert(QStringLiteral("recommended"), true);
                apps.removeAt(i);
                apps.prepend(marked);
                break;
            }
        }
    }

    emit askOpenWith(path, m_ops->mimeForPath(path), apps);
}

void FileSystemController::openWithDesktop(const QString &desktopPath)
{
    const QString path = m_pendingOpenPath.isEmpty() ? m_selectedPath : m_pendingOpenPath;
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        notify(m_i18n->t(QStringLiteral("notify.not_exists"), {path}));
        return;
    }

    QString error;
    if (!m_ops->openWithDesktop(path, desktopPath, &error)) {
        if (!error.isEmpty())
            notify(error);
        return;
    }

    // Remember choice per MIME type for next time.
    m_config->setOpenWithHandler(m_ops->mimeForPath(path), QFileInfo(desktopPath).fileName());
    notify(m_i18n->t(QStringLiteral("notify.open_with_saved"),
                     {m_ops->mimeForPath(path), QFileInfo(desktopPath).completeBaseName()}));
}

void FileSystemController::editSelected()
{
    QString error;
    if (!m_ops->openWithEditor(m_selectedPath, &error) && !error.isEmpty())
        notify(error);
}

void FileSystemController::requestRename()
{
    emit askTextInput(m_i18n->t(QStringLiteral("prompt.rename")), selectedName(), QStringLiteral("rename"));
}

void FileSystemController::confirmRename(const QString &newName)
{
    if (newName.isEmpty() || newName == selectedName())
        return;
    QString error;
    const QString target =
        m_ops->safeJoin(QFileInfo(m_selectedPath).absolutePath(), newName, &error);
    if (target.isEmpty()) {
        notify(error);
        return;
    }
    if (QFileInfo::exists(target)) {
        emit askConfirm(m_i18n->t(QStringLiteral("notify.overwrite_ask"), {newName}),
                        QStringLiteral("renameOverwrite:") + newName, true,
                        m_i18n->t(QStringLiteral("dialog.overwrite")));
        return;
    }
    if (!m_ops->renamePath(m_selectedPath, newName, false, &error)) {
        notify(error);
        return;
    }
    m_sizes->invalidate(QFileInfo(m_selectedPath).absolutePath());
    m_marked.remove(m_selectedPath);
    const QString newPath =
        m_ops->safeJoin(QFileInfo(m_selectedPath).absolutePath(), newName, nullptr);
    setSelectedPath(newPath);
    refreshListing(newPath);
    notify(m_i18n->t(QStringLiteral("notify.renamed"), {newName}));
}

void FileSystemController::requestDelete(bool permanent)
{
    const QStringList paths = targetPaths();
    if (paths.isEmpty())
        return;

    QString msg;
    const QString action = permanent ? m_i18n->t(QStringLiteral("delete.action_permanent"))
                                     : m_i18n->t(QStringLiteral("delete.action_trash"));
    if (paths.size() == 1) {
        const QFileInfo info(paths.first());
        msg = m_i18n->t(QStringLiteral("delete.ask_one"),
                        {info.isDir() ? m_i18n->t(QStringLiteral("delete.folder"))
                                      : m_i18n->t(QStringLiteral("delete.file")),
                         info.fileName(), action});
    } else {
        msg = m_i18n->t(QStringLiteral("delete.ask_many"),
                        {QString::number(paths.size()), action});
    }

    if (permanent) {
        emit askConfirm(msg, QStringLiteral("deletePermanent"), true,
                        m_i18n->t(QStringLiteral("dialog.delete_permanent")));
        return;
    }

    if (m_config->useTrash() && FileOperations::trashAvailable()) {
        emit askChoice(msg, QStringLiteral("deleteChoice"));
        return;
    }

    emit askConfirm(msg, QStringLiteral("deleteHard"), true, m_i18n->t(QStringLiteral("dialog.delete")));
}

void FileSystemController::confirmDelete(bool permanent, bool useTrash)
{
    QString error;
    const QStringList paths = targetPaths();
    if (!m_ops->deletePaths(paths, permanent, useTrash && m_config->useTrash(), &error)) {
        if (!error.isEmpty())
            notify(error);
        return;
    }
    for (const QString &path : paths) {
        m_sizes->invalidate(path);
        m_marked.remove(path);
    }
    const QString parent = QFileInfo(paths.first()).absolutePath();
    setSelectedPath(parent);
    setCurrentPath(parent);
    refreshListing(parent);
    notify(permanent ? m_i18n->t(QStringLiteral("notify.deleted_permanent"))
                     : m_i18n->t(QStringLiteral("notify.deleted_trash")));
}

void FileSystemController::requestNewFile()
{
    emit askTextInput(m_i18n->t(QStringLiteral("prompt.new_file")), QString(), QStringLiteral("newFile"));
}

void FileSystemController::confirmNewFile(const QString &name)
{
    if (name.isEmpty())
        return;
    QString error;
    if (!m_ops->createFile(targetDir(), name, &error)) {
        notify(error);
        return;
    }
    const QString path = m_ops->safeJoin(targetDir(), name, nullptr);
    m_sizes->invalidate(targetDir());
    setSelectedPath(path);
    refreshListing(path);
    notify(m_i18n->t(QStringLiteral("notify.file_created"), {name}));
}

void FileSystemController::requestNewFolder()
{
    emit askTextInput(m_i18n->t(QStringLiteral("prompt.new_folder")), QString(),
                      QStringLiteral("newFolder"));
}

void FileSystemController::confirmNewFolder(const QString &name)
{
    if (name.isEmpty())
        return;
    QString error;
    if (!m_ops->createFolder(targetDir(), name, &error)) {
        notify(error);
        return;
    }
    const QString path = m_ops->safeJoin(targetDir(), name, nullptr);
    m_sizes->invalidate(targetDir());
    setSelectedPath(path);
    refreshListing(path);
    notify(m_i18n->t(QStringLiteral("notify.folder_created"), {name}));
}

void FileSystemController::computeSelectedSize()
{
    if (!selectedIsDir()) {
        notify(m_i18n->t(QStringLiteral("notify.size_dirs_only")));
        return;
    }
    notify(m_i18n->t(QStringLiteral("notify.size_computing"), {selectedName()}));
    m_sizes->compute(m_selectedPath);
}

void FileSystemController::removeBookmark(const QString &path)
{
    if (m_config->removeBookmark(path))
        notify(m_i18n->t(QStringLiteral("notify.favorite_removed"), {path}));
}

void FileSystemController::toggleFavoriteSelected()
{
    // Favorites are folders: use selected dir, or parent of a selected file.
    QFileInfo info(m_selectedPath.isEmpty() ? m_currentPath : m_selectedPath);
    if (!info.exists()) {
        notify(m_i18n->t(QStringLiteral("notify.not_exists"), {m_selectedPath}));
        return;
    }
    const QString fav = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    const bool added = m_config->toggleBookmark(fav);
    notify(added ? m_i18n->t(QStringLiteral("notify.favorite_added"), {fav})
                 : m_i18n->t(QStringLiteral("notify.favorite_removed"), {fav}));
}

void FileSystemController::completeAction(const QString &action, const QString &value)
{
    if (action == QLatin1String("rename")) {
        confirmRename(value);
    } else if (action.startsWith(QLatin1String("renameOverwrite:"))) {
        const QString newName = action.section(QLatin1Char(':'), 1);
        QString error;
        if (!m_ops->renamePath(m_selectedPath, newName, true, &error)) {
            notify(error);
            return;
        }
        m_sizes->invalidate(QFileInfo(m_selectedPath).absolutePath());
        const QString newPath =
            m_ops->safeJoin(QFileInfo(m_selectedPath).absolutePath(), newName, nullptr);
        setSelectedPath(newPath);
        refreshListing(newPath);
        notify(m_i18n->t(QStringLiteral("notify.renamed"), {newName}));
    } else if (action == QLatin1String("newFile")) {
        confirmNewFile(value);
    } else if (action == QLatin1String("newFolder")) {
        confirmNewFolder(value);
    } else if (action == QLatin1String("goto")) {
        goTo(value);
    } else if (action == QLatin1String("filter")) {
        setNameFilter(value);
    } else if (action == QLatin1String("deletePermanent") || action == QLatin1String("deleteHard")) {
        confirmDelete(true, false);
    } else if (action == QLatin1String("deleteTrash")) {
        confirmDelete(false, true);
    } else if (action == QLatin1String("pasteOverwrite")) {
        QString error;
        QStringList pending;
        m_ops->pastePaths(m_clipboardPaths, m_clipboardMode, targetDir(), true, &pending, &error);
        if (!error.isEmpty())
            notify(error);
        if (m_clipboardMode == QLatin1String("cut")) {
            m_clipboardPaths.clear();
            m_clipboardMode.clear();
            emit clipboardChanged();
        }
        m_sizes->invalidate(targetDir());
        refreshListing(targetDir());
        notify(m_i18n->t(QStringLiteral("notify.paste_done")));
    }
}

QStringList FileSystemController::targetPaths() const
{
    if (!m_marked.isEmpty())
        return QStringList(m_marked.begin(), m_marked.end());
    if (!m_selectedPath.isEmpty())
        return {m_selectedPath};
    return {};
}

QString FileSystemController::targetDir() const
{
    QFileInfo info(m_selectedPath);
    return info.isDir() ? info.absoluteFilePath() : info.absolutePath();
}

void FileSystemController::setCurrentPath(const QString &path)
{
    if (m_currentPath == path)
        return;
    m_currentPath = path;
    emit currentPathChanged();
}

void FileSystemController::setSelectedPath(const QString &path)
{
    if (m_selectedPath == path)
        return;
    m_selectedPath = path;
    emit selectedPathChanged();
}

void FileSystemController::refreshListing(const QString &focusPath)
{
    m_model->setEntries(listDirectory(m_currentPath));
    m_model->setMarked(m_marked);

    const QString focus = focusPath.isEmpty() ? m_selectedPath : focusPath;
    int found = -1;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        if (m_model->pathAt(i) == focus) {
            found = i;
            break;
        }
    }
    if (found >= 0) {
        m_selectedIndex = found;
        setSelectedPath(focus);
    } else {
        m_selectedIndex = m_model->rowCount() > 0 ? 0 : -1;
        if (m_selectedIndex >= 0)
            setSelectedPath(m_model->pathAt(m_selectedIndex));
        else
            setSelectedPath(m_currentPath);
    }
    // Index may change without a path change — QML still needs the signal.
    emit selectedPathChanged();
    refreshPreview();
    refreshStatus();
}

void FileSystemController::refreshStatus()
{
    QStringList parts{m_selectedPath};
    if (!m_marked.isEmpty())
        parts << m_i18n->t(QStringLiteral("status.marked"), {QString::number(m_marked.size())});
    if (!m_clipboardPaths.isEmpty())
        parts << m_i18n->t(QStringLiteral("status.clipboard"), {clipboardSummary()});

    if (selectedIsDir()) {
        qint64 size = 0;
        int files = 0;
        int dirs = 0;
        if (m_sizes->getCached(m_selectedPath, &size, &files, &dirs)) {
            parts << m_i18n->t(QStringLiteral("status.dir_size"),
                             {FileOperations::humanSize(size), QString::number(files),
                              QString::number(dirs)});
        }
    } else if (QFileInfo::exists(m_selectedPath)) {
        parts << FileOperations::humanSize(QFileInfo(m_selectedPath).size());
    }

    m_statusText = parts.join(QStringLiteral("  |  "));
    emit statusTextChanged();
}

void FileSystemController::refreshPreview()
{
    m_preview->setMaxBytes(m_config->previewMaxBytes());
    m_preview->load(m_selectedPath);
}

void FileSystemController::notify(const QString &text)
{
    m_message = text;
    emit messageChanged();
}

QVector<FileEntry> FileSystemController::listDirectory(const QString &path) const
{
    QDir dir(path);
    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System;
    if (m_showHidden)
        filters |= QDir::Hidden;

    const QFileInfoList infos = dir.entryInfoList(filters, QDir::NoSort);
    QVector<FileEntry> entries;
    entries.reserve(infos.size());
    const QString needle = m_nameFilter.toLower();

    for (const QFileInfo &info : infos) {
        if (!m_showHidden && info.fileName().startsWith(QLatin1Char('.')))
            continue;
        if (!needle.isEmpty() && !info.isDir()
            && !info.fileName().toLower().contains(needle))
            continue;

        FileEntry entry;
        entry.name = info.fileName();
        entry.path = info.absoluteFilePath();
        entry.isDir = info.isDir();
        entry.isSymLink = info.isSymLink();
        entry.isHidden = info.fileName().startsWith(QLatin1Char('.'));
        entry.size = info.isDir() ? -1 : info.size();
        entry.mtime = info.lastModified().toSecsSinceEpoch();
        entries.push_back(entry);
    }

    std::sort(entries.begin(), entries.end(), [this](const FileEntry &a, const FileEntry &b) {
        if (a.isDir != b.isDir)
            return a.isDir && !b.isDir;
        if (m_sortBy == QLatin1String("mtime"))
            return a.mtime > b.mtime;
        if (m_sortBy == QLatin1String("size"))
            return a.size > b.size;
        return a.name.toLower() < b.name.toLower();
    });

    return entries;
}

QString FileSystemController::resolveStartPath(const QString &cliPath, QString *warning) const
{
    QString candidate = cliPath.trimmed();
    if (candidate.isEmpty())
        candidate = m_config->startPath();

    QFileInfo info(QDir::cleanPath(candidate));
    if (!info.exists()) {
        if (warning)
            *warning = QStringLiteral("'%1' existiert nicht, nutze Home").arg(candidate);
        return QDir::homePath();
    }
    if (!info.isDir()) {
        if (warning)
            *warning = QStringLiteral("'%1' ist kein Verzeichnis, nutze Parent").arg(candidate);
        return info.absolutePath();
    }
    return info.absoluteFilePath();
}
