#include "fileoperations.h"

#include "i18n.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QMimeDatabase>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>

namespace {

bool copyDirRecursively(const QString &src, const QString &dest)
{
    QDir srcDir(src);
    if (!srcDir.exists())
        return false;
    if (!QDir().mkpath(dest))
        return false;

    const auto entries = srcDir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
    for (const QFileInfo &entry : entries) {
        const QString target = QDir(dest).filePath(entry.fileName());
        if (entry.isSymLink()) {
            if (!QFile::link(entry.symLinkTarget(), target) && !QFile::copy(entry.absoluteFilePath(), target))
                return false;
        } else if (entry.isDir()) {
            if (!copyDirRecursively(entry.absoluteFilePath(), target))
                return false;
        } else if (!QFile::copy(entry.absoluteFilePath(), target)) {
            return false;
        }
    }
    return true;
}

bool removePath(const QString &path)
{
    QFileInfo info(path);
    if (info.isDir() && !info.isSymLink())
        return QDir(path).removeRecursively();
    return QFile::remove(path);
}

} // namespace

FileOperations::FileOperations(I18n *i18n, QObject *parent)
    : QObject(parent)
    , m_i18n(i18n)
{
}

QString FileOperations::tr(const QString &key, const QVariantList &args) const
{
    return m_i18n ? m_i18n->t(key, args) : key;
}

bool FileOperations::isSafeBasename(const QString &name)
{
    if (name.isEmpty() || name != name.trimmed())
        return false;
    if (name == QLatin1String(".") || name == QLatin1String(".."))
        return false;
    if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\'))
        || name.contains(QChar(0)))
        return false;
    if (QFileInfo(name).isAbsolute())
        return false;
    return QFileInfo(name).fileName() == name;
}

QString FileOperations::validateBasename(const QString &name, QString *error) const
{
    const QString trimmed = name.trimmed();
    if (!isSafeBasename(trimmed)) {
        if (error)
            *error = tr(QStringLiteral("error.invalid_name"));
        return {};
    }
    return trimmed;
}

QString FileOperations::safeJoin(const QString &baseDir, const QString &name, QString *error) const
{
    const QString baseName = validateBasename(name, error);
    if (baseName.isEmpty())
        return {};

    const QString baseAbs = QFileInfo(baseDir).absoluteFilePath();
    const QString target = QFileInfo(QDir(baseAbs).filePath(baseName)).absoluteFilePath();
    if (QFileInfo(target).absolutePath() != baseAbs) {
        if (error)
            *error = tr(QStringLiteral("error.outside_dir"));
        return {};
    }
    return target;
}

bool FileOperations::isProtectedPath(const QString &path)
{
    const QFileInfo info(path);
    const QString resolved = info.exists() ? info.canonicalFilePath() : info.absoluteFilePath();
    const QString homeCanon = QFileInfo(QDir::homePath()).canonicalFilePath();
    return resolved == QLatin1String("/") || resolved == QDir::homePath() || resolved == homeCanon;
}

bool FileOperations::isPasteIntoSelf(const QString &src, const QString &dest)
{
    const QFileInfo srcInfo(src);
    if (!srcInfo.isDir())
        return false;
    const QString s = srcInfo.canonicalFilePath();
    const QString d = QFileInfo(dest).absoluteFilePath();
    if (s.isEmpty())
        return false;
    return d == s || d.startsWith(s + QLatin1Char('/'));
}

bool FileOperations::trashAvailable()
{
    return !QStandardPaths::findExecutable(QStringLiteral("gio")).isEmpty()
        || !QStandardPaths::findExecutable(QStringLiteral("trash-put")).isEmpty();
}

QString FileOperations::humanSize(qint64 bytes)
{
    double value = static_cast<double>(qMax<qint64>(0, bytes));
    static const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    int unit = 0;
    while (value >= 1024.0 && unit < 5) {
        value /= 1024.0;
        ++unit;
    }
    if (unit == 0)
        return QStringLiteral("%1 B").arg(bytes);
    return QStringLiteral("%1 %2").arg(value, 0, 'f', 1).arg(QLatin1String(units[unit]));
}

bool FileOperations::renamePath(const QString &src, const QString &newName, bool overwrite,
                                QString *error)
{
    QString err;
    const QString target = safeJoin(QFileInfo(src).absolutePath(), newName, &err);
    if (target.isEmpty()) {
        if (error)
            *error = err;
        return false;
    }

    if (QFileInfo::exists(target)) {
        if (!overwrite) {
            if (error)
                *error = tr(QStringLiteral("error.exists"), {newName});
            return false;
        }
        if (!removePath(target)) {
            if (error)
                *error = tr(QStringLiteral("error.cannot_remove_target"));
            return false;
        }
    }

    if (!QFile::rename(src, target)) {
        if (error)
            *error = tr(QStringLiteral("error.rename_failed"));
        return false;
    }
    return true;
}

bool FileOperations::deletePaths(const QStringList &paths, bool permanent, bool preferTrash,
                                 QString *error)
{
    int ok = 0;
    for (const QString &path : paths) {
        bool done = false;
        if (!permanent && preferTrash && trashAvailable()) {
            const QString gio = QStandardPaths::findExecutable(QStringLiteral("gio"));
            if (!gio.isEmpty())
                done = QProcess::execute(gio, {QStringLiteral("trash"), path}) == 0;
            if (!done) {
                const QString trashPut =
                    QStandardPaths::findExecutable(QStringLiteral("trash-put"));
                if (!trashPut.isEmpty())
                    done = QProcess::execute(trashPut, {path}) == 0;
            }
        }

        if (!done)
            done = removePath(path);

        if (done)
            ++ok;
        else if (error && error->isEmpty())
            *error = tr(QStringLiteral("error.delete_failed"), {QFileInfo(path).fileName()});
    }
    return ok > 0;
}

bool FileOperations::createFile(const QString &dir, const QString &name, QString *error)
{
    QString err;
    const QString target = safeJoin(dir, name, &err);
    if (target.isEmpty()) {
        if (error)
            *error = err;
        return false;
    }
    if (QFileInfo::exists(target)) {
        if (error)
            *error = tr(QStringLiteral("error.exists"), {name});
        return false;
    }
    QFile file(target);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = tr(QStringLiteral("error.create_failed"), {file.errorString()});
        return false;
    }
    file.close();
    return true;
}

bool FileOperations::createFolder(const QString &dir, const QString &name, QString *error)
{
    QString err;
    const QString target = safeJoin(dir, name, &err);
    if (target.isEmpty()) {
        if (error)
            *error = err;
        return false;
    }
    if (QFileInfo::exists(target)) {
        if (error)
            *error = tr(QStringLiteral("error.exists"), {name});
        return false;
    }
    if (!QDir().mkdir(target)) {
        if (error)
            *error = tr(QStringLiteral("error.mkdir_failed"));
        return false;
    }
    return true;
}

bool FileOperations::pastePaths(const QStringList &sources, const QString &mode,
                                const QString &destDir, bool overwrite,
                                QStringList *pendingOverwrite, QString *error)
{
    for (const QString &src : sources) {
        QFileInfo srcInfo(src);
        if (!srcInfo.exists()) {
            if (error)
                *error = tr(QStringLiteral("error.source_missing"), {src});
            continue;
        }

        const QString dest = QDir(destDir).filePath(srcInfo.fileName());
        const QString destResolved = QFileInfo(dest).absoluteFilePath();
        const QString srcResolved = srcInfo.canonicalFilePath();

        if (destResolved == srcResolved)
            continue;
        if (isPasteIntoSelf(src, dest)) {
            if (error)
                *error = tr(QStringLiteral("error.paste_into_self"), {srcInfo.fileName()});
            continue;
        }

        if (QFileInfo::exists(dest) && !overwrite) {
            if (pendingOverwrite)
                pendingOverwrite->append(src);
            continue;
        }

        if (QFileInfo::exists(dest) && overwrite && !removePath(dest)) {
            if (error)
                *error = tr(QStringLiteral("error.overwrite_failed"), {srcInfo.fileName()});
            continue;
        }

        bool ok = false;
        if (mode == QLatin1String("copy")) {
            ok = srcInfo.isDir() ? copyDirRecursively(src, dest) : QFile::copy(src, dest);
        } else {
            ok = QFile::rename(src, dest);
            if (!ok) {
                ok = (srcInfo.isDir() ? copyDirRecursively(src, dest) : QFile::copy(src, dest))
                    && removePath(src);
            }
        }

        if (!ok && error)
            *error = tr(QStringLiteral("error.paste_failed"), {srcInfo.fileName()});
    }

    return !pendingOverwrite || pendingOverwrite->isEmpty();
}

namespace {

bool looksLikeBrowser(const QString &desktopId)
{
    const QString id = desktopId.toLower();
    static const QStringList needles{
        QStringLiteral("firefox"), QStringLiteral("chrom"), QStringLiteral("brave"),
        QStringLiteral("opera"),   QStringLiteral("vivaldi"), QStringLiteral("edge"),
        QStringLiteral("librewolf"), QStringLiteral("tor-browser"), QStringLiteral("epiphany"),
        QStringLiteral("zen-browser"), QStringLiteral("floorp"), QStringLiteral("midori"),
        QStringLiteral("falkon"), QStringLiteral("qutebrowser"), QStringLiteral("waterfox"),
        QStringLiteral("url-handler")};
    for (const QString &n : needles) {
        if (id.contains(n))
            return true;
    }
    return false;
}

bool isGenericOrBinaryMime(const QString &mime)
{
    return mime.isEmpty()
        || mime == QLatin1String("application/octet-stream")
        || mime == QLatin1String("application/x-sharedlib")
        || mime == QLatin1String("application/x-executable")
        || mime == QLatin1String("application/x-msdos-program")
        || mime == QLatin1String("application/vnd.microsoft.portable-executable")
        || mime == QLatin1String("application/x-pie-executable")
        || mime == QLatin1String("application/x-object")
        || mime == QLatin1String("application/x-raw-disk-image");
}

bool isKnownMediaOrArchiveMime(const QString &mime)
{
    return mime.startsWith(QLatin1String("image/"))
        || mime.startsWith(QLatin1String("video/"))
        || mime.startsWith(QLatin1String("audio/"))
        || mime == QLatin1String("application/pdf")
        || mime == QLatin1String("application/zip")
        || mime == QLatin1String("application/gzip")
        || mime == QLatin1String("application/x-tar")
        || mime == QLatin1String("application/x-7z-compressed")
        || mime == QLatin1String("application/x-rar-compressed")
        || mime == QLatin1String("application/vnd.rar")
        || mime == QLatin1String("application/x-bzip2")
        || mime == QLatin1String("application/x-xz");
}

bool isTextualMime(const QString &mime)
{
    if (mime.startsWith(QLatin1String("text/")))
        return true;
    static const QStringList textual{
        QStringLiteral("application/json"),
        QStringLiteral("application/xml"),
        QStringLiteral("application/javascript"),
        QStringLiteral("application/typescript"),
        QStringLiteral("application/x-shellscript"),
        QStringLiteral("application/x-desktop"),
        QStringLiteral("application/x-yaml"),
        QStringLiteral("application/toml"),
        QStringLiteral("application/sql"),
        QStringLiteral("application/xhtml+xml"),
        QStringLiteral("application/rdf+xml"),
        QStringLiteral("application/x-php"),
        QStringLiteral("application/x-ruby"),
        QStringLiteral("application/x-perl"),
        QStringLiteral("application/x-python"),
        QStringLiteral("application/x-python-code"),
        QStringLiteral("inode/x-empty"),
    };
    return textual.contains(mime);
}

bool probeLooksTextual(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray probe = file.read(8192);
    if (probe.isEmpty())
        return true; // empty file → editor
    return !probe.contains('\0');
}

bool isTextualFile(const QString &path)
{
    QMimeDatabase db;
    const QString mime = db.mimeTypeForFile(path).name();
    if (isTextualMime(mime))
        return true;
    if (isKnownMediaOrArchiveMime(mime) || isGenericOrBinaryMime(mime)) {
        // Ambiguous octet-stream / unknown: sniff content.
        if (mime == QLatin1String("application/octet-stream") || mime.isEmpty())
            return probeLooksTextual(path);
        return false;
    }
    // Other application/* etc.: sniff before trusting browser handlers.
    return probeLooksTextual(path);
}

bool execAcceptsFiles(const QString &exec)
{
    return exec.contains(QLatin1String("%f")) || exec.contains(QLatin1String("%F"))
        || exec.contains(QLatin1String("%u")) || exec.contains(QLatin1String("%U"))
        || !exec.contains(QLatin1Char('%'));
}

QString desktopDisplayName(QSettings &ini)
{
    const QString locale = QLocale::system().name();
    const QString lang = locale.section(QLatin1Char('_'), 0, 0);
    QString name = ini.value(QStringLiteral("Name[%1]").arg(locale)).toString();
    if (name.isEmpty())
        name = ini.value(QStringLiteral("Name[%1]").arg(lang)).toString();
    if (name.isEmpty())
        name = ini.value(QStringLiteral("Name")).toString();
    return name;
}

QVariantMap appFromDesktopFile(const QString &desktopPath)
{
    QSettings ini(desktopPath, QSettings::IniFormat);
    ini.beginGroup(QStringLiteral("Desktop Entry"));
    if (ini.value(QStringLiteral("Type")).toString() != QLatin1String("Application"))
        return {};
    if (ini.value(QStringLiteral("NoDisplay"), false).toBool())
        return {};
    if (ini.value(QStringLiteral("Hidden"), false).toBool())
        return {};
    const QString exec = ini.value(QStringLiteral("Exec")).toString();
    if (exec.isEmpty() || !execAcceptsFiles(exec))
        return {};
    const QString name = desktopDisplayName(ini);
    if (name.isEmpty())
        return {};

    QVariantMap app;
    app.insert(QStringLiteral("id"), QFileInfo(desktopPath).fileName());
    app.insert(QStringLiteral("name"), name);
    app.insert(QStringLiteral("desktopPath"), desktopPath);
    app.insert(QStringLiteral("browser"), looksLikeBrowser(QFileInfo(desktopPath).fileName()));
    return app;
}

QStringList applicationSearchDirs()
{
    QStringList dirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    const QStringList extras{QStringLiteral("/usr/share/applications"),
                             QStringLiteral("/usr/local/share/applications")};
    for (const QString &extra : extras) {
        if (!dirs.contains(extra))
            dirs.append(extra);
    }
    return dirs;
}

QString findDesktopPath(const QString &desktopId)
{
    if (desktopId.isEmpty())
        return {};
    if (QFileInfo::exists(desktopId) && desktopId.endsWith(QLatin1String(".desktop")))
        return desktopId;

    for (const QString &dir : applicationSearchDirs()) {
        const QString direct = QDir(dir).filePath(desktopId);
        if (QFileInfo::exists(direct))
            return direct;
        QDirIterator it(dir, {desktopId}, QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext())
            return it.next();
    }
    return {};
}

QList<QVariantMap> scanAllApps()
{
    QHash<QString, QVariantMap> byId;
    for (const QString &dir : applicationSearchDirs()) {
        QDirIterator it(dir, {QStringLiteral("*.desktop")}, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QVariantMap app = appFromDesktopFile(path);
            if (app.isEmpty())
                continue;
            const QString id = app.value(QStringLiteral("id")).toString();
            if (!byId.contains(id))
                byId.insert(id, app);
        }
    }
    QList<QVariantMap> apps = byId.values();
    std::sort(apps.begin(), apps.end(), [](const QVariantMap &a, const QVariantMap &b) {
        return QString::localeAwareCompare(a.value(QStringLiteral("name")).toString(),
                                           b.value(QStringLiteral("name")).toString())
            < 0;
    });
    return apps;
}

QStringList desktopIdsFromGioMime(const QString &mime)
{
    QProcess proc;
    proc.start(QStringLiteral("gio"), {QStringLiteral("mime"), mime});
    if (!proc.waitForFinished(2000))
        return {};
    const QString out = QString::fromUtf8(proc.readAllStandardOutput());
    QStringList ids;
    for (QString line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        line = line.trimmed();
        if (!line.endsWith(QLatin1String(".desktop")))
            continue;
        const int colon = line.lastIndexOf(QLatin1Char(':'));
        QString id = colon >= 0 ? line.mid(colon + 1).trimmed() : line;
        if (id.endsWith(QLatin1String(".desktop")) && !ids.contains(id))
            ids.append(id);
    }
    return ids;
}

QString firstNonBrowserDesktop(const QString &mime)
{
    for (const QString &id : desktopIdsFromGioMime(mime)) {
        if (looksLikeBrowser(id))
            continue;
        const QString path = findDesktopPath(id);
        if (!path.isEmpty())
            return path;
    }
    return {};
}

QString shellQuote(const QString &value)
{
    QString out = value;
    out.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QLatin1Char('\'') + out + QLatin1Char('\'');
}

bool launchExpandedExec(const QString &desktopPath, const QString &filePath)
{
    QSettings ini(desktopPath, QSettings::IniFormat);
    ini.beginGroup(QStringLiteral("Desktop Entry"));
    QString exec = ini.value(QStringLiteral("Exec")).toString();
    if (exec.isEmpty())
        return false;
    const bool terminal = ini.value(QStringLiteral("Terminal"), false).toBool();
    ini.endGroup();

    const QString absFile = QFileInfo(filePath).absoluteFilePath();
    const QString fileUrl = QUrl::fromLocalFile(absFile).toString(QUrl::FullyEncoded);

    // Expand field codes (keep only one file arg).
    exec.replace(QStringLiteral("%F"), shellQuote(absFile));
    exec.replace(QStringLiteral("%f"), shellQuote(absFile));
    exec.replace(QStringLiteral("%U"), shellQuote(fileUrl));
    exec.replace(QStringLiteral("%u"), shellQuote(fileUrl));
    exec.replace(QRegularExpression(QStringLiteral("%[iIcCk]")), QString());
    exec = exec.simplified();

    if (terminal) {
        // Prefer Freedesktop / Omarchy terminal launchers.
        if (QProcess::startDetached(QStringLiteral("xdg-terminal-exec"),
                                    {QStringLiteral("sh"), QStringLiteral("-lc"), exec})) {
            return true;
        }
        if (QProcess::startDetached(QStringLiteral("omarchy-default-terminal"),
                                    {QStringLiteral("-e"), QStringLiteral("sh"),
                                     QStringLiteral("-lc"), exec})) {
            return true;
        }
        static const QStringList terms{QStringLiteral("ghostty"), QStringLiteral("alacritty"),
                                       QStringLiteral("kitty"), QStringLiteral("foot"),
                                       QStringLiteral("xterm")};
        for (const QString &term : terms) {
            if (QProcess::startDetached(term, {QStringLiteral("-e"), QStringLiteral("sh"),
                                               QStringLiteral("-lc"), exec})) {
                return true;
            }
        }
        return false;
    }

    // GUI app: run via sh -lc so quoting from field-code expansion works.
    return QProcess::startDetached(QStringLiteral("sh"), {QStringLiteral("-lc"), exec});
}

bool launchDesktop(const QString &desktopPath, const QString &filePath)
{
    const QString absFile = QFileInfo(filePath).absoluteFilePath();
    if (!QFileInfo::exists(absFile))
        return false;

    // Direct Exec expansion is more reliable than gio launch for Terminal=true apps.
    if (launchExpandedExec(desktopPath, absFile))
        return true;

    if (QProcess::startDetached(QStringLiteral("gio"),
                                {QStringLiteral("launch"), desktopPath, absFile})) {
        return true;
    }
    const QString id = QFileInfo(desktopPath).completeBaseName();
    return QProcess::startDetached(QStringLiteral("gtk-launch"), {id, absFile});
}

} // namespace

bool FileOperations::openWithDesktop(const QString &path, const QString &desktopPath, QString *error)
{
    if (QFileInfo(path).isDir()) {
        if (error)
            *error = tr(QStringLiteral("error.open_failed"));
        return false;
    }
    if (looksLikeBrowser(QFileInfo(desktopPath).fileName())) {
        // Local files must never open in a browser (file:// download UX).
        if (error)
            *error = tr(QStringLiteral("error.open_browser_blocked"));
        return false;
    }
    if (!QFileInfo::exists(desktopPath)) {
        if (error)
            *error = tr(QStringLiteral("error.open_with_missing"), {desktopPath});
        return false;
    }
    if (!launchDesktop(desktopPath, path)) {
        if (error)
            *error = tr(QStringLiteral("error.open_with_failed"),
                        {QFileInfo(desktopPath).completeBaseName()});
        return false;
    }
    return true;
}

bool FileOperations::openWithEditor(const QString &path, QString *error)
{
    if (QFileInfo(path).isDir()) {
        if (error)
            *error = tr(QStringLiteral("error.editor_files_only"));
        return false;
    }

    // 1) $EDITOR / $VISUAL (Omarchy: omarchy-launch-editor --inline)
    QString editor = qEnvironmentVariable("EDITOR");
    if (editor.isEmpty())
        editor = qEnvironmentVariable("VISUAL");
    if (!editor.isEmpty() && !editor.contains(QLatin1String("xdg-open"))) {
        QStringList parts = QProcess::splitCommand(editor);
        if (!parts.isEmpty()) {
            const QString program = parts.takeFirst();
            parts.append(path);
            if (QProcess::startDetached(program, parts))
                return true;
        }
    }

    // 2) MIME default for text/plain, skipping browsers
    const QString textDesktop = firstNonBrowserDesktop(QStringLiteral("text/plain"));
    if (!textDesktop.isEmpty() && launchDesktop(textDesktop, path))
        return true;

    // 3) Common GUI/TTY editors
    static const QStringList fallbacks{
        QStringLiteral("omarchy-launch-editor"), QStringLiteral("omawrite"),
        QStringLiteral("nvim"), QStringLiteral("vim"), QStringLiteral("nano"),
        QStringLiteral("gedit"), QStringLiteral("mousepad"), QStringLiteral("xed")};
    for (const QString &program : fallbacks) {
        if (QProcess::startDetached(program, {path}))
            return true;
    }

    if (error)
        *error = tr(QStringLiteral("error.editor_start"),
                    {editor.isEmpty() ? QStringLiteral("editor") : editor});
    return false;
}

bool FileOperations::openWithDefault(const QString &path, QString *error)
{
    if (QFileInfo(path).isDir())
        return true;

    // Textual / non-binary content → default text editor (never browser).
    if (isTextualFile(path))
        return openWithEditor(path, error);

    // Non-browser handler for this MIME (image viewer, PDF, …).
    const QString mime = mimeForPath(path);
    const QString desktop = firstNonBrowserDesktop(mime);
    if (!desktop.isEmpty())
        return openWithDesktop(path, desktop, error);

    // Do NOT fall back to QDesktopServices / xdg-open — that often opens a browser.
    if (error)
        *error = tr(QStringLiteral("error.open_no_handler"));
    return false;
}

QString FileOperations::mimeForPath(const QString &path) const
{
    QMimeDatabase db;
    return db.mimeTypeForFile(path).name();
}

QString FileOperations::desktopPathForId(const QString &desktopId) const
{
    QString id = desktopId.trimmed();
    if (id.isEmpty())
        return {};
    if (!id.endsWith(QLatin1String(".desktop")))
        id += QStringLiteral(".desktop");
    return findDesktopPath(id);
}

bool FileOperations::shouldAskOpenWith(const QString &path) const
{
    const QFileInfo info(path);
    if (!info.exists() || info.isDir())
        return false;

    // Text goes to the editor — no chooser.
    if (isTextualFile(path))
        return false;

    // Known non-browser handler available.
    if (!firstNonBrowserDesktop(mimeForPath(path)).isEmpty())
        return false;

    return true;
}

QVariantList FileOperations::openWithCandidates(const QString &path) const
{
    QVariantList out;
    if (QFileInfo(path).isDir())
        return out;

    const QString mime = mimeForPath(path);
    const QList<QVariantMap> all = scanAllApps();
    QHash<QString, QVariantMap> byId;
    for (const QVariantMap &app : all)
        byId.insert(app.value(QStringLiteral("id")).toString(), app);

    QSet<QString> seen;
    const auto appendId = [&](const QString &id, bool recommended) {
        QString normalized = id.trimmed();
        if (!normalized.endsWith(QLatin1String(".desktop")))
            normalized += QStringLiteral(".desktop");
        if (seen.contains(normalized) || !byId.contains(normalized))
            return;
        if (looksLikeBrowser(normalized))
            return;
        QVariantMap app = byId.value(normalized);
        app.insert(QStringLiteral("recommended"), recommended);
        out.append(app);
        seen.insert(normalized);
    };

    for (const QString &id : desktopIdsFromGioMime(mime))
        appendId(id, true);
    for (const QString &id : desktopIdsFromGioMime(QStringLiteral("text/plain")))
        appendId(id, isTextualFile(path));

    for (const QVariantMap &app : all) {
        const QString id = app.value(QStringLiteral("id")).toString();
        appendId(id, false);
    }
    return out;
}
