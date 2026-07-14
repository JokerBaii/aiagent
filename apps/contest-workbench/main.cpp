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
#include <QStringList>
#include <QtQml>

#include <algorithm>
#include <cstdio>
#include <iterator>

namespace {

void loadLocalEnvironment() {
    const QStringList candidates{
        QDir::current().filePath(QStringLiteral(".env")),
        QDir{QCoreApplication::applicationDirPath()}.filePath(QStringLiteral(".env")),
        QDir{QCoreApplication::applicationDirPath()}.filePath(QStringLiteral("../.env")),
        QDir{QCoreApplication::applicationDirPath()}.filePath(QStringLiteral("../../.env"))};
    constexpr const char* allowedNames[]{"LLM_PROVIDER", "DEEPSEEK_API_KEY", "DEEPSEEK_AUTH_TOKEN",
                                         "DEEPSEEK_BASE_URL", "DEEPSEEK_MODEL"};

    for (const auto& candidate : candidates) {
        QFile file{QDir::cleanPath(candidate)};
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        while (!file.atEnd()) {
            auto line = QString::fromUtf8(file.readLine(16 * 1024)).trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
                continue;
            }
            if (line.startsWith(QStringLiteral("export "))) {
                line = line.mid(7).trimmed();
            }
            const auto separator = line.indexOf(QLatin1Char('='));
            if (separator <= 0) {
                continue;
            }
            const auto name = line.left(separator).trimmed();
            const auto allowed =
                std::any_of(std::begin(allowedNames), std::end(allowedNames),
                            [&](const char* item) { return name == QString::fromLatin1(item); });
            if (!allowed || !qEnvironmentVariableIsEmpty(name.toLatin1().constData())) {
                continue;
            }
            auto value = line.mid(separator + 1).trimmed();
            if (value.size() >= 2 &&
                ((value.front() == QLatin1Char('"') && value.back() == QLatin1Char('"')) ||
                 (value.front() == QLatin1Char('\'') && value.back() == QLatin1Char('\'')))) {
                value = value.mid(1, value.size() - 2);
            }
            qputenv(name.toLatin1().constData(), value.toUtf8());
        }
        return;
    }
}

} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("ContestTrust"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("contest-trust.local"));
    QCoreApplication::setApplicationName(QStringLiteral("大学生项目审计与完善平台"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    loadLocalEnvironment();

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
