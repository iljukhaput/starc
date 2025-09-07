#include "application.h"

#include <interfaces/management_layer/i_application_manager.h>

//#include <QBreakpadHandler.h>
#include <QDebug>
#include <QDir>
#include <QPluginLoader>
#include <QStandardPaths>


#include "client/crash_report_database.h"
#include "client/crashpad_client.h"
#include "client/settings.h"
#include "crashpad_paths.h"

#if defined(Q_OS_WIN)
    #define NOMINMAX
    #include <windows.h>
    #include <filesystem>
#endif

#if defined(Q_OS_MAC)
    #include <mach-o/dyld.h>
#endif

#if defined(Q_OS_LINUX)
    #include <unistd.h>
    #define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif


QString getExecutableDir() {
#if defined(Q_OS_MAC)
    unsigned int bufferSize = 512;
    std::vector<char> buffer(bufferSize + 1);

    if(_NSGetExecutablePath(&buffer[0], &bufferSize))
    {
        buffer.resize(bufferSize);
        _NSGetExecutablePath(&buffer[0], &bufferSize);
    }

    char* lastForwardSlash = strrchr(&buffer[0], '/');
    if (lastForwardSlash == NULL) return NULL;
    *lastForwardSlash = 0;

    return &buffer[0];
#elif defined(Q_OS_WINDOWS)
    HMODULE hModule = GetModuleHandleW(NULL);
    WCHAR path[MAX_PATH];
    DWORD retVal = GetModuleFileNameW(hModule, path, MAX_PATH);
    if (retVal == 0) return NULL;

    wchar_t *lastBackslash = wcsrchr(path, '\\');
    if (lastBackslash == NULL) return NULL;
    *lastBackslash = 0;

    return QString::fromWCharArray(path);
#elif defined(Q_OS_LINUX)
    char pBuf[FILENAME_MAX];
    int len = sizeof(pBuf);
    int bytes = MIN(readlink("/proc/self/exe", pBuf, len), len - 1);
    if (bytes >= 0) {
        pBuf[bytes] = '\0';
    }

    char* lastForwardSlash = strrchr(&pBuf[0], '/');
    if (lastForwardSlash == NULL) return NULL;
    *lastForwardSlash = '\0';

    return QString::fromStdString(pBuf);
#else
    #error getExecutableDir not implemented on this platform
#endif
}

bool initializeCrashpad(QString dbName, QString appName, QString appVersion)
{
    // Get directory where the exe lives so we can pass a full path to handler, reportsDir and
    // metricsDir
    QString exeDir = getExecutableDir();

    // Helper class for cross-platform file systems
    Paths crashpadPaths(exeDir);

    // Ensure that crashpad_handler is shipped with your application
    base::FilePath handler(Paths::getPlatformString(crashpadPaths.getHandlerPath()));

    // Directory where reports will be saved. Important! Must be writable or crashpad_handler will
    // crash.
    base::FilePath reportsDir(Paths::getPlatformString(crashpadPaths.getReportsPath()));

    // Directory where metrics will be saved. Important! Must be writable or crashpad_handler will
    // crash.
    base::FilePath metricsDir(Paths::getPlatformString(crashpadPaths.getMetricsPath()));

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
    if (database == NULL)
        return false;

    // Enable automated crash uploads
    crashpad::Settings* settings = database->GetSettings();
    if (settings == NULL)
        return false;
    settings->SetUploadsEnabled(true);

    // Attachments to be uploaded alongside the crash - default bundle size limit is 20MB
    std::vector<base::FilePath> attachments;
    base::FilePath attachment(Paths::getPlatformString(crashpadPaths.getAttachmentPath()));
#if defined(Q_OS_WINDOWS) || defined(Q_OS_LINUX)
    // Crashpad hasn't implemented attachments on OS X yet
    attachments.push_back(attachment);
#endif

    // Start crash handler
    crashpad::CrashpadClient* client = new crashpad::CrashpadClient();
    bool status = client->StartHandler(handler, reportsDir, metricsDir, url.toStdString(),
                                       annotations.toStdMap(), arguments, true, true, attachments);
#ifdef _WIN32
    // Register WER module after starting the handler
    if (status) {
        std::string werDllPath = exeDir.toStdString() + "\\crashpad_wer.dll";
        qDebug() << "Looking for Crashpad WER DLL at: " + QString::fromStdString(werDllPath);

        if (std::filesystem::exists(werDllPath)) {
            qDebug() << "Crashpad WER DLL found, registering with Crashpad...";
            std::wstring werDllPathW(werDllPath.begin(), werDllPath.end());
            if (client->RegisterWerModule(werDllPathW)) {
                qDebug() << "Successfully registered WER module: " + QString::fromStdString(werDllPath);
                qDebug() << "WER callbacks are now active for enhanced crash detection!";
            } else {
                qDebug() << "Failed to register WER module: " + QString::fromStdString(werDllPath);
            }
        } else {
            qDebug() << "Crashpad WER DLL not found - continuing without WER integration";
        }
    }
#endif
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
