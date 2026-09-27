#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

class I18n : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString language READ language NOTIFY languageChanged)
    Q_PROPERTY(QString languageName READ languageName NOTIFY languageChanged)
    Q_PROPERTY(int revision READ revision NOTIFY languageChanged)
    Q_PROPERTY(QStringList availableLanguages READ availableLanguages NOTIFY catalogsChanged)

public:
    explicit I18n(QObject *parent = nullptr);

    QString language() const { return m_language; }
    QString languageName() const;
    int revision() const { return m_revision; }
    QStringList availableLanguages() const { return m_available; }

    // Translate key; args replace %1, %2, … via QString::arg.
    // QML: i18n.t("key") or i18n.t("key", ["a", "b"])
    Q_INVOKABLE QString t(const QString &key, const QVariantList &args = QVariantList()) const;

    // Thread-safe-ish snapshot for background workers.
    QHash<QString, QString> catalog() const;

public slots:
    void setLanguage(const QString &code);
    void cycleLanguage();
    void reload();

signals:
    void languageChanged();
    void catalogsChanged();

private:
    QString lookup(const QString &key) const;
    bool loadFile(const QString &path, QHash<QString, QString> *out) const;
    void discoverAvailable();
    void loadLanguage(const QString &code);
    QString resolveSystemLanguage() const;
    QStringList searchRoots() const;

    QString m_language = QStringLiteral("en");
    int m_revision = 0;
    QHash<QString, QString> m_fallback;
    QHash<QString, QString> m_strings;
    QStringList m_available;
};
