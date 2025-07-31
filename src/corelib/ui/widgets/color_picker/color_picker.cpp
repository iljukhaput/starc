#include "color_picker.h"

#include "color_2d_slider.h"
#include "color_hue_slider.h"
#include "color_palette.h"

#include <ui/design_system/design_system.h>
#include <ui/widgets/button/button.h>
#include <ui/widgets/text_field/text_field.h>

#include <QApplication>
#include <QKeyEvent>
#include <QScreen>
#include <QVBoxLayout>
#include <QWindow>


class ColorPicker::Implementation
{
public:
    explicit Implementation(QWidget* _parent);

    QColor grabScreenColor(const QPoint& _position);

    ColorPallete* colorPallete = nullptr;
    Widget* customColorPanel = nullptr;
    TextField* colorCode = nullptr;
    color_widgets::Color2DSlider* colorSlider = nullptr;
    ColorHueSlider* colorHueSlider = nullptr;
    Button* cancelButton = nullptr;
    Button* addButton = nullptr;
    QHBoxLayout* buttonsLayout = nullptr;

    QVector<Widget*> screenOverlays;

    bool isScreenColorPicking = false;
};

ColorPicker::Implementation::Implementation(QWidget* _parent)
    : colorPallete(new ColorPallete(_parent))
    , customColorPanel(new Widget(_parent))
    , colorCode(new TextField(_parent))
    , colorSlider(new color_widgets::Color2DSlider(_parent))
    , colorHueSlider(new ColorHueSlider(_parent))
    , cancelButton(new Button(_parent))
    , addButton(new Button(_parent))
    , buttonsLayout(new QHBoxLayout)
{
    customColorPanel->hide();
    colorCode->setTrailingIcon(u8"\U000F020A");
    cancelButton->setFocusPolicy(Qt::NoFocus);
    addButton->setFocusPolicy(Qt::NoFocus);

    buttonsLayout->setContentsMargins({});
    buttonsLayout->setSpacing(0);
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(cancelButton);
    buttonsLayout->addWidget(addButton);

    for (auto screen : QApplication::screens()) {
        auto overlay = new Widget;
        overlay->setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
        overlay->setAttribute(Qt::WA_TranslucentBackground);
        overlay->setCursor(Qt::CrossCursor);
        overlay->setMouseTracking(true);

        QColor backgroundColor = Qt::white;
        //
        // Если сделать полностью прозрачный фон, то на Windows такое окно автоматом закрывается
        //
        backgroundColor.setAlpha(
#ifdef Q_OS_WIN
            1
#else
            0
#endif
        );
        overlay->setBackgroundColor(backgroundColor);

        overlay->move(screen->geometry().topLeft());
        overlay->hide();
        screenOverlays.append(overlay);
    }
}

QColor ColorPicker::Implementation::grabScreenColor(const QPoint& _position)
{
    const auto screen = QGuiApplication::screenAt(_position);
    if (screen == nullptr) {
        return {};
    }

    //
    // Используем 0 вместо хэндла окна, чтобы захватить весь экран
    //
    const auto sceenPixmap = screen->grabWindow(0, _position.x(), _position.y(), 1, 1);
    return sceenPixmap.toImage().pixel(0, 0);
}


// ****


ColorPicker::ColorPicker(QWidget* _parent)
    : StackWidget(_parent)
    , d(new Implementation(this))
{
    for (auto screenOverlay : std::as_const(d->screenOverlays)) {
        screenOverlay->installEventFilter(this);
    }

    QVBoxLayout* customColorPanelLayout = new QVBoxLayout(d->customColorPanel);
    customColorPanelLayout->setContentsMargins({});
    customColorPanelLayout->setSpacing(0);
    customColorPanelLayout->addWidget(d->colorCode);
    customColorPanelLayout->addWidget(d->colorSlider, 1);
    customColorPanelLayout->addWidget(d->colorHueSlider);
    customColorPanelLayout->addLayout(d->buttonsLayout);

    setCurrentWidget(d->colorPallete);
    addWidget(d->customColorPanel);

    d->colorHueSlider->setHue(d->colorSlider->hue());

    connect(d->colorPallete, &ColorPallete::selectedColorChanged, this,
            &ColorPicker::selectedColorChanged);
    connect(d->colorPallete, &ColorPallete::addCustomColorPressed, this, [this] {
        const QColor colorToSelect = d->colorPallete->selectedColor().isValid()
            ? d->colorPallete->selectedColor()
            : kDefaultWidgetColor;
        d->colorCode->setText(colorToSelect.name());
        setCurrentWidget(d->customColorPanel);
    });
    connect(d->colorCode, &TextField::textChanged, this, [this] {
        const auto colorName = d->colorCode->text();
        if (colorName.length() != 7) {
            //
            // В кейсе когда из буфера обмена вставили хекс без решётки, докрутим его немного
            //
            if (colorName.length() == 6 && !colorName.startsWith('#')) {
                d->colorCode->setText("#" + colorName);
            }
            return;
        }

        const QColor color(colorName);
        if (!color.isValid()) {
            return;
        }

        QSignalBlocker hueBlocker(d->colorHueSlider);
        d->colorHueSlider->setHue(color.hueF());

        QSignalBlocker colorBlocker(d->colorSlider);
        d->colorSlider->setColor(color);
    });
    connect(d->colorCode, &TextField::trailingIconPressed, this, [this] {
        d->isScreenColorPicking = true;
        for (auto screenOverlay : std::as_const(d->screenOverlays)) {
            screenOverlay->raise();
            screenOverlay->showMaximized();
        }
    });
    connect(d->colorSlider, &color_widgets::Color2DSlider::colorChanged, this,
            [this](const QColor& _color) {
                QSignalBlocker blocker(d->colorCode);
                d->colorCode->setText(_color.name());
            });
    connect(d->colorHueSlider, &ColorHueSlider::hueChanged, this,
            [this](qreal _hue) { d->colorSlider->setHue(_hue); });
    connect(d->cancelButton, &Button::clicked, this, [this] { setCurrentWidget(d->colorPallete); });
    connect(d->addButton, &Button::clicked, this, [this] {
        const auto color = d->colorSlider->color();
        d->colorPallete->setSelectedColor(color);
        setCurrentWidget(d->colorPallete);

        emit selectedColorChanged(color);
    });
}

ColorPicker::~ColorPicker()
{
    qDeleteAll(d->screenOverlays);
}

void ColorPicker::setCustomPalette(const QVector<QColor>& _palette)
{
    d->colorPallete->setCustomPalette(_palette);
    updateGeometry();
}

void ColorPicker::setColorCanBeDeselected(bool _can)
{
    d->colorPallete->setColorCanBeDeselected(_can);
}

QColor ColorPicker::selectedColor() const
{
    return d->colorPallete->selectedColor();
}

void ColorPicker::setSelectedColor(const QColor& _color)
{
    setCurrentWidget(d->colorPallete);
    d->colorPallete->setSelectedColor(_color);
}

QSize ColorPicker::sizeHint() const
{
    auto sizeHint = d->colorPallete->sizeHint();
    if (d->colorPallete->hasCustomPalette()) {
        return sizeHint;
    }

    if (const auto panelSizeHint = d->customColorPanel->sizeHint();
        sizeHint.width() < panelSizeHint.width() || sizeHint.height() < panelSizeHint.height()) {
        sizeHint.setWidth(std::max(sizeHint.width(), panelSizeHint.width()));
        sizeHint.setHeight(std::max(sizeHint.height(), panelSizeHint.height()));
    }
    return sizeHint;
}

bool ColorPicker::eventFilter(QObject* _watched, QEvent* _event)
{
    if (auto overlay = qobject_cast<Widget*>(_watched);
        overlay != nullptr && overlay->isVisible() && d->screenOverlays.contains(overlay)) {
        switch (_event->type()) {
        case QEvent::MouseMove: {
            if (d->isScreenColorPicking) {
                d->colorCode->setText(d->grabScreenColor(QCursor::pos()).name());
            }
            break;
        }

        case QEvent::KeyPress: {
            auto keyEvent = static_cast<QKeyEvent*>(_event);
            if (keyEvent->key() != Qt::Key_Escape) {
                break;
            }

            Q_FALLTHROUGH();
        }

        case QEvent::MouseButtonRelease: {
            d->isScreenColorPicking = false;
            for (auto screenOverlay : std::as_const(d->screenOverlays)) {
                screenOverlay->hide();
            }
            emit selectedColorChanged(d->colorSlider->color());
            break;
        }

        case QEvent::Enter: {
            QApplication::restoreOverrideCursor();
            break;
        }

        default: {
            break;
        }
        }
    }

    return StackWidget::eventFilter(_watched, _event);
}

void ColorPicker::processBackgroundColorChange()
{
    d->colorPallete->setBackgroundColor(backgroundColor());
    d->customColorPanel->setBackgroundColor(backgroundColor());
    d->colorSlider->setBackgroundColor(backgroundColor());
    d->colorHueSlider->setBackgroundColor(backgroundColor());
}

void ColorPicker::processTextColorChange()
{
    d->colorPallete->setTextColor(textColor());
    d->colorCode->setBackgroundColor(textColor());
    d->colorCode->setTextColor(textColor());
    d->colorSlider->setTextColor(textColor());
    d->colorHueSlider->setTextColor(textColor());
}

void ColorPicker::updateTranslations()
{
    d->colorCode->setLabel(tr("Color hex code"));
    d->cancelButton->setText(tr("Cancel"));
    d->addButton->setText(tr("Add"));
}

void ColorPicker::designSystemChangeEvent(DesignSystemChangeEvent* _event)
{
    Widget::designSystemChangeEvent(_event);

    d->colorCode->setCustomMargins(
        { Ui::DesignSystem::layout().px8(), Ui::DesignSystem::layout().px8(),
          Ui::DesignSystem::layout().px8(), Ui::DesignSystem::layout().px8() });

    d->colorHueSlider->setFixedHeight(Ui::DesignSystem::layout().px24());

    d->addButton->setBackgroundColor(Ui::DesignSystem::color().accent());
    d->addButton->setTextColor(Ui::DesignSystem::color().accent());
    d->cancelButton->setBackgroundColor(Ui::DesignSystem::color().accent());
    d->cancelButton->setTextColor(Ui::DesignSystem::color().accent());
    //
    d->buttonsLayout->setContentsMargins(
        Ui::DesignSystem::layout().px8(), Ui::DesignSystem::layout().px2(),
        Ui::DesignSystem::layout().px8(), Ui::DesignSystem::layout().px8());
}
