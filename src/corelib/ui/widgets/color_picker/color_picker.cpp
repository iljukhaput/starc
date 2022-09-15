#include "color_picker.h"

#include "color_2d_slider.h"
#include "color_hue_slider.h"
#include "color_palette.h"

#include <ui/design_system/design_system.h>
#include <ui/widgets/button/button.h>
#include <ui/widgets/text_field/text_field.h>

#include <QApplication>
#include <QDesktopWidget>
#include <QKeyEvent>
#include <QScreen>
#include <QVBoxLayout>


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

    bool screenColorPicking = false;
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
}

QColor ColorPicker::Implementation::grabScreenColor(const QPoint& _position)
{
    const auto desk = QApplication::desktop();
    const auto screen = QGuiApplication::primaryScreen();
    const auto sceenPixmap = screen->grabWindow(desk->winId(), _position.x(), _position.y(), 1, 1);
    return sceenPixmap.toImage().pixel(0, 0);
}


// ****


ColorPicker::ColorPicker(QWidget* _parent)
    : StackWidget(_parent)
    , d(new Implementation(this))
{

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
        d->screenColorPicking = true;
        setCursor(Qt::CrossCursor);
        grabMouse(Qt::CrossCursor);
        grabKeyboard();
        setMouseTracking(true);
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

ColorPicker::~ColorPicker() = default;

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
    d->colorPallete->setSelectedColor(_color);
}

void ColorPicker::mouseMoveEvent(QMouseEvent* _event)
{
    if (d->screenColorPicking) {
        d->colorCode->setText(d->grabScreenColor(_event->globalPos()).name());
        return;
    }

    StackWidget::mouseMoveEvent(_event);
}

void ColorPicker::mouseReleaseEvent(QMouseEvent* _event)
{
    if (d->screenColorPicking) {
        d->screenColorPicking = false;
        releaseMouse();
        releaseKeyboard();
        d->colorCode->setText(d->grabScreenColor(_event->globalPos()).name());
        setCursor(Qt::ArrowCursor);
        setMouseTracking(false);
        return;
    }

    StackWidget::mouseReleaseEvent(_event);
}

void ColorPicker::keyPressEvent(QKeyEvent* _event)
{
    if (d->screenColorPicking) {
        if (_event->key() == Qt::Key_Escape) {
            d->screenColorPicking = false;
            releaseMouse();
            releaseKeyboard();
            setCursor(Qt::ArrowCursor);
            setMouseTracking(false);
        }
        _event->accept();
        return;
    }

    StackWidget::keyPressEvent(_event);
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

    d->addButton->setBackgroundColor(Ui::DesignSystem::color().secondary());
    d->addButton->setTextColor(Ui::DesignSystem::color().secondary());
    d->cancelButton->setBackgroundColor(Ui::DesignSystem::color().secondary());
    d->cancelButton->setTextColor(Ui::DesignSystem::color().secondary());
    //
    d->buttonsLayout->setContentsMargins(
        Ui::DesignSystem::layout().px8(), Ui::DesignSystem::layout().px2(),
        Ui::DesignSystem::layout().px8(), Ui::DesignSystem::layout().px8());
}
