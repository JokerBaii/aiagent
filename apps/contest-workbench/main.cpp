/**
 * @file main.cpp
 * @brief 大学生项目审计与完善平台桌面端入口。
 */

#include "CompileController.hpp"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QtQml>

#include <cstdio>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("ContestTrust"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("contest-trust.local"));
    QCoreApplication::setApplicationName(QStringLiteral("大学生项目审计与完善平台"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("大学生项目材料审计平台"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption projectOption({QStringLiteral("p"), QStringLiteral("project")},
                                     QStringLiteral("启动后立即导入并审计项目路径。"),
                                     QStringLiteral("path"));
    parser.addOption(projectOption);
    parser.process(app);

    if (qEnvironmentVariableIsEmpty("CONTEST_WORKSPACE_ROOT")) {
        const auto workspaceRoot =
            QDir{QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)}.filePath(
                QStringLiteral("workspaces"));
        qputenv("CONTEST_WORKSPACE_ROOT", QFile::encodeName(workspaceRoot));
    }

    qmlRegisterType<CompileController>("ContestTrust", 1, 0, "CompileController");

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings,
                     [](const QList<QQmlError>& warnings) {
                         for (const auto& warning : warnings) {
                             qWarning().noquote() << warning.toString();
                         }
                     });
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        std::fprintf(stderr, "Failed to load qrc:/qml/Main.qml\n");
        return 1;
    }
    if (parser.isSet(projectOption)) {
        if (auto* controller = engine.rootObjects().constFirst()->findChild<CompileController*>()) {
            controller->selectProject(parser.value(projectOption));
        }
    }
    return app.exec();
}
