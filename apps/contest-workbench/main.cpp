/**
 * @file main.cpp
 * @brief 大学生项目材料审计平台桌面端入口。
 */

#include "CompileController.hpp"

#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickStyle>
#include <QtQml>

#include <cstdio>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("ContestTrust"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("contest-trust.local"));
    QCoreApplication::setApplicationName(QStringLiteral("大学生项目材料审计平台"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

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
    return app.exec();
}
