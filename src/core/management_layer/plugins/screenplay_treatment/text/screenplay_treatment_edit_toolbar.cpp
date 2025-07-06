#include "screenplay_treatment_edit_toolbar.h"

#include <ui/design_system/design_system.h>
#include <ui/widgets/card/card_popup_with_tree.h>

#include <QAbstractItemModel>
#include <QAction>
#include <QHBoxLayout>


namespace Ui {

class ScreenplayTreatmentEditToolbar::Implementation
{
public:
    explicit Implementation(QWidget* _parent);

    /**
     * @brief Показать попап
     */
    void showPopup(ScreenplayTreatmentEditToolbar* _parent);


    QAction* undoAction = nullptr;
    QAction* redoAction = nullptr;
    QAction* paragraphTypeAction = nullptr;
    QAction* fastFormatAction = nullptr;
    QAction* searchAction = nullptr;
    QAction* commentsAction = nullptr;
    QAction* aiAssistantAction = nullptr;
    QAction* isolationAction = nullptr;

    CardPopupWithTree* popup = nullptr;
};

ScreenplayTreatmentEditToolbar::Implementation::Implementation(QWidget* _parent)
    : undoAction(new QAction)
    , redoAction(new QAction)
    , paragraphTypeAction(new QAction)
    , fastFormatAction(new QAction)
    , searchAction(new QAction)
    , commentsAction(new QAction)
    , aiAssistantAction(new QAction(_parent))
    , isolationAction(new QAction(_parent))
    , popup(new CardPopupWithTree(_parent))
{
}

void ScreenplayTreatmentEditToolbar::Implementation::showPopup(
    ScreenplayTreatmentEditToolbar* _parent)
{
    const auto width = Ui::DesignSystem::floatingToolBar().spacing() * 2
        + _parent->actionCustomWidth(paragraphTypeAction);

    const auto left = QPoint(
        _parent->isLeftToRight() ? (Ui::DesignSystem::floatingToolBar().shadowMargins().left()
                                    + Ui::DesignSystem::floatingToolBar().margins().left()
                                    + Ui::DesignSystem::floatingToolBar().iconSize().width() * 2
                                    + Ui::DesignSystem::floatingToolBar().spacing() * 1
                                    - Ui::DesignSystem::card().shadowMargins().left())
                                 : (Ui::DesignSystem::floatingToolBar().shadowMargins().left()
                                    + Ui::DesignSystem::floatingToolBar().margins().left()
                                    + Ui::DesignSystem::floatingToolBar().iconSize().width() * 4
                                    + Ui::DesignSystem::floatingToolBar().spacing() * 3
                                    - Ui::DesignSystem::card().shadowMargins().left()),
        _parent->rect().bottom() - Ui::DesignSystem::floatingToolBar().shadowMargins().bottom());
    const auto position = _parent->mapToGlobal(left)
        + QPointF(Ui::DesignSystem::textField().margins().left(),
                  -Ui::DesignSystem::textField().margins().bottom());

    popup->showPopup(position.toPoint(), _parent->height(), width,
                     popup->contentModel()->rowCount());
}


// ****


ScreenplayTreatmentEditToolbar::ScreenplayTreatmentEditToolbar(QWidget* _parent)
    : FloatingToolBar(_parent)
    , d(new Implementation(this))
{
    setCurtain(true);

    d->undoAction->setIconText(u8"\U000f054c");
    addAction(d->undoAction);
    connect(d->undoAction, &QAction::triggered, this, &ScreenplayTreatmentEditToolbar::undoPressed);

    d->redoAction->setIconText(u8"\U000f044e");
    addAction(d->redoAction);
    connect(d->redoAction, &QAction::triggered, this, &ScreenplayTreatmentEditToolbar::redoPressed);

    d->paragraphTypeAction->setText(tr("Scene heading"));
    d->paragraphTypeAction->setIconText(u8"\U000f035d");
    addAction(d->paragraphTypeAction);
    connect(d->paragraphTypeAction, &QAction::triggered, this, [this] {
        d->paragraphTypeAction->setIconText(u8"\U000f0360");
        d->showPopup(this);
    });

    d->fastFormatAction->setIconText(u8"\U000f0328");
    d->fastFormatAction->setCheckable(true);
    addAction(d->fastFormatAction);
    connect(d->fastFormatAction, &QAction::toggled, this,
            &ScreenplayTreatmentEditToolbar::updateTranslations);
    connect(d->fastFormatAction, &QAction::toggled, this,
            &ScreenplayTreatmentEditToolbar::fastFormatPanelVisibleChanged);
    connect(d->fastFormatAction, &QAction::toggled, this, [this](bool _checked) {
        d->paragraphTypeAction->setVisible(!_checked);
        designSystemChangeEvent(nullptr);
    });

    d->searchAction->setIconText(u8"\U000f0349");
    d->searchAction->setShortcut(QKeySequence::Find);
    addAction(d->searchAction);
    connect(d->searchAction, &QAction::triggered, this,
            &ScreenplayTreatmentEditToolbar::searchPressed);

    d->commentsAction->setIconText(u8"\U000f0e31");
    d->commentsAction->setCheckable(true);
    addAction(d->commentsAction);
    connect(d->commentsAction, &QAction::toggled, this,
            &ScreenplayTreatmentEditToolbar::updateTranslations);
    connect(d->commentsAction, &QAction::toggled, this,
            &ScreenplayTreatmentEditToolbar::commentsModeEnabledChanged);

    d->aiAssistantAction->setIconText(u8"\U000F0068");
    d->aiAssistantAction->setCheckable(true);
    addAction(d->aiAssistantAction);
    connect(d->aiAssistantAction, &QAction::toggled, this,
            &ScreenplayTreatmentEditToolbar::updateTranslations);
    connect(d->aiAssistantAction, &QAction::toggled, this,
            &ScreenplayTreatmentEditToolbar::aiAssistantEnabledChanged);

    d->isolationAction->setIconText(u8"\U000F0EFF");
    d->isolationAction->setCheckable(true);
    addAction(d->isolationAction);
    connect(d->isolationAction, &QAction::toggled, this,
            &ScreenplayTreatmentEditToolbar::updateTranslations);
    connect(d->isolationAction, &QAction::toggled, this,
            &ScreenplayTreatmentEditToolbar::itemIsolationEnabledChanged);

    connect(d->popup, &CardPopupWithTree::currentIndexChanged, this,
            [this](const QModelIndex& _index) { emit paragraphTypeChanged(_index); });
    connect(d->popup, &Card::disappeared, this, [this] {
        d->paragraphTypeAction->setIconText(u8"\U000f035d");
        animateHoverOut();
    });
}

ScreenplayTreatmentEditToolbar::~ScreenplayTreatmentEditToolbar() = default;

void ScreenplayTreatmentEditToolbar::setReadOnly(bool _readOnly)
{
    const auto enabled = !_readOnly;
    d->undoAction->setEnabled(enabled);
    d->redoAction->setEnabled(enabled);
    d->paragraphTypeAction->setEnabled(enabled);
}

void ScreenplayTreatmentEditToolbar::setParagraphTypesModel(QAbstractItemModel* _model)
{
    if (d->popup->contentModel() != nullptr) {
        d->popup->contentModel()->disconnect(this);
    }

    d->popup->setContentModel(_model);

    if (_model != nullptr) {
        connect(_model, &QAbstractItemModel::rowsInserted, this,
                [this] { designSystemChangeEvent(nullptr); });
        connect(_model, &QAbstractItemModel::rowsRemoved, this,
                [this] { designSystemChangeEvent(nullptr); });
        if (_model->rowCount() > 0) {
            d->popup->setCurrentIndex(_model->index(0, 0));
        }
    }

    //
    // Обновим внешний вид, чтобы пересчитать ширину элемента с выбором стилей
    //
    designSystemChangeEvent(nullptr);
}

void ScreenplayTreatmentEditToolbar::setCurrentParagraphType(const QModelIndex& _index)
{
    //
    // Не вызываем сигнал о смене типа параграфа, т.к. это сделал не пользователь
    //
    QSignalBlocker blocker(this);

    d->paragraphTypeAction->setText(_index.data().toString());
    d->popup->setCurrentIndex(_index);
}

void ScreenplayTreatmentEditToolbar::setParagraphTypesEnabled(bool _enabled)
{
    d->paragraphTypeAction->setEnabled(_enabled);
}

bool ScreenplayTreatmentEditToolbar::isFastFormatPanelVisible() const
{
    return d->fastFormatAction->isChecked();
}

void ScreenplayTreatmentEditToolbar::setFastFormatPanelVisible(bool _visible)
{
    d->fastFormatAction->setChecked(_visible);
}

QString ScreenplayTreatmentEditToolbar::searchIcon() const
{
    return d->searchAction->iconText();
}

QPointF ScreenplayTreatmentEditToolbar::searchIconPosition() const
{
    const auto allActions = actions();
    const auto visibleActionsSize
        = std::count_if(allActions.begin(), allActions.end(),
                        [](QAction* _action) { return _action->isVisible(); })
        - 2;
    qreal width = Ui::DesignSystem::floatingToolBar().shadowMargins().left()
        + Ui::DesignSystem::floatingToolBar().margins().left()
        + (Ui::DesignSystem::floatingToolBar().iconSize().width()
           + Ui::DesignSystem::floatingToolBar().spacing())
            * visibleActionsSize;
    for (const auto action : allActions) {
        if (!action->isVisible() || action->isSeparator()) {
            continue;
        }

        if (actionCustomWidth(action) > 0) {
            width += actionCustomWidth(action);
            width -= Ui::DesignSystem::floatingToolBar().iconSize().width();
        }
    }

    return QPointF(width,
                   Ui::DesignSystem::floatingToolBar().shadowMargins().top()
                       + Ui::DesignSystem::floatingToolBar().margins().top());
}

bool ScreenplayTreatmentEditToolbar::isCommentsModeEnabled() const
{
    return d->commentsAction->isChecked();
}

void ScreenplayTreatmentEditToolbar::setCommentsModeEnabled(bool _enabled)
{
    d->commentsAction->setChecked(_enabled);
}

bool ScreenplayTreatmentEditToolbar::isAiAssistantEnabled() const
{
    return d->aiAssistantAction->isChecked();
}

void ScreenplayTreatmentEditToolbar::setAiAssistantEnabled(bool _enabled)
{
    d->aiAssistantAction->setChecked(_enabled);
}

bool ScreenplayTreatmentEditToolbar::isItemIsolationEnabled() const
{
    return d->isolationAction->isChecked();
}

void ScreenplayTreatmentEditToolbar::setItemIsolationEnabled(bool _enabled)
{
    d->isolationAction->setChecked(_enabled);
}

bool ScreenplayTreatmentEditToolbar::canAnimateHoverOut() const
{
    return !d->popup->isVisible();
}

void ScreenplayTreatmentEditToolbar::updateTranslations()
{
    d->undoAction->setToolTip(
        tr("Undo last action")
        + QString(" (%1)").arg(
            QKeySequence(QKeySequence::Undo).toString(QKeySequence::NativeText)));
    d->redoAction->setToolTip(
        tr("Redo last action")
        + QString(" (%1)").arg(
            QKeySequence(QKeySequence::Redo).toString(QKeySequence::NativeText)));
    d->paragraphTypeAction->setToolTip(tr("Current paragraph format"));
    d->fastFormatAction->setToolTip(d->fastFormatAction->isChecked()
                                        ? tr("Hide fast format panel")
                                        : tr("Show fast format panel"));
    d->searchAction->setToolTip(
        tr("Search text")
        + QString(" (%1)").arg(
            QKeySequence(QKeySequence::Find).toString(QKeySequence::NativeText)));
    d->commentsAction->setToolTip(d->commentsAction->isChecked() ? tr("Disable review mode")
                                                                 : tr("Enable review mode"));
    d->aiAssistantAction->setToolTip(d->aiAssistantAction->isChecked() ? tr("Disable AI assistant")
                                                                       : tr("Enable AI assistant"));
    d->isolationAction->setToolTip(d->isolationAction->isChecked()
                                       ? tr("Show full text")
                                       : tr("Show only current scene text"));
}

void ScreenplayTreatmentEditToolbar::designSystemChangeEvent(DesignSystemChangeEvent* _event)
{
    FloatingToolBar::designSystemChangeEvent(_event);

    setActionCustomWidth(
        d->paragraphTypeAction,
        static_cast<int>(Ui::DesignSystem::treeOneLineItem().margins().left())
            + d->popup->sizeHintForColumn(0)
            + static_cast<int>(Ui::DesignSystem::treeOneLineItem().margins().right()));

    d->popup->setBackgroundColor(Ui::DesignSystem::color().background());
    d->popup->setTextColor(Ui::DesignSystem::color().onBackground());

    resize(sizeHint());
}

} // namespace Ui
