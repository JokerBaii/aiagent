/**
 * @file main.cpp
 * @brief Contest Trust Workbench 桌面端入口。
 */

#include "CompileController.hpp"

#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QtQml>

#include <cstdio>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
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
