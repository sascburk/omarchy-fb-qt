#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>

class ThemeLoader : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool darkMode READ darkMode NOTIFY themeChanged)
    Q_PROPERTY(qreal textScale READ textScale WRITE setTextScale NOTIFY textScaleChanged)
    Q_PROPERTY(QString background READ background NOTIFY themeChanged)
    Q_PROPERTY(QString darkerBackground READ darkerBackground NOTIFY themeChanged)
    Q_PROPERTY(QString lighterBackground READ lighterBackground NOTIFY themeChanged)
    Q_PROPERTY(QString foreground READ foreground NOTIFY themeChanged)
    Q_PROPERTY(QString mutedForeground READ mutedForeground NOTIFY themeChanged)
    Q_PROPERTY(QString accent READ accent NOTIFY themeChanged)
    Q_PROPERTY(QString selection READ selection NOTIFY themeChanged)
    Q_PROPERTY(QString muted READ muted NOTIFY themeChanged)
    Q_PROPERTY(QString red READ red NOTIFY themeChanged)
    Q_PROPERTY(QString focusFill READ focusFill NOTIFY themeChanged)
    Q_PROPERTY(QString markFill READ markFill NOTIFY themeChanged)
    Q_PROPERTY(QString mark READ mark NOTIFY themeChanged)

public:
    explicit ThemeLoader(QObject *parent = nullptr);

    bool darkMode() const { return m_darkMode; }
    qreal textScale() const { return m_textScale; }
    QString background() const { return m_background; }
    QString darkerBackground() const { return m_darkerBackground; }
    QString lighterBackground() const { return m_lighterBackground; }
    QString foreground() const { return m_foreground; }
    QString mutedForeground() const { return m_mutedForeground; }
    QString accent() const { return m_accent; }
    QString selection() const { return m_selection; }
    QString muted() const { return m_muted; }
    QString red() const { return m_red; }
    QString focusFill() const { return m_focusFill; }
    QString markFill() const { return m_markFill; }
    QString mark() const { return m_mark; }

    Q_INVOKABLE int scaled(int pixels) const
    {
        return qMax(1, int(qRound(pixels * m_textScale)));
    }

public slots:
    void setDarkMode(bool darkMode);
    void setTextScale(qreal textScale);
    void reload();

signals:
    void themeChanged();
    void textScaleChanged();

private:
    void applyDefaults();
    void loadColorsToml();
    void watchThemePaths();
    void rebuildDerivedColors();
    QString pickUnusedMarkTint() const;
    static bool distinctlyDifferent(const QString &aHex, const QString &bHex);
    static QString mixOnto(const QString &baseHex, const QString &tintHex, qreal amount);

    bool m_darkMode = true;
    qreal m_textScale = 1.0;
    QString m_background;
    QString m_darkerBackground;
    QString m_lighterBackground;
    QString m_foreground;
    QString m_mutedForeground;
    QString m_accent;
    QString m_selection;
    QString m_muted;
    QString m_red;
    QString m_green;
    QString m_yellow;
    QString m_orange;
    QString m_cyan;
    QString m_magenta;
    QString m_brown;
    QString m_focusFill;
    QString m_markFill;
    QString m_mark;
    QString m_markOverride;
    QFileSystemWatcher m_watcher;
};
