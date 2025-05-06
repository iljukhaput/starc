#include "writing_sprint_panel.h"

#include <ui/design_system/design_system.h>
#include <ui/widgets/button/button.h>
#include <ui/widgets/card/card_popup.h>
#include <ui/widgets/check_box/check_box.h>
#include <ui/widgets/icon_button/icon_button.h>
#include <ui/widgets/label/label.h>
#include <ui/widgets/text_field/text_field.h>
#include <utils/3rd_party/WAF/Animation/Animation.h>
#include <utils/helpers/color_helper.h>
#include <utils/helpers/time_helper.h>

#include <QBoxLayout>
#include <QDesktopServices>
#include <QPainter>
#include <QResizeEvent>
#include <QSettings>
#include <QSoundEffect>
#include <QTimer>
#include <QVariantAnimation>


namespace Ui {

namespace {
const QString kSprintDurationKey = QLatin1String("widgets/writing-sprint-panel/duration");
const QString kPlaySoundKey = QLatin1String("widgets/writing-sprint-panel/play-sound");
} // namespace

class WritingSprintPanel::Implementation
{
public:
    explicit Implementation(WritingSprintPanel* _q);

    /**
     * @brief Запущен ли спринт
     */
    bool isSprintActive() const;

    /**
     * @brief Завершён ли спринт
     */
    bool isSprintFinished() const;

    /**
     * @brief Развернуть/свернуть панель
     */
    void expandPanel(bool _force = false);
    void collapsePanel();
    bool isExpanded() const;

    /**
     * @brief Обновить состояние кнопки в зависимости от текущего состояния спринта
     */
    void updateSprintAction();

    /**
     * @brief Показать параметры попапа учитывая область родителя
     */
    void showSprintOptions();


    /**
     * @brief Владелец данных
     */
    WritingSprintPanel* q = nullptr;

    /**
     * @brief Текст на полосе прогресса, когда на неё навели курсор
     */
    QString status;

    /**
     * @brief Анимация заполнения прошедшего времени спринта
     */
    QVariantAnimation sprintDuration;

    /**
     * @brief Анимация размера полосы прогресса спринта
     */
    QVariantAnimation heightAnimation;

    /**
     * @brief Параметры спринта
     */
    CardPopup* sprintOptions = nullptr;
    QVBoxLayout* sprintOptionsLayout = nullptr;
    TextField* sprintTime = nullptr;
    Button* sprintAction = nullptr;
    CheckBox* playSound = nullptr;
    /**
     * @brief Поздравительная часть
     */
    QVBoxLayout* sprintResultsLayout = nullptr;
    IconButton* shareResults = nullptr;
    H6Label* sprintResultTitle = nullptr;
    H4Label* sprintResultWords = nullptr;
    Subtitle2Label* sprintResultSubtitle = nullptr;
    Button* restartSprint = nullptr;
};

WritingSprintPanel::Implementation::Implementation(WritingSprintPanel* _q)
    : q(_q)
    , sprintOptions(new CardPopup(_q))
    , sprintOptionsLayout(new QVBoxLayout)
    , sprintTime(new TextField(sprintOptions))
    , sprintAction(new Button(sprintOptions))
    , playSound(new CheckBox(sprintOptions))
    , sprintResultsLayout(new QVBoxLayout)
    , shareResults(new IconButton(sprintOptions))
    , sprintResultTitle(new H6Label(sprintOptions))
    , sprintResultWords(new H4Label(sprintOptions))
    , sprintResultSubtitle(new Subtitle2Label(sprintOptions))
    , restartSprint(new Button(sprintOptions))
{
    sprintDuration.setEasingCurve(QEasingCurve::Linear);
    sprintDuration.setStartValue(0.0);
    sprintDuration.setEndValue(1.0);

    heightAnimation.setEasingCurve(QEasingCurve::OutQuad);
    heightAnimation.setDuration(160);

    QSettings settings;
    sprintTime->setText(settings.value(kSprintDurationKey, "25").toString());
    playSound->setChecked(settings.value(kPlaySoundKey, false).toBool());

    sprintAction->setIcon(u8"\U000F040A");
    sprintAction->setFlat(true);

    shareResults->setIcon(u8"\U000F0544");
    restartSprint->setIcon(u8"\U000F0450");
    restartSprint->setFlat(true);

    sprintOptionsLayout->setContentsMargins({});
    sprintOptionsLayout->setSpacing(0);
    {
        auto layout = new QHBoxLayout;
        layout->setSpacing(0);
        layout->setContentsMargins({});
        layout->addWidget(sprintTime, 1);
        layout->addWidget(sprintAction);
        sprintOptionsLayout->addLayout(layout);
    }
    sprintOptionsLayout->addWidget(playSound);
    //
    sprintResultsLayout->setContentsMargins({});
    sprintResultsLayout->setSpacing(0);
    sprintResultsLayout->addLayout(sprintOptionsLayout);
    sprintResultsLayout->addWidget(shareResults, 0, Qt::AlignRight);
    sprintResultsLayout->addWidget(sprintResultTitle, 0, Qt::AlignHCenter);
    sprintResultsLayout->addWidget(sprintResultWords, 0, Qt::AlignHCenter);
    sprintResultsLayout->addWidget(sprintResultSubtitle, 0, Qt::AlignHCenter);
    sprintResultsLayout->addWidget(restartSprint, 0, Qt::AlignHCenter);
    sprintOptions->setContentLayout(sprintResultsLayout);
}

bool WritingSprintPanel::Implementation::isSprintActive() const
{
    return sprintDuration.state() == QVariantAnimation::Running;
}

bool WritingSprintPanel::Implementation::isSprintFinished() const
{
    return sprintDuration.state() == QVariantAnimation::Stopped
        && sprintDuration.currentTime() == sprintDuration.duration();
}

void WritingSprintPanel::Implementation::expandPanel(bool _force)
{
    if (!_force) {
        if (heightAnimation.direction() == QVariantAnimation::Forward) {
            return;
        }
    }

    heightAnimation.setDirection(QVariantAnimation::Forward);
    heightAnimation.start();
}

void WritingSprintPanel::Implementation::collapsePanel()
{
    if (heightAnimation.direction() == QVariantAnimation::Backward) {
        return;
    }

    heightAnimation.setDirection(QVariantAnimation::Backward);
    heightAnimation.start();
}

bool WritingSprintPanel::Implementation::isExpanded() const
{
    return q->height() > heightAnimation.startValue().toDouble();
}

void WritingSprintPanel::Implementation::updateSprintAction()
{
    if (isSprintActive()) {
        sprintAction->setIcon(u8"\U000F04DB");
        sprintAction->setBackgroundColor(DesignSystem::color().error());
        sprintAction->setTextColor(DesignSystem::color().onError());
    } else {
        sprintAction->setIcon(u8"\U000F040A");
        sprintAction->setBackgroundColor(DesignSystem::color().accent());
        sprintAction->setTextColor(DesignSystem::color().onAccent());
    }
}

void WritingSprintPanel::Implementation::showSprintOptions()
{
    if (!sprintTime->hasFocus()) {
        sprintTime->setFocus();
    }

    const auto popupSize = sprintOptions->sizeHint();
    sprintOptions->showPopup(q->mapToGlobal(q->rect().center()) - QPoint(popupSize.width() / 2, 0),
                             q->height());
}


// ****


WritingSprintPanel::WritingSprintPanel(QWidget* _parent)
    : Widget(_parent)
    , d(new Implementation(this))
{
    setAttribute(Qt::WA_Hover);
    setCursor(Qt::PointingHandCursor);

    _parent->installEventFilter(this);


    connect(&d->sprintDuration, &QVariantAnimation::valueChanged, this, [this] {
        d->status = QString("%1: %2").arg(
            tr("Until the end of the sprint left"),
            TimeHelper::toString(std::chrono::milliseconds{ d->sprintDuration.duration()
                                                            - d->sprintDuration.currentTime() }));
        update();
    });
    connect(&d->sprintDuration, &QVariantAnimation::finished, this, [this] {
        d->status = tr("Sprint finished!");
        d->updateSprintAction();

        WAF::Animation::circleFillIn(topLevelWidget(), mapToGlobal(rect().topRight()),
                                     ColorHelper::transparent(DesignSystem::color().accent(),
                                                              DesignSystem::disabledTextOpacity()));

        if (d->playSound->isChecked()) {
            auto sound = new QSoundEffect(this);
            sound->setSource(QUrl::fromLocalFile(":/audio/sprint_ends"));
            connect(sound, &QSoundEffect::playingChanged, this, [sound] {
                if (!sound->isPlaying()) {
                    sound->deleteLater();
                }
            });
            sound->play();
        }

        emit sprintFinished();
    });
    connect(&d->heightAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& _value) { resize(width(), _value.toDouble()); });
    connect(d->sprintOptions, &CardPopup::disappeared, this, [this] {
        if (!underMouse()) {
            d->collapsePanel();
        }
    });
    connect(d->sprintTime, &TextField::enterPressed, d->sprintAction, &Button::click);
    connect(d->sprintAction, &Button::clicked, this, [this] {
        //
        // Останавливаем текущий спринт
        //
        if (d->isSprintActive()) {
            d->sprintDuration.pause();
            d->status.clear();
            d->updateSprintAction();

            emit sprintStopped();
        }
        //
        // Запускаем новый спринт
        //
        else {
            d->sprintOptions->hidePopup();

            QSettings settings;
            settings.setValue(kSprintDurationKey, d->sprintTime->text().toInt());
            settings.setValue(kPlaySoundKey, d->playSound->isChecked());

            //
            // После того, как скроются параметры спринта
            //
            QTimer::singleShot(420, this, [this] {
                d->sprintDuration.setDuration(d->sprintTime->text().toInt() * 60 * 1000);
                d->sprintDuration.stop();
                d->sprintDuration.start();
                d->updateSprintAction();
                emit sprintStarted();
            });
        }
    });
    connect(d->shareResults, &IconButton::clicked, this, [this] {
        const auto message = QString("%1 %2").arg(
            tr("Done! I've just written %n word(s)", 0, d->sprintResultWords->text().toInt()),
            tr("in %n minute(s)", 0, d->sprintTime->text().toInt()));
        QDesktopServices::openUrl(QUrl(
            QString("https://twitter.com/intent/tweet?text=%1&hashtags=writesprint&via=starcapp_")
                .arg(message)));
    });
    connect(d->restartSprint, &Button::clicked, this, [this] {
        d->sprintTime->show();
        d->sprintAction->show();
        d->playSound->show();
        d->shareResults->hide();
        d->sprintResultTitle->hide();
        d->sprintResultWords->hide();
        d->sprintResultSubtitle->hide();
        d->restartSprint->hide();
        d->sprintResultsLayout->invalidate();

        d->sprintOptions->resize(d->sprintOptions->sizeHint());
        d->sprintOptions->move(mapToGlobal(rect().center())
                               - QPoint(d->sprintOptions->width() / 2, 0));
    });
}

WritingSprintPanel::~WritingSprintPanel() = default;

void WritingSprintPanel::showPanel()
{
    if (d->isExpanded() || d->isSprintActive()) {
        return;
    }

    d->sprintDuration.setCurrentTime(d->sprintDuration.duration());
    d->status.clear();

    d->sprintTime->show();
    d->sprintAction->show();
    d->playSound->show();
    d->shareResults->hide();
    d->sprintResultTitle->hide();
    d->sprintResultWords->hide();
    d->sprintResultSubtitle->hide();
    d->restartSprint->hide();

    raise();
    resize(width(), 1);
    show();

    d->expandPanel(true);

    QTimer::singleShot(d->heightAnimation.duration(), this, [this] { d->showSprintOptions(); });
}

void WritingSprintPanel::setResult(int _words)
{
    if (d->isExpanded()) {
        d->sprintOptions->hide();
    }

    d->sprintResultWords->setText(QString::number(_words));

    d->sprintTime->hide();
    d->sprintAction->hide();
    d->playSound->hide();
    d->shareResults->show();
    d->sprintResultTitle->show();
    d->sprintResultWords->show();
    d->sprintResultSubtitle->show();
    d->restartSprint->show();

    d->sprintResultsLayout->invalidate();
}

bool WritingSprintPanel::eventFilter(QObject* _watched, QEvent* _event)
{
    if (_watched == parent()) {
        if (_event->type() == QEvent::Resize) {
            const auto resizeEvent = static_cast<QResizeEvent*>(_event);
            resize(resizeEvent->size().width(), height());
        } else if (_event->type() == QEvent::ChildAdded) {
            raise();
        }
    }

    return Widget::eventFilter(_watched, _event);
}

void WritingSprintPanel::paintEvent(QPaintEvent* _event)
{
    Q_UNUSED(_event)

    QPainter painter(this);

    //
    // Заливаем фон
    //
    painter.fillRect(rect(), Qt::transparent);
    const auto backgroundRect = rect().adjusted(0, 0, 0, -DesignSystem::layout().px(6));
    painter.fillRect(backgroundRect,
                     ColorHelper::transparent(DesignSystem::color().accent(),
                                              d->isSprintFinished()
                                                  ? DesignSystem::inactiveTextOpacity()
                                                  : DesignSystem::hoverBackgroundOpacity()));

    //
    // Заполненная полоса прогресса
    //
    auto completedRect = backgroundRect;
    completedRect.setWidth(completedRect.width() * d->sprintDuration.currentValue().toDouble());
    painter.fillRect(completedRect,
                     ColorHelper::transparent(DesignSystem::color().accent(),
                                              DesignSystem::inactiveTextOpacity()));

    //
    // Если панель в состоянии открытой
    //
    {
        painter.setOpacity(d->isExpanded() ? (height() / d->heightAnimation.endValue().toDouble())
                                           : 0.0);

        //
        // Рисуем текстовку и кнопку закрытия
        //
        const QRectF closeIconRect(0, 0, DesignSystem::layout().px48(), backgroundRect.height());
        //
        // ... над полупрозрачной областью
        //
        painter.setClipRect(backgroundRect.adjusted(completedRect.width(), 0, 0, 0));
        painter.setPen(DesignSystem::color().onSurface());
        painter.setFont(DesignSystem::font().iconsSmall());
        painter.drawText(closeIconRect, Qt::AlignCenter, u8"\U000F0156");
        //
        painter.setFont(DesignSystem::font().subtitle2());
        painter.drawText(backgroundRect, Qt::AlignCenter, d->status);
        //
        // ... над залитой областью
        //
        painter.setClipRect(completedRect);
        painter.setPen(DesignSystem::color().onAccent());
        painter.setFont(DesignSystem::font().iconsSmall());
        painter.drawText(closeIconRect, Qt::AlignCenter, u8"\U000F0156");
        //
        painter.setFont(DesignSystem::font().subtitle2());
        painter.drawText(backgroundRect, Qt::AlignCenter, d->status);
    }
}

void WritingSprintPanel::enterEvent(QEvent* _event)
{
    Widget::enterEvent(_event);

    if (height() < d->heightAnimation.endValue().toDouble()) {
        d->expandPanel();
    }
}

void WritingSprintPanel::leaveEvent(QEvent* _event)
{
    Widget::leaveEvent(_event);

    if (height() > d->heightAnimation.startValue().toDouble() && !d->sprintOptions->isVisible()) {
        d->collapsePanel();
    }
}

void WritingSprintPanel::mousePressEvent(QMouseEvent* _event)
{
    Widget::mousePressEvent(_event);

    //
    // Пользователь хочет закрыть панель
    //
    if (0 <= _event->pos().x() && _event->pos().x() <= DesignSystem::layout().px48()) {
        d->sprintDuration.stop();
        d->sprintDuration.setCurrentTime(0);
        d->status.clear();
        d->updateSprintAction();
        d->collapsePanel();
        hide();
    }
    //
    // Показать панель настроек спринта
    //
    else {
        d->showSprintOptions();
    }
}

void WritingSprintPanel::updateTranslations()
{
    d->sprintTime->setLabel(tr("Sprint duration"));
    d->sprintTime->setSuffix(tr("min"));
    d->playSound->setText(tr("Play sound at end"));
    d->sprintResultTitle->setText(tr("Great job"));
    d->sprintResultSubtitle->setText(tr("words written"));
    d->restartSprint->setText(tr("Restart sprint"));
}

void WritingSprintPanel::designSystemChangeEvent(DesignSystemChangeEvent* _event)
{
    Widget::designSystemChangeEvent(_event);

    d->heightAnimation.setStartValue(DesignSystem::layout().px12());
    d->heightAnimation.setEndValue(DesignSystem::layout().px(30));

    d->sprintOptions->setBackgroundColor(DesignSystem::color().background());
    d->sprintTime->setBackgroundColor(DesignSystem::color().onBackground());
    d->sprintTime->setTextColor(DesignSystem::color().onBackground());
    d->sprintTime->setCustomMargins(
        { DesignSystem::layout().px12(), 0, DesignSystem::layout().px16(), 0 });
    d->sprintAction->setBackgroundColor(DesignSystem::color().accent());
    d->sprintAction->setTextColor(DesignSystem::color().background());
    d->playSound->setBackgroundColor(DesignSystem::color().background());
    d->playSound->setTextColor(DesignSystem::color().onBackground());
    d->playSound->setContentsMargins(DesignSystem::layout().px12(), DesignSystem::layout().px12(),
                                     0, DesignSystem::layout().px12());

    d->sprintResultsLayout->setContentsMargins(
        DesignSystem::layout().px4(), DesignSystem::layout().px16(), DesignSystem::layout().px16(),
        DesignSystem::layout().px4());

    d->shareResults->setBackgroundColor(DesignSystem::color().background());
    d->shareResults->setTextColor(DesignSystem::color().onBackground());
    d->sprintResultTitle->setBackgroundColor(DesignSystem::color().background());
    d->sprintResultTitle->setTextColor(DesignSystem::color().onBackground());
    d->sprintResultTitle->setContentsMargins(0, DesignSystem::layout().px12(), 0, 0);
    d->sprintResultWords->setBackgroundColor(DesignSystem::color().background());
    d->sprintResultWords->setTextColor(DesignSystem::color().accent());
    d->sprintResultSubtitle->setBackgroundColor(DesignSystem::color().background());
    d->sprintResultSubtitle->setTextColor(DesignSystem::color().onBackground());
    d->sprintResultSubtitle->setContentsMargins(0, 0, 0, DesignSystem::layout().px48());
    d->restartSprint->setBackgroundColor(DesignSystem::color().accent());
    d->restartSprint->setTextColor(DesignSystem::color().onAccent());
    d->restartSprint->setContentsMargins(DesignSystem::layout().px12(), 0, 0,
                                         DesignSystem::layout().px12());
}

} // namespace Ui
