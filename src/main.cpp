#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QStandardPaths>
#include <QUrl>

#include <cstdio>

#include "appconfig.h"
#include "dirsizecalculator.h"
#include "fileoperations.h"
#include "filesystemcontroller.h"
#include "i18n.h"
#include "previewprovider.h"
#include "systemtheme.h"
#include "themeloader.h"

namespace {

QtMessageHandler g_previousMessageHandler = nullptr;

bool desktopFileAvailable(const QString &desktopId)
{
    const QString fileName = desktopId + QStringLiteral(".desktop");
    const QStringList roots = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    for (const QString &root : roots) {
        if (QFile::exists(root + QLatin1Char('/') + fileName))
            return true;
    }
    return QFile::exists(QStringLiteral("/usr/share/applications/") + fileName);
}

QFont resolveInterfaceFont()
{
    // Prefer iA Writer Mono; fallbacks reduce noisy script lookup warnings.
    const QStringList candidates{QStringLiteral("iA Writer Mono S"), QStringLiteral("JetBrains Mono"),
                                 QStringLiteral("Noto Sans Mono"), QStringLiteral("monospace")};
    QFont font;
    font.setFamilies(candidates);
    font.setStyleHint(QFont::Monospace);
    return font;
}

void ensureLoggingRule(QByteArray *rules, const char *rule)
{
    if (!rules || !rule || rules->contains(rule))
        return;
    if (!rules->isEmpty() && !rules->endsWith(';'))
        *rules += ';';
    *rules += rule;
}

void quietQtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    const char *cat = context.category ? context.category : "";
    // Drop FFmpeg plugin chatter (banner / hwaccel) regardless of QT_LOGGING_RULES.
    if (qstrncmp(cat, "qt.multimedia.ffmpeg", 20) == 0)
        return;
    if (msg.contains(QLatin1String("VDPAU")) || msg.contains(QLatin1String("libvdpau")))
        return;

    if (g_previousMessageHandler)
        g_previousMessageHandler(type, context, msg);
    else
        fprintf(stderr, "%s\n", qPrintable(msg));
}

} // namespace

int main(int argc, char *argv[])
{
    // App identity must be set before QGuiApplication (xdg-desktop-portal / Wayland app-id).
    QGuiApplication::setApplicationName(QStringLiteral("omarchy-fb-qt"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Omarchy File Browser"));
    QGuiApplication::setOrganizationName(QStringLiteral("Omarchy"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("omarchy.org"));
    if (desktopFileAvailable(QStringLiteral("omarchy-fb-qt")))
        QGuiApplication::setDesktopFileName(QStringLiteral("omarchy-fb-qt"));

    // Merge suppressions even if the session already set QT_LOGGING_RULES.
    {
        QByteArray rules = qgetenv("QT_LOGGING_RULES");
        ensureLoggingRule(&rules, "qt.text.font.db=false");
        ensureLoggingRule(&rules, "qt.multimedia.ffmpeg=false");
        ensureLoggingRule(&rules, "qt.multimedia.ffmpeg.*=false");
        qputenv("QT_LOGGING_RULES", rules);
    }
    // Avoid FFmpeg probing missing NVIDIA VDPAU; software/VAAPI is enough for previews.
    qputenv("QT_FFMPEG_DECODING_HW_DEVICE_TYPES", "vaapi");
    // libvdpau defaults to nvidia on some setups; only "trace" exists here → stops the dlopen error.
    if (qEnvironmentVariableIsEmpty("VDPAU_DRIVER")
        && !QFile::exists(QStringLiteral("/usr/lib/vdpau/libvdpau_nvidia.so"))
        && !QFile::exists(QStringLiteral("/usr/lib64/vdpau/libvdpau_nvidia.so"))) {
        qputenv("VDPAU_DRIVER", "trace");
    }
    g_previousMessageHandler = qInstallMessageHandler(quietQtMessageHandler);

    // FFmpeg video frames render black under Vulkan RHI on Wayland; prefer OpenGL
    // unless the user already chose a backend via QSG_RHI_BACKEND.
    if (qEnvironmentVariableIsEmpty("QSG_RHI_BACKEND"))
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle(QStringLiteral("Material"));

    SystemTheme systemTheme(&app);
    ThemeLoader theme(&app);
    theme.setDarkMode(systemTheme.darkMode());
    QObject::connect(&systemTheme, &SystemTheme::darkModeChanged, &theme, &ThemeLoader::setDarkMode);

    const QFont interfaceFont = resolveInterfaceFont();
    const qreal basePointSize =
        interfaceFont.pointSizeF() > 0 ? interfaceFont.pointSizeF() : app.font().pointSizeF();
    const auto applyInterfaceFont = [&app, interfaceFont, basePointSize](qreal textScale) {
        QFont scaled = interfaceFont;
        scaled.setPointSizeF(basePointSize * textScale);
        app.setFont(scaled);
    };
    applyInterfaceFont(systemTheme.textScale());
    theme.setTextScale(systemTheme.textScale());
    QObject::connect(&systemTheme, &SystemTheme::textScaleChanged, &theme,
                     [&theme, applyInterfaceFont](qreal textScale) {
                         applyInterfaceFont(textScale);
                         theme.setTextScale(textScale);
                     });

    AppConfig config(&app);
    I18n i18n(&app);
    i18n.setLanguage(config.language());
    QObject::connect(&config, &AppConfig::configChanged, &i18n, [&]() {
        // Re-apply language (including auto → current system locale).
        i18n.setLanguage(config.language());
    });

    FileOperations ops(&i18n, &app);
    DirSizeCalculator sizes(&i18n, &app);
    PreviewProvider preview(&i18n, &app);
    FileSystemController controller(&config, &ops, &sizes, &preview, &i18n, &app);

    const QString cliPath = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    controller.initialize(cliPath);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
                     [](const QList<QQmlError> &warnings) {
                         for (const QQmlError &warning : warnings)
                             qWarning().noquote() << warning.toString();
                     });

    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("config"), &config);
    engine.rootContext()->setContextProperty(QStringLiteral("fs"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("i18n"), &i18n);

    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
