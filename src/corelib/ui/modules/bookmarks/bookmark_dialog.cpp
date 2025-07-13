#include "bookmark_dialog.h"

#include <ui/design_system/design_system.h>
#include <ui/widgets/button/button.h>
#include <ui/widgets/color_picker/color_picker_popup.h>
#include <ui/widgets/text_field/text_field.h>
#include <utils/helpers/ui_helper.h>

#include <QBoxLayout>


namespace Ui {

class BookmarkDialog::Implementation
{
public:
    explicit Implementation(QWidget* _parent);


    DialogType dialogType = DialogType::CreateNew;

    TextField* bookmarkName = nullptr;
    ColorPickerPopup* bookmarkColorPopup = nullptr;

    QHBoxLayout* buttonsLayout = nullptr;
    Button* cancelButton = nullptr;
    Button* saveButton = nullptr;
};

BookmarkDialog::Implementation::Implementation(QWidget* _parent)
    : bookmarkName(new TextField(_parent))
    , bookmarkColorPopup(new ColorPickerPopup(_parent))
    , buttonsLayout(new QHBoxLayout)
    , cancelButton(new Button(_parent))
    , saveButton(new Button(_parent))
{
    bookmarkColorPopup->setColorCanBeDeselected(false);
    bookmarkColorPopup->setSelectedColor(Qt::red);
    bookmarkName->setTrailingIcon(u8"\U000F0765");
    bookmarkName->setTrailingIconColor(bookmarkColorPopup->selectedColor());

    buttonsLayout->setContentsMargins({});
    buttonsLayout->setSpacing(0);
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(cancelButton, 0, Qt::AlignVCenter);
    buttonsLayout->addWidget(saveButton, 0, Qt::AlignVCenter);
}


// ****


BookmarkDialog::BookmarkDialog(QWidget* _parent)
    : AbstractDialog(_parent)
    , d(new Implementation(this))
{
    setAcceptButton(d->saveButton);
    setRejectButton(d->cancelButton);

    contentsLayout()->setContentsMargins({});
    contentsLayout()->setSpacing(0);
    int row = 0;
    contentsLayout()->addWidget(d->bookmarkName, row++, 0);
    contentsLayout()->addLayout(d->buttonsLayout, row++, 0);


    connect(d->bookmarkName, &TextField::trailingIconPressed, this, [this] {
        d->bookmarkColorPopup->showPopup(d->bookmarkName, Qt::AlignBottom | Qt::AlignRight);
    });
    connect(d->bookmarkColorPopup, &ColorPickerPopup::selectedColorChanged, this,
            [this](const QColor& _color) { d->bookmarkName->setTrailingIconColor(_color); });
    connect(d->cancelButton, &Button::clicked, this, &BookmarkDialog::hideDialog);
    connect(d->saveButton, &Button::clicked, this, &BookmarkDialog::savePressed);
}

BookmarkDialog::~BookmarkDialog() = default;

void BookmarkDialog::setDialogType(DialogType _type)
{
    if (d->dialogType == _type) {
        return;
    }

    d->dialogType = _type;
    updateTranslations();
}

QString BookmarkDialog::bookmarkName() const
{
    return d->bookmarkName->text();
}

void BookmarkDialog::setBookmarkName(const QString& _text)
{
    d->bookmarkName->setText(_text);
}

QColor BookmarkDialog::bookmarkColor() const
{
    return d->bookmarkColorPopup->selectedColor();
}

void BookmarkDialog::setBookmarkColor(const QColor& _color)
{
    if (!_color.isValid() || d->bookmarkColorPopup->selectedColor() == _color) {
        return;
    }

    d->bookmarkName->setTrailingIconColor(_color);
    d->bookmarkColorPopup->setSelectedColor(_color);
}

QWidget* BookmarkDialog::focusedWidgetAfterShow() const
{
    return d->bookmarkName;
}

QWidget* BookmarkDialog::lastFocusableWidget() const
{
    return d->saveButton;
}

void BookmarkDialog::updateTranslations()
{
    setTitle(d->dialogType == DialogType::CreateNew ? tr("Create new bookmark")
                                                    : tr("Edit bookmark"));
    d->bookmarkName->setLabel(tr("Bookmark text"));
    d->bookmarkName->setTrailingIconToolTip(tr("Select bookmark color"));
    d->cancelButton->setText(tr("Cancel"));
    d->saveButton->setText(d->dialogType == DialogType::CreateNew ? tr("Create") : tr("Update"));
}

void BookmarkDialog::designSystemChangeEvent(DesignSystemChangeEvent* _event)
{
    AbstractDialog::designSystemChangeEvent(_event);

    d->bookmarkName->setTextColor(DesignSystem::color().onBackground());
    d->bookmarkName->setBackgroundColor(DesignSystem::color().onBackground());

    d->bookmarkColorPopup->setBackgroundColor(DesignSystem::color().background());
    d->bookmarkColorPopup->setTextColor(DesignSystem::color().onBackground());

    UiHelper::initColorsFor(d->cancelButton, UiHelper::DialogDefault);
    UiHelper::initColorsFor(d->saveButton, UiHelper::DialogAccept);

    contentsLayout()->setVerticalSpacing(DesignSystem::compactLayout().px16());
    d->buttonsLayout->setContentsMargins(
        QMarginsF(DesignSystem::layout().px12(), DesignSystem::layout().px12(),
                  DesignSystem::layout().px16(), DesignSystem::layout().px12())
            .toMargins());
}

} // namespace Ui
