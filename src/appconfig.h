#pragma once

#include <QHash>
#include <QObject>
#include <QSettings>
#include <QStringList>

class AppConfig : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString startPath READ startPath WRITE setStartPath NOTIFY configChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY configChanged)
    Q_PROPERTY(int previewMaxBytes READ previewMaxBytes WRITE setPreviewMaxBytes NOTIFY configChanged)
    Q_PROPERTY(bool useTrash READ useTrash WRITE setUseTrash NOTIFY configChanged)
    Q_PROPERTY(QString sortBy READ sortBy WRITE setSortBy NOTIFY configChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY configChanged)
    Q_PROPERTY(QStringList bookmarks READ bookmarks NOTIFY configChanged)

public:
    explicit AppConfig(QObject *parent = nullptr);

    QString startPath() const { return m_startPath; }
    bool showHidden() const { return m_showHidden; }
    int previewMaxBytes() const { return m_previewMaxBytes; }
    bool useTrash() const { return m_useTrash; }
    QString sortBy() const { return m_sortBy; }
    QString language() const { return m_language; }
    QStringList bookmarks() const { return m_bookmarks; }
    QString openWithHandler(const QString &mime) const;
    void setOpenWithHandler(const QString &mime, const QString &desktopId);

public slots:
    void setStartPath(const QString &path);
    void setShowHidden(bool show);
    void setPreviewMaxBytes(int bytes);
    void setUseTrash(bool use);
    void setSortBy(const QString &sortBy);
    void setLanguage(const QString &language);
    void cycleSortBy();
    bool addBookmark(const QString &path);
    bool removeBookmark(const QString &path);
    bool isBookmark(const QString &path) const;
    bool toggleBookmark(const QString &path);
    void save();
    void reload();

signals:
    void configChanged();

private:
    void load();
    QString resolveExistingDir(const QString &candidate) const;

    QSettings m_settings;
    QString m_startPath;
    bool m_showHidden = false;
    int m_previewMaxBytes = 65536;
    bool m_useTrash = true;
    QString m_sortBy = QStringLiteral("name");
    QString m_language = QStringLiteral("auto");
    QStringList m_bookmarks;
    QHash<QString, QString> m_openWith; // mime → desktop id
};
