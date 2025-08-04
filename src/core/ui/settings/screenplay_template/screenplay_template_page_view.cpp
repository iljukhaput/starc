#include "screenplay_template_page_view.h"

#include "../widgets/page_layout.h"

#include <domain/document_object.h>
#include <ui/design_system/design_system.h>
#include <ui/widgets/card/card.h>
#include <ui/widgets/check_box/check_box.h>
#include <ui/widgets/label/label.h>
#include <ui/widgets/radio_button/radio_button.h>
#include <ui/widgets/radio_button/radio_button_group.h>
#include <ui/widgets/scroll_bar/scroll_bar.h>
#include <ui/widgets/slider/slider.h>
#include <ui/widgets/text_field/text_field.h>
#include <utils/helpers/color_helper.h>
#include <utils/helpers/measurement_helper.h>
#include <utils/helpers/ui_helper.h>

#include <QEvent>
#include <QGridLayout>
#include <QScrollArea>


namespace Ui {

class ScreenplayTemplatePageView::Implementation
{
public:
    explicit Implementation(QWidget* _parent);

    /**
     * @brief Получить заданное значение в текущей метрике
     */
    qreal valueInCurrentMetrics(qreal _value);


    bool useMm = true;

    QScrollArea* content = nullptr;

    Card* card = nullptr;
    QGridLayout* cardLayout = nullptr;

    TextField* templateName = nullptr;
    CaptionLabel* pageFormatTitle = nullptr;
    RadioButton* pageFormatA4 = nullptr;
    RadioButton* pageFormatLetter = nullptr;
    CaptionLabel* pageMarginsTitle = nullptr;
    TextField* leftMargin = nullptr;
    TextField* topMargin = nullptr;
    int marginsSplitterRow = 0;
    TextField* rightMargin = nullptr;
    TextField* bottomMargin = nullptr;
    CaptionLabel* pageNumbersAlignHorizontalTitle = nullptr;
    RadioButton* pageNumbersAlignTop = nullptr;
    RadioButton* pageNumbersAlignBottom = nullptr;
    CaptionLabel* pageNumbersAlignVerticalTitle = nullptr;
    RadioButton* pageNumbersAlignLeft = nullptr;
    RadioButton* pageNumbersAlignCenter = nullptr;
    RadioButton* pageNumbersAlignRight = nullptr;
    CheckBox* isFirstPageNumberVisible = nullptr;
    CaptionLabel* pageSplitterTitle = nullptr;
    Slider* pageSplitter = nullptr;
    CaptionLabel* pageSplitterLeft = nullptr;
    CaptionLabel* pageSplitterRight = nullptr;

    PageLayout* pageLayoutPreview = nullptr;
};

ScreenplayTemplatePageView::Implementation::Implementation(QWidget* _parent)
    : content(UiHelper::createScrollArea(_parent))
    , card(new Card(content))
    , cardLayout(new QGridLayout)
    , templateName(new TextField(card))
    , pageFormatTitle(new CaptionLabel(card))
    , pageFormatA4(new RadioButton(card))
    , pageFormatLetter(new RadioButton(card))
    , pageMarginsTitle(new CaptionLabel(card))
    , leftMargin(new TextField(card))
    , topMargin(new TextField(card))
    , rightMargin(new TextField(card))
    , bottomMargin(new TextField(card))
    , pageNumbersAlignHorizontalTitle(new CaptionLabel(card))
    , pageNumbersAlignTop(new RadioButton(card))
    , pageNumbersAlignBottom(new RadioButton(card))
    , pageNumbersAlignVerticalTitle(new CaptionLabel(card))
    , pageNumbersAlignLeft(new RadioButton(card))
    , pageNumbersAlignCenter(new RadioButton(card))
    , pageNumbersAlignRight(new RadioButton(card))
    , isFirstPageNumberVisible(new CheckBox(card))
    , pageSplitterTitle(new CaptionLabel(card))
    , pageSplitter(new Slider(card))
    , pageSplitterLeft(new CaptionLabel(card))
    , pageSplitterRight(new CaptionLabel(card))
    , pageLayoutPreview(new PageLayout(card))
{
    auto pageFormatGroup = new RadioButtonGroup(card);
    pageFormatGroup->add(pageFormatA4);
    pageFormatGroup->add(pageFormatLetter);
    pageFormatA4->setChecked(true);
    //
    auto verticalAlignmentGroup = new RadioButtonGroup(card);
    verticalAlignmentGroup->add(pageNumbersAlignTop);
    verticalAlignmentGroup->add(pageNumbersAlignBottom);
    pageNumbersAlignTop->setChecked(true);
    //
    auto horizontalAlignmentGroup = new RadioButtonGroup(card);
    horizontalAlignmentGroup->add(pageNumbersAlignLeft);
    horizontalAlignmentGroup->add(pageNumbersAlignCenter);
    horizontalAlignmentGroup->add(pageNumbersAlignRight);
    pageNumbersAlignLeft->setChecked(true);

    pageSplitter->setMaximumValue(100);
    pageSplitter->setDefaultValue(50);
    pageSplitterLeft->setText("50%");
    pageSplitterRight->setText("50%");

    cardLayout->setContentsMargins({});
    cardLayout->setSpacing(0);
    int row = 0;
    cardLayout->addWidget(templateName, row++, 0, 1, 3);
    cardLayout->addWidget(pageFormatTitle, row++, 0, 1, 2);
    cardLayout->addWidget(pageFormatA4, row, 0, 1, 2);
    cardLayout->addWidget(pageLayoutPreview, row++, 2, 16, 1);
    cardLayout->addWidget(pageFormatLetter, row++, 0, 1, 2);
    cardLayout->addWidget(pageMarginsTitle, row++, 0, 1, 2);
    cardLayout->addWidget(leftMargin, row, 0, 1, 1);
    cardLayout->addWidget(rightMargin, row++, 1, 1, 1);
    marginsSplitterRow = row++;
    cardLayout->addWidget(topMargin, row, 0, 1, 1);
    cardLayout->addWidget(bottomMargin, row++, 1, 1, 1);
    cardLayout->addWidget(pageNumbersAlignVerticalTitle, row++, 0, 1, 2);
    cardLayout->addWidget(pageNumbersAlignTop, row++, 0, 1, 2);
    cardLayout->addWidget(pageNumbersAlignBottom, row++, 0, 1, 2);
    cardLayout->addWidget(pageNumbersAlignHorizontalTitle, row++, 0, 1, 2);
    cardLayout->addWidget(pageNumbersAlignLeft, row++, 0, 1, 2);
    cardLayout->addWidget(pageNumbersAlignCenter, row++, 0, 1, 2);
    cardLayout->addWidget(pageNumbersAlignRight, row++, 0, 1, 2);
    cardLayout->addWidget(isFirstPageNumberVisible, row++, 0, 1, 2);
    cardLayout->addWidget(pageSplitterTitle, row++, 0, 1, 2);
    cardLayout->addWidget(pageSplitter, row++, 0, 1, 2);
    cardLayout->addWidget(pageSplitterLeft, row, 0, 1, 1, Qt::AlignLeft);
    cardLayout->addWidget(pageSplitterRight, row++, 1, 1, 1, Qt::AlignRight);
    cardLayout->setColumnStretch(0, 1);
    cardLayout->setColumnStretch(1, 1);
    cardLayout->setColumnStretch(2, 2);
    card->setContentLayout(cardLayout);

    auto layout = qobject_cast<QVBoxLayout*>(content->widget()->layout());
    layout->addWidget(card);
    layout->addStretch();
}

qreal ScreenplayTemplatePageView::Implementation::valueInCurrentMetrics(qreal _value)
{
    return useMm ? _value : MeasurementHelper::inchToMm(_value);
}


//****


ScreenplayTemplatePageView::ScreenplayTemplatePageView(QWidget* _parent)
    : Widget(_parent)
    , d(new Implementation(this))
{
    //
    // Фильтруем события всех детей, чтобы ловить фокусирование
    //
    for (auto child : findChildren<QWidget*>()) {
        child->installEventFilter(this);
    }

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->setSpacing(0);
    layout->addWidget(d->content);

    connect(d->pageFormatA4, &RadioButton::checkedChanged, this, [this](bool _checked) {
        d->pageLayoutPreview->setPageSize(_checked ? QPageSize::A4 : QPageSize::Letter);
        emit pageChanged();
    });
    auto updateMargins = [this] {
        d->pageLayoutPreview->setMargins(
            QMarginsF(d->valueInCurrentMetrics(d->leftMargin->text().toDouble()),
                      d->valueInCurrentMetrics(d->topMargin->text().toDouble()),
                      d->valueInCurrentMetrics(d->rightMargin->text().toDouble()),
                      d->valueInCurrentMetrics(d->bottomMargin->text().toDouble())));
        emit pageChanged();
    };
    connect(d->leftMargin, &TextField::textChanged, this, updateMargins);
    connect(d->topMargin, &TextField::textChanged, this, updateMargins);
    connect(d->rightMargin, &TextField::textChanged, this, updateMargins);
    connect(d->bottomMargin, &TextField::textChanged, this, updateMargins);
    auto updatePageNumberAlignment = [this] {
        d->pageLayoutPreview->setPageNumberAlignment(
            (d->pageNumbersAlignTop->isChecked() ? Qt::AlignTop : Qt::AlignBottom)
            | (d->pageNumbersAlignLeft->isChecked()
                   ? Qt::AlignLeft
                   : (d->pageNumbersAlignCenter->isChecked() ? Qt::AlignHCenter : Qt::AlignRight)));
        emit pageChanged();
    };
    connect(d->pageNumbersAlignTop, &RadioButton::checkedChanged, this, updatePageNumberAlignment);
    connect(d->pageNumbersAlignLeft, &RadioButton::checkedChanged, this, updatePageNumberAlignment);
    connect(d->pageNumbersAlignCenter, &RadioButton::checkedChanged, this,
            updatePageNumberAlignment);
    connect(d->isFirstPageNumberVisible, &CheckBox::checkedChanged, this,
            &ScreenplayTemplatePageView::pageChanged);
    connect(d->pageSplitter, &Slider::valueChanged, this, [this](int _value) {
        d->pageSplitterLeft->setText(QString("%1%").arg(_value));
        d->pageSplitterRight->setText(QString("%1%").arg(d->pageSplitter->maximumValue() - _value));
        d->pageLayoutPreview->setPageSplitter(
            _value / static_cast<qreal>(d->pageSplitter->maximumValue()));
        emit pageChanged();
    });
}

ScreenplayTemplatePageView::~ScreenplayTemplatePageView() = default;

void ScreenplayTemplatePageView::setUseMm(bool _mm)
{
    if (d->useMm == _mm) {
        return;
    }

    d->useMm = _mm;

    //
    // Обновим переводы
    //
    updateTranslations();
}

void ScreenplayTemplatePageView::configureTemplateFor(Domain::DocumentObjectType _type)
{
    for (auto widget : std::vector<Widget*>{
             d->pageSplitter,
             d->pageSplitterTitle,
             d->pageSplitterLeft,
             d->pageSplitterRight,
         }) {
        widget->setVisible(_type != Domain::DocumentObjectType::SimpleText
                           && _type != Domain::DocumentObjectType::Novel);
    }
}

QString ScreenplayTemplatePageView::templateName() const
{
    return d->templateName->text();
}

void ScreenplayTemplatePageView::setTemplateName(const QString& _name)
{
    d->templateName->setText(_name);
}

QPageSize::PageSizeId ScreenplayTemplatePageView::pageSizeId() const
{
    return d->pageFormatA4->isChecked() ? QPageSize::A4 : QPageSize::Letter;
}

void ScreenplayTemplatePageView::setPageSize(QPageSize::PageSizeId _pageSize)
{
    if (_pageSize == QPageSize::A4) {
        d->pageFormatA4->setChecked(true);
    } else {
        d->pageFormatLetter->setChecked(true);
    }
}

QMarginsF ScreenplayTemplatePageView::pageMargins() const
{
    return { d->leftMargin->text().toDouble(), d->topMargin->text().toDouble(),
             d->rightMargin->text().toDouble(), d->bottomMargin->text().toDouble() };
}

void ScreenplayTemplatePageView::setPageMargins(const QMarginsF& _margins)
{
    d->leftMargin->setText(QString::number(_margins.left()));
    d->topMargin->setText(QString::number(_margins.top()));
    d->rightMargin->setText(QString::number(_margins.right()));
    d->bottomMargin->setText(QString::number(_margins.bottom()));
}

Qt::Alignment ScreenplayTemplatePageView::pageNumbersAlignment() const
{
    return (d->pageNumbersAlignTop->isChecked() ? Qt::AlignTop : Qt::AlignBottom)
        | (d->pageNumbersAlignLeft->isChecked()
               ? Qt::AlignLeft
               : (d->pageNumbersAlignCenter->isChecked() ? Qt::AlignHCenter : Qt::AlignRight));
}

void ScreenplayTemplatePageView::setPageNumbersAlignment(Qt::Alignment _alignment)
{
    if (_alignment.testFlag(Qt::AlignTop)) {
        d->pageNumbersAlignTop->setChecked(true);
    } else {
        d->pageNumbersAlignBottom->setChecked(true);
    }

    if (_alignment.testFlag(Qt::AlignLeft)) {
        d->pageNumbersAlignLeft->setChecked(true);
    } else if (_alignment.testFlag(Qt::AlignHCenter)) {
        d->pageNumbersAlignCenter->setChecked(true);
    } else {
        d->pageNumbersAlignRight->setChecked(true);
    }
}

bool ScreenplayTemplatePageView::isFirstPageNumberVisible() const
{
    return d->isFirstPageNumberVisible->isChecked();
}

void ScreenplayTemplatePageView::setFirstPageNumberVisible(bool _visible)
{
    d->isFirstPageNumberVisible->setChecked(_visible);
}

int ScreenplayTemplatePageView::leftHalfOfPageWidthPercents() const
{
    return d->pageSplitter->value();
}

void ScreenplayTemplatePageView::setLeftHalfOfPage(int _value)
{
    d->pageSplitter->setValue(_value);
}

bool ScreenplayTemplatePageView::eventFilter(QObject* _watched, QEvent* _event)
{
    if (_event->type() == QEvent::FocusIn) {
        if (_watched == d->leftMargin) {
            d->pageLayoutPreview->setCurrentItem(PageLayoutItem::LeftMargin);
        } else if (_watched == d->topMargin) {
            d->pageLayoutPreview->setCurrentItem(PageLayoutItem::TopMargin);
        } else if (_watched == d->rightMargin) {
            d->pageLayoutPreview->setCurrentItem(PageLayoutItem::RightMargin);
        } else if (_watched == d->bottomMargin) {
            d->pageLayoutPreview->setCurrentItem(PageLayoutItem::BottomMargin);
        } else if (_watched == d->pageNumbersAlignTop || _watched == d->pageNumbersAlignBottom
                   || _watched == d->pageNumbersAlignLeft || _watched == d->pageNumbersAlignCenter
                   || _watched == d->pageNumbersAlignRight || d->isFirstPageNumberVisible) {
            d->pageLayoutPreview->setCurrentItem(PageLayoutItem::PageNumber);
        } else if (_watched == d->pageSplitter) {
            d->pageLayoutPreview->setCurrentItem(PageLayoutItem::Splitter);
        } else {
            d->pageLayoutPreview->setCurrentItem(PageLayoutItem::None);
        }
    }

    return Widget::eventFilter(_watched, _event);
}

void ScreenplayTemplatePageView::updateTranslations()
{
    const auto metricSystem = d->useMm ? tr("mm") : tr("inch");
    d->templateName->setLabel(tr("Template name"));
    d->pageFormatTitle->setText(tr("Page format"));
    auto setPageFormatLabel = [this, metricSystem](RadioButton* _button, const QString& _text,
                                                   qreal _widthMm, qreal _heightMm) {
        auto width = _widthMm;
        auto height = _heightMm;
        if (!d->useMm) {
            width = MeasurementHelper::mmToInch(_widthMm);
            height = MeasurementHelper::mmToInch(_heightMm);
        }
        _button->setText(QString("%1 (%2x%3 %4)")
                             .arg(_text)
                             .arg(width, 0, 'g', 3)
                             .arg(height, 0, 'g', 3)
                             .arg(metricSystem));
    };
    setPageFormatLabel(d->pageFormatA4, "A4", 210, 297);
    setPageFormatLabel(d->pageFormatLetter, "Letter", 215.9, 279.4);
    d->pageMarginsTitle->setText(tr("Page margins"));
    d->leftMargin->setLabel(tr("Left"));
    d->leftMargin->setSuffix(metricSystem);
    d->topMargin->setLabel(tr("Top"));
    d->topMargin->setSuffix(metricSystem);
    d->rightMargin->setLabel(tr("Right"));
    d->rightMargin->setSuffix(metricSystem);
    d->bottomMargin->setLabel(tr("Bottom"));
    d->bottomMargin->setSuffix(metricSystem);
    d->pageNumbersAlignVerticalTitle->setText(tr("Page numbering alignment by vertical"));
    d->pageNumbersAlignTop->setText(tr("Top"));
    d->pageNumbersAlignBottom->setText(tr("Bottom"));
    d->pageNumbersAlignHorizontalTitle->setText(tr("by horizontal"));
    d->pageNumbersAlignLeft->setText(tr("Left"));
    d->pageNumbersAlignCenter->setText(tr("Center"));
    d->pageNumbersAlignRight->setText(tr("Right"));
    d->isFirstPageNumberVisible->setText(tr("Show number on the first page"));
    d->pageSplitterTitle->setText(tr("Split page into columns in proportion"));
}

void ScreenplayTemplatePageView::designSystemChangeEvent(DesignSystemChangeEvent* _event)
{
    Widget::designSystemChangeEvent(_event);

    setBackgroundColor(DesignSystem::color().surface());
    d->content->widget()->layout()->setContentsMargins(
        QMarginsF(DesignSystem::layout().px24(), DesignSystem::compactLayout().topContentMargin(),
                  DesignSystem::layout().px24(), DesignSystem::compactLayout().px24())
            .toMargins());
    d->card->setBackgroundColor(DesignSystem::color().background());
    d->card->setContentsMargins(0, DesignSystem::layout().px24(), 0, DesignSystem::layout().px24());

    for (auto textField :
         { d->templateName, d->leftMargin, d->topMargin, d->rightMargin, d->bottomMargin }) {
        textField->setBackgroundColor(DesignSystem::color().onBackground());
        textField->setTextColor(DesignSystem::color().onBackground());
    }
    for (auto textField : { d->leftMargin, d->topMargin }) {
        textField->setCustomMargins({ isLeftToRight() ? DesignSystem::layout().px24() : 0.0, 0.0,
                                      isLeftToRight() ? 0.0 : DesignSystem::layout().px24(), 0.0 });
    }
    for (auto textField : { d->rightMargin, d->bottomMargin }) {
        textField->setCustomMargins({ isLeftToRight() ? DesignSystem::compactLayout().px16()
                                                      : DesignSystem::layout().px24(),
                                      0.0,
                                      isLeftToRight() ? DesignSystem::layout().px24()
                                                      : DesignSystem::compactLayout().px16(),
                                      0.0 });
    }

    for (auto title : { d->pageFormatTitle, d->pageMarginsTitle, d->pageNumbersAlignHorizontalTitle,
                        d->pageNumbersAlignVerticalTitle, d->pageSplitterTitle, d->pageSplitterLeft,
                        d->pageSplitterRight }) {
        title->setBackgroundColor(DesignSystem::color().background());
        title->setTextColor(ColorHelper::transparent(DesignSystem::color().onBackground(),
                                                     DesignSystem::inactiveTextOpacity()));
        title->setContentsMargins(DesignSystem::layout().px24(),
                                  DesignSystem::compactLayout().px24(),
                                  DesignSystem::layout().px24(), DesignSystem::layout().px4());
    }
    d->pageMarginsTitle->setContentsMargins(
        DesignSystem::layout().px24(), DesignSystem::compactLayout().px16(),
        DesignSystem::layout().px24(), DesignSystem::layout().px12());
    d->pageNumbersAlignHorizontalTitle->setContentsMargins(
        DesignSystem::layout().px24(), DesignSystem::layout().px4(), DesignSystem::layout().px24(),
        DesignSystem::layout().px4());
    d->pageSplitterLeft->setContentsMargins(
        DesignSystem::layout().px24(), DesignSystem::layout().px4(), DesignSystem::layout().px24(),
        DesignSystem::layout().px24());
    d->pageSplitterRight->setContentsMargins(
        DesignSystem::layout().px24(), DesignSystem::layout().px4(), DesignSystem::layout().px24(),
        DesignSystem::layout().px24());

    for (auto widget : std::vector<Widget*>{
             d->pageFormatA4,
             d->pageFormatLetter,
             d->pageNumbersAlignTop,
             d->pageNumbersAlignBottom,
             d->pageNumbersAlignLeft,
             d->pageNumbersAlignCenter,
             d->pageNumbersAlignRight,
             d->isFirstPageNumberVisible,
         }) {
        widget->setBackgroundColor(DesignSystem::color().background());
        widget->setTextColor(DesignSystem::color().onBackground());
    }

    d->cardLayout->setRowMinimumHeight(d->marginsSplitterRow, DesignSystem::compactLayout().px16());

    d->pageSplitter->setBackgroundColor(DesignSystem::color().background());
    d->pageSplitter->setContentsMargins(DesignSystem::layout().px24(), 0,
                                        DesignSystem::layout().px24(), 0);

    d->pageLayoutPreview->setBackgroundColor(DesignSystem::color().background());
    d->pageLayoutPreview->setTextColor(DesignSystem::color().onBackground());
    d->pageLayoutPreview->setContentsMargins(0, 0, DesignSystem::layout().px24(), 0);
}

} // namespace Ui
