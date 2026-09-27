#include "themeloader.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QtMath>

ThemeLoader::ThemeLoader(QObject *parent)
    : QObject(parent)
{
    applyDefaults();
    loadColorsToml();
    watchThemePaths();

    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this]() {
        loadColorsToml();
        watchThemePaths();
    });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this]() {
        loadColorsToml();
        watchThemePaths();
    });
}

void ThemeLoader::setDarkMode(bool darkMode)
{
    if (m_darkMode == darkMode)
        return;
    m_darkMode = darkMode;
    loadColorsToml();
}

void ThemeLoader::setTextScale(qreal textScale)
{
    if (qFuzzyCompare(m_textScale, textScale))
        return;
    m_textScale = textScale;
    emit textScaleChanged();
}

void ThemeLoader::reload()
{
    loadColorsToml();
    watchThemePaths();
}

QString ThemeLoader::mixOnto(const QString &baseHex, const QString &tintHex, qreal amount)
{
    const QColor base(baseHex);
    const QColor tint(tintHex);
    if (!base.isValid() || !tint.isValid())
        return tintHex;
    amount = qBound(0.0, amount, 1.0);
    QColor out;
    out.setRgbF(base.redF() * (1.0 - amount) + tint.redF() * amount,
                base.greenF() * (1.0 - amount) + tint.greenF() * amount,
                base.blueF() * (1.0 - amount) + tint.blueF() * amount);
    return out.name(QColor::HexRgb);
}

bool ThemeLoader::distinctlyDifferent(const QString &aHex, const QString &bHex)
{
    const QColor a(aHex);
    const QColor b(bHex);
    if (!a.isValid() || !b.isValid())
        return false;
    if (a.name(QColor::HexRgb).compare(b.name(QColor::HexRgb), Qt::CaseInsensitive) == 0)
        return false;

    // Prefer hue distance; also accept strong lightness / chroma differences.
    if (a.hsvSaturationF() > 0.08 && b.hsvSaturationF() > 0.08) {
        const qreal dist = qAbs(a.hsvHueF() - b.hsvHueF());
        const qreal hueDist = qMin(dist, 1.0 - dist);
        if (hueDist >= 0.10)
            return true;
    }
    const qreal dr = a.redF() - b.redF();
    const qreal dg = a.greenF() - b.greenF();
    const qreal db = a.blueF() - b.blueF();
    return (dr * dr + dg * dg + db * db) >= 0.08;
}

QString ThemeLoader::pickUnusedMarkTint() const
{
    // Colors the UI does not otherwise use (accent/selection/muted/red/green/fg/bg).
    // Prefer magenta → cyan → yellow → brown → orange.
    if (!m_markOverride.isEmpty())
        return m_markOverride;

    const QStringList candidates{m_magenta, m_cyan, m_yellow, m_brown, m_orange};
    for (const QString &candidate : candidates) {
        if (candidate.isEmpty())
            continue;
        if (!distinctlyDifferent(candidate, m_accent))
            continue;
        if (!distinctlyDifferent(candidate, m_red))
            continue;
        // Skip if identical to green (sometimes themes alias colors).
        if (!distinctlyDifferent(candidate, m_green) && distinctlyDifferent(m_green, m_accent))
            continue;
        return candidate;
    }

    // Fallbacks if every candidate collides with accent.
    for (const QString &candidate : candidates) {
        if (!candidate.isEmpty() && distinctlyDifferent(candidate, m_accent))
            return candidate;
    }
    if (!m_magenta.isEmpty())
        return m_magenta;
    if (!m_cyan.isEmpty())
        return m_cyan;
    if (!m_yellow.isEmpty())
        return m_yellow;
    return m_darkMode ? QStringLiteral("#c678dd") : QStringLiteral("#a626a4");
}

void ThemeLoader::rebuildDerivedColors()
{
    const QString base = m_darkerBackground.isEmpty() ? m_background : m_darkerBackground;

    m_focusFill = mixOnto(base, m_accent, m_darkMode ? 0.38 : 0.28);

    m_mark = pickUnusedMarkTint();
    m_markFill = mixOnto(base, m_mark, m_darkMode ? 0.42 : 0.32);
}

void ThemeLoader::applyDefaults()
{
    m_markOverride.clear();
    if (m_darkMode) {
        m_background = QStringLiteral("#101010");
        m_darkerBackground = QStringLiteral("#090909");
        m_lighterBackground = QStringLiteral("#1e1e1e");
        m_foreground = QStringLiteral("#eeeeee");
        m_mutedForeground = QStringLiteral("#8a8a8d");
        m_accent = QStringLiteral("#5584aa");
        m_selection = QStringLiteral("#186a9a");
        m_muted = QStringLiteral("#333333");
        m_red = QStringLiteral("#D35F5F");
        m_green = QStringLiteral("#5fad6e");
        m_yellow = QStringLiteral("#d4a017");
        m_orange = QStringLiteral("#d08770");
        m_cyan = QStringLiteral("#4db6ac");
        m_magenta = QStringLiteral("#c678dd");
        m_brown = QStringLiteral("#8b6914");
    } else {
        m_background = QStringLiteral("#ffffff");
        m_darkerBackground = QStringLiteral("#f0f0f0");
        m_lighterBackground = QStringLiteral("#f7f7f7");
        m_foreground = QStringLiteral("#222324");
        m_mutedForeground = QStringLiteral("#666666");
        m_accent = QStringLiteral("#2077b2");
        m_selection = QStringLiteral("#2077b2");
        m_muted = QStringLiteral("#dddddd");
        m_red = QStringLiteral("#b91c1c");
        m_green = QStringLiteral("#2e7d32");
        m_yellow = QStringLiteral("#b8860b");
        m_orange = QStringLiteral("#d97706");
        m_cyan = QStringLiteral("#0d9488");
        m_magenta = QStringLiteral("#a626a4");
        m_brown = QStringLiteral("#7c4a03");
    }
    rebuildDerivedColors();
}

void ThemeLoader::loadColorsToml()
{
    applyDefaults();

    const QString colorsPath =
        QDir::homePath() + QStringLiteral("/.local/state/omarchy/current/theme/colors.toml");
    QString themeMode;

    QFile file(colorsPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                continue;

            const int equals = line.indexOf(QLatin1Char('='));
            if (equals < 0)
                continue;

            const QString key = line.left(equals).trimmed();
            QString value = line.mid(equals + 1).trimmed();
            if (value.size() >= 2
                && ((value.front() == QLatin1Char('"') && value.back() == QLatin1Char('"'))
                    || (value.front() == QLatin1Char('\'') && value.back() == QLatin1Char('\'')))) {
                value = value.mid(1, value.size() - 2);
            }

            if (key == QStringLiteral("mode"))
                themeMode = value;
            else if (key == QStringLiteral("background"))
                m_background = value;
            else if (key == QStringLiteral("darker_background"))
                m_darkerBackground = value;
            else if (key == QStringLiteral("dark_background"))
                m_darkerBackground = value;
            else if (key == QStringLiteral("lighter_background"))
                m_lighterBackground = value;
            else if (key == QStringLiteral("foreground"))
                m_foreground = value;
            else if (key == QStringLiteral("light_foreground"))
                m_mutedForeground = value;
            else if (key == QStringLiteral("accent"))
                m_accent = value;
            else if (key == QStringLiteral("selection"))
                m_selection = value;
            else if (key == QStringLiteral("muted"))
                m_muted = value;
            else if (key == QStringLiteral("red"))
                m_red = value;
            else if (key == QStringLiteral("green"))
                m_green = value;
            else if (key == QStringLiteral("yellow"))
                m_yellow = value;
            else if (key == QStringLiteral("orange"))
                m_orange = value;
            else if (key == QStringLiteral("cyan"))
                m_cyan = value;
            else if (key == QStringLiteral("magenta"))
                m_magenta = value;
            else if (key == QStringLiteral("brown"))
                m_brown = value;
            else if (key == QStringLiteral("mark"))
                m_markOverride = value;
        }
    }

    bool themeModeKnown = false;
    bool themeIsDark = m_darkMode;
    if (themeMode == QStringLiteral("dark")) {
        themeIsDark = true;
        themeModeKnown = true;
    } else if (themeMode == QStringLiteral("light")) {
        themeIsDark = false;
        themeModeKnown = true;
    } else {
        const QColor background(m_background);
        if (background.isValid()) {
            const double luminance = 0.299 * background.redF() + 0.587 * background.greenF()
                + 0.114 * background.blueF();
            themeIsDark = luminance < 0.5;
            themeModeKnown = true;
        }
    }

    if (themeModeKnown && themeIsDark != m_darkMode)
        m_darkMode = themeIsDark;

    rebuildDerivedColors();
    emit themeChanged();
}

void ThemeLoader::watchThemePaths()
{
    const QStringList watched = m_watcher.files() + m_watcher.directories();
    if (!watched.isEmpty())
        m_watcher.removePaths(watched);

    const QString currentDir = QDir::homePath() + QStringLiteral("/.local/state/omarchy/current");
    const QString themeDir = currentDir + QStringLiteral("/theme");
    const QString colorsPath = themeDir + QStringLiteral("/colors.toml");

    if (QDir(currentDir).exists())
        m_watcher.addPath(currentDir);
    if (QDir(themeDir).exists())
        m_watcher.addPath(themeDir);
    if (QFile::exists(colorsPath))
        m_watcher.addPath(colorsPath);
}
