#include "i18n.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSet>
#include <QStandardPaths>

I18n::I18n(QObject *parent)
    : QObject(parent)
{
    discoverAvailable();

    // Load English as permanent fallback catalog.
    for (const QString &root : searchRoots()) {
        const QString path = root + QStringLiteral("/en.json");
        if (loadFile(path, &m_fallback))
            break;
    }

    // Default to system UI language until AppConfig applies language=… / auto.
    loadLanguage(resolveSystemLanguage());
}

QString I18n::resolveSystemLanguage() const
{
    // Prefer QLocale::uiLanguages() (respects LANGUAGE / LC_MESSAGES order).
    const QStringList ui = QLocale::system().uiLanguages();
    for (QString tag : ui) {
        tag.replace(QLatin1Char('-'), QLatin1Char('_'));
        const QString primary = tag.section(QLatin1Char('_'), 0, 0).toLower();
        if (!primary.isEmpty() && m_available.contains(primary))
            return primary;
    }
    const QString fallback =
        QLocale::system().name().section(QLatin1Char('_'), 0, 0).toLower();
    if (m_available.contains(fallback))
        return fallback;
    return QStringLiteral("en");
}

QString I18n::languageName() const
{
    return lookup(QStringLiteral("meta.name"));
}

QString I18n::t(const QString &key, const QVariantList &args) const
{
    QString text = lookup(key);
    for (const QVariant &arg : args)
        text = text.arg(arg.toString());
    return text;
}

QHash<QString, QString> I18n::catalog() const
{
    QHash<QString, QString> merged = m_fallback;
    for (auto it = m_strings.constBegin(); it != m_strings.constEnd(); ++it)
        merged.insert(it.key(), it.value());
    return merged;
}

void I18n::setLanguage(const QString &code)
{
    QString normalized = code.trimmed().toLower();
    if (normalized.isEmpty() || normalized == QLatin1String("auto"))
        normalized = resolveSystemLanguage();
    if (!m_available.contains(normalized))
        normalized = QStringLiteral("en");
    if (normalized == m_language && m_revision > 0)
        return;
    loadLanguage(normalized);
}

void I18n::cycleLanguage()
{
    if (m_available.isEmpty())
        return;
    const int idx = m_available.indexOf(m_language);
    const QString next = m_available.at((idx + 1) % m_available.size());
    loadLanguage(next);
}

void I18n::reload()
{
    discoverAvailable();
    emit catalogsChanged();
    loadLanguage(m_language);
}

QString I18n::lookup(const QString &key) const
{
    if (m_strings.contains(key))
        return m_strings.value(key);
    if (m_fallback.contains(key))
        return m_fallback.value(key);
    return key;
}

bool I18n::loadFile(const QString &path, QHash<QString, QString> *out) const
{
    if (path.isEmpty() || !out)
        return false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return false;
    const QJsonObject obj = doc.object();
    out->clear();
    for (auto it = obj.begin(); it != obj.end(); ++it)
        out->insert(it.key(), it.value().toString());
    return !out->isEmpty();
}

void I18n::discoverAvailable()
{
    m_available.clear();
    QSet<QString> found;
    for (const QString &root : searchRoots()) {
        QDir dir(root);
        if (!dir.exists())
            continue;
        const auto files = dir.entryList({QStringLiteral("*.json")}, QDir::Files);
        for (const QString &file : files) {
            if (file.startsWith(QLatin1Char('_')))
                continue;
            found.insert(QFileInfo(file).completeBaseName().toLower());
        }
    }
    m_available = QStringList(found.begin(), found.end());
    m_available.sort();
    if (!m_available.contains(QStringLiteral("en")))
        m_available.prepend(QStringLiteral("en"));
}

void I18n::loadLanguage(const QString &code)
{
    QHash<QString, QString> loaded;
    for (const QString &root : searchRoots()) {
        const QString path = root + QLatin1Char('/') + code + QStringLiteral(".json");
        if (loadFile(path, &loaded))
            break;
    }
    if (loaded.isEmpty() && code != QLatin1String("en")) {
        loadLanguage(QStringLiteral("en"));
        return;
    }
    m_strings = loaded;
    if (code == QLatin1String("en") && !loaded.isEmpty())
        m_fallback = loaded;
    m_language = code;
    ++m_revision;
    emit languageChanged();
}

QStringList I18n::searchRoots() const
{
    QStringList roots;

    // 1) Embedded Qt resources
    roots << QStringLiteral(":/languages");

    // 2) Next to the running binary (e.g. /usr/share/... or build dir)
    const QString appDir = QCoreApplication::applicationDirPath();
    roots << appDir + QStringLiteral("/languages");
    roots << appDir + QStringLiteral("/../share/omarchy-fb-qt/languages");
    roots << appDir + QStringLiteral("/../languages");

    // Prefer portable relative path from cwd / install layout
    roots << QDir::currentPath() + QStringLiteral("/languages");

#ifdef OMARCHY_FB_SOURCE_DIR
    roots << QStringLiteral(OMARCHY_FB_SOURCE_DIR "/languages");
#endif

    // Deduplicate while preserving order
    QStringList unique;
    QSet<QString> seen;
    for (const QString &r : roots) {
        const QString clean = QDir::cleanPath(r);
        if (seen.contains(clean))
            continue;
        seen.insert(clean);
        unique << clean;
    }
    return unique;
}
