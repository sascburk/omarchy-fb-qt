#pragma once

#include <QHash>
#include <QObject>
#include <QPair>

class I18n;

class DirSizeCalculator : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit DirSizeCalculator(I18n *i18n, QObject *parent = nullptr);

    bool busy() const { return m_busy; }

    // Returns cached (size, files, dirs) when still valid.
    bool getCached(const QString &path, qint64 *size, int *files, int *dirs) const;
    void invalidate(const QString &path);

public slots:
    void compute(const QString &path);

signals:
    void busyChanged();
    void computed(const QString &path, qint64 size, int files, int dirs);
    void failed(const QString &path, const QString &error);

private:
    struct CacheEntry {
        qint64 mtime = 0;
        qint64 size = 0;
        int files = 0;
        int dirs = 0;
    };

    I18n *m_i18n = nullptr;
    bool m_busy = false;
    QHash<QString, CacheEntry> m_cache;
};
