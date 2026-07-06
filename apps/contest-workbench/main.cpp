/**
 * @file main.cpp
 * @brief Contest Trust Workbench 桌面端入口。
 */

#include "CompileController.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    qmlRegisterType<CompileController>("ContestTrust", 1, 0, "CompileController");

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return app.exec();
}
