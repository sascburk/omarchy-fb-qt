#pragma once

#include "previewprovider.h"

#include <QAbstractListModel>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVector>

class AppConfig;
class DirSizeCalculator;
class FileOperations;
class I18n;

struct FileEntry {
    QString name;
    QString path;
    bool isDir = false;
    bool isSymLink = false;
    bool isHidden = false;
    qint64 size = 0;
    qint64 mtime = 0;
};

class DirectoryListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        PathRole,
        IsDirRole,
        IsSymLinkRole,
        IsHiddenRole,
        SizeRole,
        MTimeRole,
        MarkedRole,
        DisplaySizeRole
    };

    explicit DirectoryListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(QVector<FileEntry> entries);
    void setMarked(const QSet<QString> &marked);
    QString pathAt(int row) const;
    bool isDirAt(int row) const;

private:
    QVector<FileEntry> m_entries;
    QSet<QString> m_marked;
};

class FileSystemController : public QObject {
    Q_OBJECT
    Q_PROPERTY(DirectoryListModel *model READ model CONSTANT)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(QString selectedPath READ selectedPath NOTIFY selectedPathChanged)
    Q_PROPERTY(QString selectedName READ selectedName NOTIFY selectedPathChanged)
    Q_PROPERTY(bool selectedIsDir READ selectedIsDir NOTIFY selectedPathChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)
    Q_PROPERTY(QString nameFilter READ nameFilter WRITE setNameFilter NOTIFY nameFilterChanged)
    Q_PROPERTY(QString sortBy READ sortBy WRITE setSortBy NOTIFY sortByChanged)
    Q_PROPERTY(int markedCount READ markedCount NOTIFY markedChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY selectedPathChanged)
    Q_PROPERTY(QString clipboardSummary READ clipboardSummary NOTIFY clipboardChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(PreviewProvider *preview READ preview CONSTANT)

public:
    explicit FileSystemController(AppConfig *config, FileOperations *ops,
                                  DirSizeCalculator *sizes, PreviewProvider *preview, I18n *i18n,
                                  QObject *parent = nullptr);

    DirectoryListModel *model() const { return m_model; }
    PreviewProvider *preview() const { return m_preview; }

    QString currentPath() const { return m_currentPath; }
    QString selectedPath() const { return m_selectedPath; }
    QString selectedName() const;
    bool selectedIsDir() const;
    bool showHidden() const { return m_showHidden; }
    QString nameFilter() const { return m_nameFilter; }
    QString sortBy() const { return m_sortBy; }
    int markedCount() const { return m_marked.size(); }
    int selectedIndex() const { return m_selectedIndex; }
    QString clipboardSummary() const;
    QString statusText() const { return m_statusText; }
    QString message() const { return m_message; }

public slots:
    void initialize(const QString &cliPath = QString());
    void selectIndex(int index);
    void activateIndex(int index);
    void goParent();
    void goTo(const QString &path);
    void reload();
    void setShowHidden(bool show);
    void setNameFilter(const QString &filter);
    void setSortBy(const QString &sortBy);
    void cycleSort();
    void cycleLanguage();
    void toggleMark();
    void markAll();
    void clearMarksAndFilter();
    void copySelected();
    void cutSelected();
    void paste();
    void openSelected();
    void requestOpenWith();
    void openWithDesktop(const QString &desktopPath);
    void editSelected();
    void requestRename();
    void confirmRename(const QString &newName);
    void requestDelete(bool permanent);
    void confirmDelete(bool permanent, bool useTrash);
    void requestNewFile();
    void confirmNewFile(const QString &name);
    void requestNewFolder();
    void confirmNewFolder(const QString &name);
    void computeSelectedSize();
    void removeBookmark(const QString &path);
    void toggleFavoriteSelected();
    void completeAction(const QString &action, const QString &value = QString());
    QStringList targetPaths() const;
    QString targetDir() const;

signals:
    void currentPathChanged();
    void selectedPathChanged();
    void showHiddenChanged();
    void nameFilterChanged();
    void sortByChanged();
    void markedChanged();
    void clipboardChanged();
    void statusTextChanged();
    void messageChanged();
    void askTextInput(const QString &title, const QString &initial, const QString &action);
    void askConfirm(const QString &message, const QString &action, bool danger,
                    const QString &okLabel);
    void askChoice(const QString &message, const QString &action);
    void askOpenWith(const QString &path, const QString &mime, const QVariantList &apps);

private:
    void setCurrentPath(const QString &path);
    void setSelectedPath(const QString &path);
    void refreshListing(const QString &focusPath = QString());
    void refreshStatus();
    void refreshPreview();
    void notify(const QString &text);
    void requestOpenWithFor(const QString &path);
    void openPath(const QString &path);
    QVector<FileEntry> listDirectory(const QString &path) const;
    QString resolveStartPath(const QString &cliPath, QString *warning) const;

    AppConfig *m_config = nullptr;
    FileOperations *m_ops = nullptr;
    DirSizeCalculator *m_sizes = nullptr;
    PreviewProvider *m_preview = nullptr;
    I18n *m_i18n = nullptr;
    DirectoryListModel *m_model = nullptr;

    QString m_currentPath;
    QString m_selectedPath;
    bool m_showHidden = false;
    QString m_nameFilter;
    QString m_sortBy = QStringLiteral("name");
    QSet<QString> m_marked;
    QStringList m_clipboardPaths;
    QString m_clipboardMode;
    QString m_statusText;
    QString m_message;
    QString m_pendingOpenPath;
    int m_selectedIndex = -1;
};
