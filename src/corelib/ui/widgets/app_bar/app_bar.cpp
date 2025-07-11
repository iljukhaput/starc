#include "app_bar.h"

#include <ui/design_system/design_system.h>
#include <ui/widgets/context_menu/context_menu.h>
#include <utils/helpers/color_helper.h>
#include <utils/helpers/icon_helper.h>

#include <QAction>
#include <QPaintEvent>
#include <QPainter>
#include <QToolTip>
#include <QVariantAnimation>

namespace {
const char* kBadgeVisibleKey = "is-badge-visible";
}


class AppBar::Implementation
{
public:
    explicit Implementation(AppBar* _q);

    /**
     * @brief Анимировать клик
     */
    void animateClick();

    /**
     * @brief Получить пункт меню по координате
     */
    QAction* mapToAction(const QPoint& _coordinate) const;

    /**
     * @brief Получить список опций заданного типа
     */
    QVector<QAction*>& options(AppBarOptionsLevel _level);

    /**
     * @brief Есть ли опции
     */
    bool hasOptions() const;


    /**
     * @brief Владелец данных
     */
    AppBar* q = nullptr;

    /**
     * @brief Иконка опций тулбара
     */
    QAction* optionsAction = nullptr;
    //
    // и сами опции
    //
    QVector<QAction*> appOptions;
    QVector<QAction*> navigatorOptions;
    QVector<QAction*> viewOptions;

    /**
     * @brief Иконка на которой кликнули последней
     */
    QAction* lastPressedAction = nullptr;

    /**
     * @brief  Декорации иконки при клике
     */
    QVariantAnimation decorationRadiusAnimation;
    QVariantAnimation decorationOpacityAnimation;
};

AppBar::Implementation::Implementation(AppBar* _q)
    : q(_q)
    , optionsAction(new QAction(_q))
{
    optionsAction->setText(u8"\U000F01D9");

    decorationRadiusAnimation.setEasingCurve(QEasingCurve::OutQuad);
    decorationRadiusAnimation.setDuration(160);

    decorationOpacityAnimation.setEasingCurve(QEasingCurve::InQuad);
    decorationOpacityAnimation.setStartValue(0.5);
    decorationOpacityAnimation.setEndValue(0.0);
    decorationOpacityAnimation.setDuration(160);
}

void AppBar::Implementation::animateClick()
{
    decorationOpacityAnimation.setCurrentTime(0);
    decorationRadiusAnimation.start();
    decorationOpacityAnimation.start();
}

QAction* AppBar::Implementation::mapToAction(const QPoint& _coordinate) const
{
    //
    // Берём увеличенный регион для удобства клика в области кнопки
    //
    const qreal actionWidth
        = Ui::DesignSystem::appBar().iconSize().width() + Ui::DesignSystem::appBar().iconsSpacing();

    //
    // Кнопка опций приоритетна
    //
    qreal actionLeft = 0;
    if (hasOptions()) {
        actionLeft = q->isLeftToRight()
            ? (q->width() - Ui::DesignSystem::appBar().iconSize().width()
               - Ui::DesignSystem::appBar().margins().right())
            : Ui::DesignSystem::appBar().margins().left();
        const qreal actionRight = actionLeft + actionWidth;
        if (actionLeft < _coordinate.x() && _coordinate.x() < actionRight) {
            return optionsAction;
        }
    }

    //
    // Ищем среди остальных действий
    //
    actionLeft = q->isLeftToRight() ? Ui::DesignSystem::appBar().margins().left()
            - (Ui::DesignSystem::appBar().iconsSpacing() / 2.0)
                                    : q->width() - (Ui::DesignSystem::appBar().iconsSpacing() / 2.0)
            - Ui::DesignSystem::appBar().iconSize().width()
            - Ui::DesignSystem::appBar().margins().left();
    for (QAction* action : q->actions()) {
        if (!action->isVisible()) {
            continue;
        }

        const qreal actionRight = actionLeft + actionWidth;

        if (actionLeft < _coordinate.x() && _coordinate.x() < actionRight) {
            return action;
        }

        actionLeft = q->isLeftToRight() ? actionRight : actionLeft - actionWidth;
    }

    return nullptr;
}

QVector<QAction*>& AppBar::Implementation::options(AppBarOptionsLevel _level)
{
    switch (_level) {
    case AppBarOptionsLevel::Modules: {
        return appOptions;
    }

    case AppBarOptionsLevel::Navigation: {
        return navigatorOptions;
    }

    case AppBarOptionsLevel::View: {
        return viewOptions;
    }
    }
}

bool AppBar::Implementation::hasOptions() const
{
    return !appOptions.isEmpty() || !navigatorOptions.isEmpty() || !viewOptions.isEmpty();
}


// ****


AppBar::AppBar(QWidget* _parent)
    : Widget(_parent)
    , d(new Implementation(this))
{
    connect(d->optionsAction, &QAction::triggered, this, [this] {
        if (!d->hasOptions()) {
            return;
        }

        //
        // Настроим список опций для меню
        //
        auto options = d->appOptions;
        if (!d->navigatorOptions.isEmpty()) {
            d->navigatorOptions.first()->setSeparator(!options.isEmpty());
            options.append(d->navigatorOptions);
        }
        if (!d->viewOptions.isEmpty()) {
            d->viewOptions.first()->setSeparator(!options.isEmpty());
            options.append(d->viewOptions);
        }

        //
        // Настроим меню опций
        //
        auto menu = new ContextMenu(this);
        menu->setBackgroundColor(Ui::DesignSystem::color().background());
        menu->setTextColor(Ui::DesignSystem::color().onBackground());
        menu->setActions(options);
        connect(menu, &ContextMenu::disappeared, menu, &ContextMenu::deleteLater);

        //
        // Покажем меню
        //
        menu->showContextMenu(QCursor::pos());
    });
    connect(&d->decorationRadiusAnimation, &QVariantAnimation::valueChanged, this,
            [this] { update(); });
    connect(&d->decorationOpacityAnimation, &QVariantAnimation::valueChanged, this,
            [this] { update(); });
}

void AppBar::setBadgeVisible(QAction* _action, bool _visible)
{
    if (!actions().contains(_action)) {
        return;
    }

    _action->setProperty(kBadgeVisibleKey, _visible);
    update();
}

QVector<QAction*> AppBar::options(AppBarOptionsLevel _level) const
{
    return d->options(_level);
}

void AppBar::setOptions(const QVector<QAction*>& _options, AppBarOptionsLevel _level)
{
    if (d->options(_level) == _options) {
        return;
    }

    d->options(_level) = _options;
    update();
}

void AppBar::clearOptions(AppBarOptionsLevel _level)
{
    if (d->options(_level).isEmpty()) {
        return;
    }

    d->options(_level).clear();
    update();
}

void AppBar::clearNavigatorOptions()
{
    clearOptions(AppBarOptionsLevel::Navigation);
}

QSize AppBar::minimumSizeHint() const
{
    const int actionsSize = actions().size() + (d->hasOptions() ? 1 : 0);
    const qreal width = Ui::DesignSystem::appBar().margins().left()
        + Ui::DesignSystem::appBar().iconSize().width() * actionsSize
        + Ui::DesignSystem::appBar().iconsSpacing() * (actionsSize - 1)
        + Ui::DesignSystem::appBar().margins().right();
    const qreal height = Ui::DesignSystem::appBar().margins().top()
        + Ui::DesignSystem::appBar().iconSize().height()
        + Ui::DesignSystem::appBar().margins().bottom();
    return QSizeF(width, height).toSize();
}

AppBar::~AppBar() = default;

bool AppBar::event(QEvent* _event)
{
    if (_event->type() == QEvent::ToolTip) {
        QHelpEvent* event = static_cast<QHelpEvent*>(_event);
        QAction* action = d->mapToAction(event->pos());
        if (action != nullptr && !action->toolTip().isEmpty()) {
            QToolTip::showText(event->globalPos(), action->toolTip());
        } else {
            QToolTip::hideText();
        }
        return true;
    }

    return Widget::event(_event);
}

void AppBar::paintEvent(QPaintEvent* _event)
{
    QPainter painter(this);

    //
    // Заливаем фон
    //
    painter.fillRect(_event->rect(), backgroundColor());

    //
    // Рисуем иконки
    //
    painter.setFont(Ui::DesignSystem::font().iconsMid());
    qreal actionX = isLeftToRight() ? Ui::DesignSystem::appBar().margins().left()
                                    : width() - Ui::DesignSystem::appBar().iconSize().width()
            - Ui::DesignSystem::appBar().margins().right();
    const qreal actionY = Ui::DesignSystem::appBar().margins().top();
    const QSizeF actionSize = Ui::DesignSystem::appBar().iconSize();
    const QColor iconInactiveColor = ColorHelper::colorBetween(textColor(), backgroundColor());
    auto drawIcon = [this, &painter, &actionX, actionY, actionSize,
                     iconInactiveColor](const QAction* _action) {
        //
        // ... сама иконка
        //
        const QRectF actionRect(QPointF(actionX, actionY), actionSize);
        painter.setPen((!_action->isCheckable() || _action->isChecked()) ? textColor()
                                                                         : iconInactiveColor);
        painter.drawText(actionRect, Qt::AlignCenter, IconHelper::directedIcon(_action->text()));

        //
        // ... красная точка
        //
        if (_action->property(kBadgeVisibleKey).toBool()) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(Ui::DesignSystem::color().error());
            painter.drawEllipse(actionRect.right() - Ui::DesignSystem::layout().px4(),
                                actionRect.top(), Ui::DesignSystem::layout().px8(),
                                Ui::DesignSystem::layout().px8());
        }

        //
        // ... декорация
        //
        if (_action == d->lastPressedAction
            && (d->decorationRadiusAnimation.state() == QVariantAnimation::Running
                || d->decorationOpacityAnimation.state() == QVariantAnimation::Running)) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(Ui::DesignSystem::color().accent());
            painter.setOpacity(d->decorationOpacityAnimation.currentValue().toReal());
            painter.drawEllipse(actionRect.center(),
                                d->decorationRadiusAnimation.currentValue().toReal(),
                                d->decorationRadiusAnimation.currentValue().toReal());
            painter.setOpacity(1.0);
        }
    };
    const auto lastActionX = isLeftToRight()
        ? (width() - Ui::DesignSystem::appBar().iconSize().width()
           - Ui::DesignSystem::appBar().margins().right())
        : Ui::DesignSystem::appBar().margins().left();
    for (const QAction* action : actions()) {
        if (!action->isVisible()) {
            continue;
        }

        drawIcon(action);

        actionX += (isLeftToRight() ? 1 : -1)
            * (actionSize.width() + Ui::DesignSystem::appBar().iconsSpacing());

        //
        // ... если следующая иконка не влезает, то прерываем отрисовку
        //
        if (isLeftToRight() ? actionX + actionSize.width() > lastActionX
                            : actionX < lastActionX + actionSize.width()) {
            break;
        }
    }

    //
    // Иконка опций панели
    //
    if (d->hasOptions()) {
        actionX = isLeftToRight() ? (width() - Ui::DesignSystem::appBar().iconSize().width()
                                     - Ui::DesignSystem::appBar().margins().right())
                                  : Ui::DesignSystem::appBar().margins().left();
        drawIcon(d->optionsAction);
    }
}

void AppBar::mousePressEvent(QMouseEvent* _event)
{
    QAction* pressedAction = d->mapToAction(_event->pos());
    if (pressedAction == nullptr) {
        return;
    }

    d->lastPressedAction = pressedAction;
    if (d->lastPressedAction->text().isEmpty()) {
        return;
    }

    d->animateClick();
}

void AppBar::mouseReleaseEvent(QMouseEvent* _event)
{
    QAction* pressedAction = d->mapToAction(_event->pos());
    if (pressedAction == nullptr) {
        return;
    }

    if (!pressedAction->isCheckable()) {
        pressedAction->trigger();
        return;
    }

    if (pressedAction->isChecked()) {
        return;
    }

    for (auto action : actions()) {
        if (action->isCheckable() && action != pressedAction) {
            action->setChecked(false);
        }
    }
    pressedAction->setChecked(true);
    update();
}

void AppBar::updateTranslations()
{
    d->optionsAction->setToolTip(tr("Show module options"));
}

void AppBar::designSystemChangeEvent(DesignSystemChangeEvent* _event)
{
    Q_UNUSED(_event)

    d->decorationRadiusAnimation.setStartValue(Ui::DesignSystem::appBar().iconSize().height()
                                               / 2.0);
    d->decorationRadiusAnimation.setEndValue(Ui::DesignSystem::appBar().heightRegular() / 2.5);

    updateGeometry();
    update();
}
