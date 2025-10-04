#include "application.h"

#include <interfaces/management_layer/i_application_manager.h>

// #include <QBreakpadHandler.h>
#include "client/crash_report_database.h"
#include "client/crashpad_client.h"
#include "client/settings.h"
#include "crashpad_paths.h"

#include <QDebug>
#include <QDir>
#include <QPluginLoader>
#include <QStandardPaths>


bool initializeCrashpad(QString dbName, QString appName, QString appVersion)
{
    // Helper class for cross-platform file systems
    CrashpadPaths crashpadPaths;

    // Ensure that crashpad_handler is shipped with your application
    base::FilePath handler(CrashpadPaths::getPlatformString(crashpadPaths.getHandlerPath()));

    // Directory where reports will be saved. Important! Must be writable or crashpad_handler will
    // crash.
    base::FilePath reportsDir(CrashpadPaths::getPlatformString(crashpadPaths.getReportsPath()));

    // Directory where metrics will be saved. Important! Must be writable or crashpad_handler will
    // crash.
    base::FilePath metricsDir(CrashpadPaths::getPlatformString(crashpadPaths.getMetricsPath()));

    // Configure url with your BugSplat database
    QString url = "https://" + dbName + ".bugsplat.com/post/bp/crash/crashpad.php";

    // Configure application name
#if defined(Q_OS_MAC)
    appName = appName + "-mac";
#elif defined(Q_OS_WINDOWS)
    appName = appName + "-win";
#elif defined(Q_OS_LINUX)
    appName = appName + "-linux";
#endif

    // Metadata that will be posted to BugSplat
    QMap<std::string, std::string> annotations;
    annotations["format"] = "minidump"; // Required: Crashpad setting to save crash as a minidump
    annotations["database"] = dbName.toStdString(); // Required: BugSplat database
    annotations["product"] = appName.toStdString(); // Required: BugSplat appName
    annotations["version"] = appVersion.toStdString(); // Required: BugSplat appVersion
    annotations["key"] = "Sample key"; // Optional: BugSplat key field
    annotations["user"] = "fred@bugsplat.com"; // Optional: BugSplat user email
    annotations["list_annotations"] = "Sample comment"; // Optional: BugSplat crash description

    // Disable crashpad rate limiting so that all crashes have dmp files
    std::vector<std::string> arguments;
    arguments.push_back("--no-rate-limit");

    // Initialize crashpad database
    std::unique_ptr<crashpad::CrashReportDatabase> database
        = crashpad::CrashReportDatabase::Initialize(reportsDir);
    if (database == nullptr) {
        return false;
    }

    // Enable automated crash uploads
    crashpad::Settings* settings = database->GetSettings();
    if (settings == nullptr) {
        return false;
    }
    settings->SetUploadsEnabled(false);

    // Attachments to be uploaded alongside the crash - default bundle size limit is 20MB
    std::vector<base::FilePath> attachments;
    base::FilePath attachment(CrashpadPaths::getPlatformString(crashpadPaths.getAttachmentPath()));
#if defined(Q_OS_WINDOWS) || defined(Q_OS_LINUX)
    // Crashpad hasn't implemented attachments on OS X yet
    attachments.push_back(attachment);
#endif

    // Start crash handler
    crashpad::CrashpadClient* client = new crashpad::CrashpadClient();
    bool status = client->StartHandler(handler, reportsDir, metricsDir, url.toStdString(),
                                       annotations.toStdMap(), arguments, true, true, attachments);
    return status;
}

using ManagementLayer::IApplicationManager;

/**
 * @brief Загрузить менеджер приложения
 */
QObject* loadApplicationManager()
{
    //
    // Смотрим папку с данными приложения на компе
    // NOTE: В Debug-режим работает с папкой сборки приложения
    //
    // TODO: Когда дорастём включить этот функционал, плюс продумать, как быть в режиме разработки
    //
    const QString pluginsDirName = "plugins";
    QDir pluginsDir(
        //#ifndef QT_NO_DEBUG
        QApplication::applicationDirPath()
        //#else
        //                QStandardPaths::writableLocation(QStandardPaths::DataLocation)
        //#endif
    );

#if defined(Q_OS_MAC)
    pluginsDir.cdUp();
#endif

    //
    // Если там нет плагинов приложения
    //
    if (!pluginsDir.cd(pluginsDirName)) {
        //
        // ... создаём локальную папку с плагинами
        //
        pluginsDir.mkpath(pluginsDir.absoluteFilePath(pluginsDirName));
        pluginsDir.cd(pluginsDirName);
        //
        // ... и копируем туда все плагины из папки, в которую установлено приложение
        //
        QDir installedPluginsDir(QApplication::applicationDirPath());
        installedPluginsDir.cd(pluginsDirName);
        for (const auto& file : installedPluginsDir.entryList(QDir::Files)) {
            QFile::copy(installedPluginsDir.absoluteFilePath(file),
                        pluginsDir.absoluteFilePath(file));
        }
    }
    //
    // Если плагины есть и если есть обновления
    //
    else {
        //
        // ... корректируем названия файлов для использования обновлённых версий
        //
        for (const QFileInfo& fileName : pluginsDir.entryInfoList({ "*.update" }, QDir::Files)) {
            QFile::rename(fileName.absoluteFilePath(),
                          fileName.absoluteFilePath().remove(".update"));
        }
    }

    //
    // Подгружаем плагин
    //
    const QString extensionFilter =
#ifdef Q_OS_WIN
        ".dll";
#elif defined(Q_OS_LINUX)
        ".so";
#elif defined(Q_OS_MAC)
        ".dylib";
#else
        "";
#endif
    const QStringList libCorePluginEntries
        = pluginsDir.entryList({ "*coreplugin*" + extensionFilter }, QDir::Files);
    if (libCorePluginEntries.isEmpty()) {
        qCritical() << "Core plugin isn't found";
        return nullptr;
    }
    if (libCorePluginEntries.size() > 1) {
        qCritical() << "Found more than 1 core plugins";
        return nullptr;
    }

    const auto pluginPath = libCorePluginEntries.first();
    QPluginLoader pluginLoader(pluginsDir.absoluteFilePath(pluginPath));
    QObject* plugin = pluginLoader.instance();
    if (plugin == nullptr) {
        qDebug() << pluginLoader.errorString();
    }

    return plugin;
}

/**
 * @brief Погнали!
 */
int main(int argc, char* argv[])
{
    //
    // Инициилизируем crashpad
    //
    QString dbName = "starc-desktop";
    QString appName = "starcapp";
    QString appVersion = "0.8.0";
    initializeCrashpad(dbName, appName, appVersion);

    //
    // Инициилизируем приложение
    //
    Application application(argc, argv);

    //
    // Загружаем менеджера приложения
    //
    auto applicationManager = loadApplicationManager();
    if (applicationManager == nullptr) {
        return 1;
    }

    //
    // Настраиваем сборщик крашдампов
    //
#if 0
    const auto crashReportsFolderPath
        = QString("%1/crashreports")
              .arg(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    QBreakpadInstance.init(
        crashReportsFolderPath,
        qobject_cast<ManagementLayer::IApplicationManager*>(applicationManager)->logFilePath());
#endif
    //
    // Устанавливаем менеджера в приложение и запускаемся
    //
    application.setApplicationManager(applicationManager);
    application.startUp();

//    *(volatile int *)0 = 0;

    return application.exec();
}
