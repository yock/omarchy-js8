// js8 — a small JS8 waterfall receiver for Omarchy.

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>
#include <QUrl>

#include "backend.h"
#include "theme.h"
#include "translator.h"
#include "waterfallitem.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("js8"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));

    // Matches the window to js8.desktop so the compositor's Wayland app id and
    // the launcher pick up the installed icon.
    app.setDesktopFileName(QStringLiteral("js8"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("js8")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("JS8 waterfall receiver"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption demoOption(QStringLiteral("demo"),
                                        QStringLiteral("Start with the built-in demo signal."));
    const QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                              QStringLiteral("Grab the window to <path> and exit."),
                                              QStringLiteral("path"));
    const QCommandLineOption waitOption(QStringLiteral("wait"),
                                        QStringLiteral("Seconds to wait before the screenshot (default 26)."),
                                        QStringLiteral("seconds"), QStringLiteral("26"));
    parser.addOption(demoOption);
    parser.addOption(screenshotOption);
    parser.addOption(waitOption);
    parser.process(app);

    QQuickStyle::setStyle(QStringLiteral("Material"));

    // raylib draws through rlgl into a QQuickFramebufferObject, so Qt Quick
    // must be using the OpenGL scene graph backend.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    qmlRegisterType<WaterfallItem>("Js8", 1, 0, "WaterfallItem");

    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Backend backend;

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    // The footer's status is built in C++, so re-read it on a language switch
    // as well as re-evaluating the QML strings.
    QObject::connect(&translator, &Translator::changed, &backend, &Backend::statusChanged);
    engine.rootContext()->setContextProperty(QStringLiteral("translator"), &translator);
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (!window)
        return 1;

    if (parser.isSet(demoOption) || parser.isSet(screenshotOption))
        backend.setDemo(true);

    if (parser.isSet(screenshotOption)) {
        const QString path = parser.value(screenshotOption);
        const int seconds = qMax(1, parser.value(waitOption).toInt());
        QTimer::singleShot(seconds * 1000, [window, path, &app] {
            const QImage image = window->grabWindow();
            const bool saved = !image.isNull() && image.save(path);
            if (!saved)
                qWarning("js8: could not save screenshot to %s", qPrintable(path));
            app.exit(saved ? 0 : 1);
        });
    }

    return app.exec();
}
