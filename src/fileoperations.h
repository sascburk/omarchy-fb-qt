#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

class I18n;

class FileOperations : public QObject {
    Q_OBJECT

public:
    explicit FileOperations(I18n *i18n, QObject *parent = nullptr);

    static bool isSafeBasename(const QString &name);
    QString validateBasename(const QString &name, QString *error) const;
    QString safeJoin(const QString &baseDir, const QString &name, QString *error) const;
    static bool isProtectedPath(const QString &path);
    static bool isPasteIntoSelf(const QString &src, const QString &dest);
    static bool trashAvailable();
    static QString humanSize(qint64 bytes);

public slots:
    bool renamePath(const QString &src, const QString &newName, bool overwrite, QString *error);
    bool deletePaths(const QStringList &paths, bool permanent, bool preferTrash, QString *error);
    bool createFile(const QString &dir, const QString &name, QString *error);
    bool createFolder(const QString &dir, const QString &name, QString *error);
    bool pastePaths(const QStringList &sources, const QString &mode, const QString &destDir,
                    bool overwrite, QStringList *pendingOverwrite, QString *error);
    bool openWithDefault(const QString &path, QString *error);
    bool openWithEditor(const QString &path, QString *error);
    QString mimeForPath(const QString &path) const;
    QString desktopPathForId(const QString &desktopId) const;
    bool shouldAskOpenWith(const QString &path) const;
    QVariantList openWithCandidates(const QString &path) const;
    bool openWithDesktop(const QString &path, const QString &desktopPath, QString *error);

signals:
    void operationFinished(bool ok, const QString &message);

private:
    QString tr(const QString &key, const QVariantList &args = {}) const;

    I18n *m_i18n = nullptr;
};
