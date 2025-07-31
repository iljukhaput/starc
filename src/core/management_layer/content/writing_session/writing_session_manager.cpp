#include "writing_session_manager.h"

#include "writing_session_storage.h"

#include <business_layer/plots/abstract_plot.h>
#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <ui/design_system/design_system.h>
#include <ui/session_statistics/session_statistics_navigator.h>
#include <ui/session_statistics/session_statistics_tool_bar.h>
#include <ui/session_statistics/session_statistics_view.h>
#include <ui/writing_session/writing_sprint_panel.h>
#include <utils/helpers/color_helper.h>

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QKeyEvent>
#include <QStandardPaths>
#include <QUuid>
#include <QWidget>


namespace ManagementLayer {

class WritingSessionManager::Implementation
{
public:
    Implementation(WritingSessionManager* _q, QWidget* _parentWidget);

    /**
     * @brief Сохранить информацию о текущей сессии
     */
    void saveCurrentSession(const QDateTime& _sessionEndedAt = {});

    /**
     * @brief Облатоботать статистику
     */
    void processStatistics();


    WritingSessionManager* q = nullptr;

    Ui::SessionStatisticsToolBar* toolBar = nullptr;
    Ui::SessionStatisticsNavigator* navigator = nullptr;
    Ui::SessionStatisticsView* view = nullptr;

    DataStorageLayer::WritingSessionStorage sessionStorage;

    QUuid sessionUuid;
    QUuid projectUuid;
    QString projectName;
    QDateTime sessionStartedAt;
    bool isCountingEnabled = true;
    int words = 0;
    int characters = 0;
    bool isLastCharacterSpace = true;

    /**
     * @brief Аккумулированные данные за последние 30 дней <min,max>
     */
    struct {
        QPair<std::chrono::seconds, std::chrono::seconds> duration;
        QPair<int, int> words;
    } last30DaysOverview;

    /**
     * @brief Графики статистики
     */
    BusinessLayer::Plot plot;

    bool isSprintStarted = false;
    int sprintWords = 0;

    Ui::WritingSprintPanel* writingSprintPanel = nullptr;
};

WritingSessionManager::Implementation::Implementation(WritingSessionManager* _q,
                                                      QWidget* _parentWidget)
    : q(_q)
    , toolBar(new Ui::SessionStatisticsToolBar(_parentWidget))
    , navigator(new Ui::SessionStatisticsNavigator(_parentWidget))
    , view(new Ui::SessionStatisticsView(_parentWidget))
    , writingSprintPanel(new Ui::WritingSprintPanel(_parentWidget))
{
    toolBar->hide();
    navigator->hide();
    view->hide();
    writingSprintPanel->hide();
}

void WritingSessionManager::Implementation::saveCurrentSession(const QDateTime& _sessionEndedAt)
{
    if (sessionUuid.isNull()) {
        return;
    }

    auto sessionEndsAt
        = _sessionEndedAt.isValid() ? _sessionEndedAt : QDateTime::currentDateTimeUtc();

    //
    // FIXME: Потенциально тут в файл может записаться хрень, если перед выключением компьютера были
    //        запущены несколько копий приложения и они начнут писать одновременно
    //
    const Domain::SessionStatistics session{
        sessionUuid,
        projectUuid,
        projectName,
        settingsValue(DataStorageLayer::kDeviceUuidKey).toString(),
        QSysInfo::prettyProductName(),
        sessionStartedAt,
        sessionEndsAt,
        words,
        characters,
    };
    sessionStorage.saveSessionStatistics({ session });

    //
    // Уведомляем клиентов о том, что была создаана новая сессия
    //
    emit q->sessionStatisticsPublishRequested({ session });
}

void WritingSessionManager::Implementation::processStatistics()
{
    QVector<Domain::SessionStatistics> sessionStatistics = sessionStorage.sessionStatistics();
    if (sessionStatistics.isEmpty()) {
        return;
    }

    //
    // Бежим по статистике и формируем график
    //
    // ... х - общий для всех
    QVector<uint> x;
    // ... y
    QVector<int> summaryY;
    QHash<QString, QVector<int>> deviceY;
    QHash<QString, QString> deviceNames;
    QPair<std::chrono::seconds, std::chrono::seconds> durationOverview
        = { std::chrono::hours{ 24 }, {} };
    QPair<int, int> wordsOverview = { std::numeric_limits<int>::max(), 0 };
    //
    // ... сначала собираем полный х, список девайсов и сводку
    //
    uint lastX = 0;
    const auto overviewStart = QDateTime::currentDateTime().addDays(-30);
    bool hasStats = false;
    for (const auto& session : std::as_const(sessionStatistics)) {
        const auto newX = session.startDateTime.date().startOfDay().toSecsSinceEpoch();
        if (newX != lastX) {
            x.append(newX);
            lastX = newX;
        }

        if (!deviceY.contains(session.deviceUuid)) {
            deviceY.insert(session.deviceUuid, {});
        }

        deviceNames[session.deviceUuid] = session.deviceName;

        if (overviewStart < session.startDateTime) {
            //
            // ... не учитываем сессии короче 3х минут
            //
            const auto sessionDuration
                = std::chrono::seconds{ session.startDateTime.secsTo(session.endDateTime) };
            if (sessionDuration > std::chrono::minutes{ 3 }) {
                durationOverview.first = std::min(durationOverview.first, sessionDuration);
                durationOverview.second = std::max(durationOverview.second, sessionDuration);
            }

            //
            // ... не учитываем сессии без словк
            //
            if (session.words > 0) {
                wordsOverview.first = std::min(wordsOverview.first, session.words);
                wordsOverview.second = std::max(wordsOverview.second, session.words);
            }

            //
            // ... фиксируем момент, что нашли стату
            //
            hasStats = true;
        }
    }
    if (!hasStats) {
        durationOverview = { {}, {} };
        wordsOverview = { 0, 0 };
    }
    last30DaysOverview.duration = durationOverview;
    last30DaysOverview.words = wordsOverview;

    //
    // ... затем собираем детальную стату по девайсам
    //
    int sessionsStatisticsIndex = 0;
    QMap<qreal, QStringList> info;
    for (const auto nextX : x) {
        summaryY.append(0);
        for (auto& y : deviceY) {
            y.append(0);
        }

        for (; sessionsStatisticsIndex < sessionStatistics.size(); ++sessionsStatisticsIndex) {
            const auto& session = sessionStatistics[sessionsStatisticsIndex];
            const auto newX = session.startDateTime.date().startOfDay().toSecsSinceEpoch();
            if (newX != nextX) {
                break;
            }

            deviceY[session.deviceUuid].last() += session.words;
            summaryY.last() += session.words;
        }

        const auto infoTitle = QDateTime::fromSecsSinceEpoch(nextX).toString("dd.MM.yyyy");
        QString infoText;
        for (auto iter = deviceNames.begin(); iter != deviceNames.end(); ++iter) {
            const auto words = deviceY[iter.key()].constLast();
            if (words > 0) {
                infoText.append(QString("%1: %2\n").arg(iter.value()).arg(words));
            }
        }
        infoText.append(QString("%1: %2").arg(tr("Total words")).arg(summaryY.constLast()));
        info.insert(nextX, QStringList{ infoTitle, infoText });
    }
    //
    // ... формируем графики
    //
    plot = {};
    plot.info = info;
    if (view->showDevices()) {
        int plotIndex = 0;
        QVector<qreal> lastY;
        for (auto iter = deviceNames.begin(); iter != deviceNames.end(); ++iter) {
            BusinessLayer::PlotData data;
            data.name = iter.value();
            data.color = ColorHelper::forNumber(plotIndex++);
            data.brushColor
                = ColorHelper::transparent(data.color, Ui::DesignSystem::elevationEndOpacity());
            data.x = { x.begin(), x.end() };
            data.y = { deviceY[iter.key()].begin(), deviceY[iter.key()].end() };
            for (auto i = 0; i < lastY.size(); ++i) {
                data.y[i] += lastY[i];
            }
            plot.data.prepend(data);

            lastY = data.y;
        }
    } else {
        BusinessLayer::PlotData data;
        data.color = Ui::DesignSystem::color().accent();
        data.brushColor
            = ColorHelper::transparent(data.color, Ui::DesignSystem::elevationEndOpacity());
        data.x = { x.begin(), x.end() };
        data.y = { summaryY.begin(), summaryY.end() };
        plot.data.append(data);
    }
}


// ****


WritingSessionManager::WritingSessionManager(QObject* _parent, QWidget* _parentWidget)
    : QObject(_parent)
    , d(new Implementation(this, _parentWidget))
{
    connect(d->toolBar, &Ui::SessionStatisticsToolBar::backPressed, this,
            &WritingSessionManager::closeSessionStatisticsRequested);
    connect(d->navigator, &Ui::SessionStatisticsNavigator::aboutToBeAppeared, this, [this] {
        d->navigator->setCurrentSessionDetails(d->words, d->sessionStartedAt.toLocalTime());

        d->processStatistics();
        d->navigator->set30DaysOverviewDetails(
            d->last30DaysOverview.duration.first, d->last30DaysOverview.duration.second,
            d->last30DaysOverview.words.first, d->last30DaysOverview.words.second);
        d->view->setPlot(d->plot);
    });
    connect(d->view, &Ui::SessionStatisticsView::viewPreferencesChanged, this, [this] {
        d->processStatistics();
        d->view->setPlot(d->plot);
    });

    connect(d->writingSprintPanel, &Ui::WritingSprintPanel::sprintStarted, this, [this] {
        d->isSprintStarted = true;
        d->sprintWords = 0;
        d->isLastCharacterSpace = true;
    });
    connect(d->writingSprintPanel, &Ui::WritingSprintPanel::sprintFinished, this, [this] {
        d->isSprintStarted = false;
        d->writingSprintPanel->setResult(d->sprintWords);
    });
}

WritingSessionManager::~WritingSessionManager() = default;

QWidget* WritingSessionManager::toolBar() const
{
    return d->toolBar;
}

QWidget* WritingSessionManager::navigator() const
{
    return d->navigator;
}

QWidget* WritingSessionManager::view() const
{
    return d->view;
}

void WritingSessionManager::addKeyPressEvent(QKeyEvent* _event)
{
    //
    // Обрабатываем событие только в случае, если
    // - виджет в фокусе
    // - можно вводить текст
    // - включён подсчёт статистики
    //
    if (QApplication::focusWidget()
        && !QApplication::focusWidget()->testAttribute(Qt::WA_InputMethodEnabled)
        && !d->isCountingEnabled) {
        return;
    }

    //
    // Для случая, когда сессия была прервана по долгому ожиданию, возобновляем её
    //
    if (!d->projectUuid.isNull() && !d->sessionStartedAt.isValid()) {
        d->sessionUuid = QUuid::createUuid();
        d->sessionStartedAt = QDateTime::currentDateTimeUtc();
    }

    //
    // Пробел и переносы строк интерпретируем, как разрыв слова
    //
    if (_event->key() == Qt::Key_Space || _event->key() == Qt::Key_Tab
        || _event->key() == Qt::Key_Enter || _event->key() == Qt::Key_Return) {
        if (!d->isLastCharacterSpace) {
            d->isLastCharacterSpace = true;
            ++d->words;
            ++d->sprintWords;
        }
    }
    //
    // В остальных случаях, если есть текст, накручиваем счётчики
    //
    else if (!_event->text().isEmpty()) {
        if (d->isLastCharacterSpace) {
            d->isLastCharacterSpace = false;
        }
        d->characters += _event->text().length();
    }
}

QDateTime WritingSessionManager::sessionStatisticsLastSyncDateTime() const
{
    return d->sessionStorage.sessionStatisticsLastSyncDateTime();
}

void WritingSessionManager::startSession(const QUuid& _projectUuid, const QString& _projectName)
{
    d->sessionUuid = QUuid::createUuid();
    d->projectUuid = _projectUuid;
    d->projectName = _projectName;
    d->sessionStartedAt = QDateTime::currentDateTimeUtc();
}

void WritingSessionManager::setCountingEnabled(bool _enabled)
{
    d->isCountingEnabled = _enabled;
}

void WritingSessionManager::splitSession(const QDateTime& _lastActionAt)
{
    d->saveCurrentSession(_lastActionAt);

    d->sessionUuid = {};
    d->sessionStartedAt = {};
    d->words = 0;
    d->characters = 0;
    d->isLastCharacterSpace = true;
    d->sprintWords = 0;
}

void WritingSessionManager::finishSession()
{
    d->saveCurrentSession();

    d->sessionUuid = {};
    d->projectUuid = {};
    d->projectName.clear();
    d->sessionStartedAt = {};
    d->words = 0;
    d->characters = 0;
    d->isLastCharacterSpace = true;
    d->sprintWords = 0;
}

void WritingSessionManager::showSprintPanel()
{
    d->writingSprintPanel->showPanel();
}

void WritingSessionManager::setSessionStatistics(
    const QVector<Domain::SessionStatistics>& _sessionStatistics, bool _ableToShowDeatils)
{
    //
    // Сперва отправим на сервер все сессии с момента последней синхронизации
    //
    emit sessionStatisticsPublishRequested(
        d->sessionStorage.sessionStatistics(d->sessionStorage.sessionStatisticsLastSyncDateTime()));

    //
    // Запоминаем дату текущей синхронизации сессий и записываем сессии
    //
    d->sessionStorage.saveSessionStatisticsLastSyncDateTime(QDateTime::currentDateTimeUtc());
    d->sessionStorage.saveSessionStatistics(_sessionStatistics);

    //
    // Настраиваем доступность просмотра детальной статистики
    //
    d->view->setAbleToShowDetails(_ableToShowDeatils);
}

} // namespace ManagementLayer
