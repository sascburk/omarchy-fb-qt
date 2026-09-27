#pragma once

#include <QObject>
#include <QString>

class I18n;

class PreviewProvider : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString text READ text NOTIFY previewChanged)
    Q_PROPERTY(QString kind READ kind NOTIFY previewChanged)
    Q_PROPERTY(QString imagePath READ imagePath NOTIFY previewChanged)
    Q_PROPERTY(QString mediaUrl READ mediaUrl NOTIFY previewChanged)
    Q_PROPERTY(bool truncated READ truncated NOTIFY previewChanged)
    Q_PROPERTY(QString path READ path NOTIFY previewChanged)

public:
    explicit PreviewProvider(I18n *i18n, QObject *parent = nullptr);

    QString text() const { return m_text; }
    QString kind() const { return m_kind; }
    QString imagePath() const { return m_imagePath; }
    QString mediaUrl() const { return m_mediaUrl; }
    bool truncated() const { return m_truncated; }
    QString path() const { return m_path; }

public slots:
    void setMaxBytes(int bytes);
    void load(const QString &path);
    void clear();

signals:
    void previewChanged();

private:
    I18n *m_i18n = nullptr;
    int m_maxBytes = 65536;
    QString m_text;
    QString m_kind;
    QString m_imagePath;
    QString m_mediaUrl;
    bool m_truncated = false;
    QString m_path;
    int m_token = 0;
};
