#include "comic_book_text_manager.h"

#include "comic_book_text_view.h"

#include <business_layer/model/comic_book/text/comic_book_text_model.h>
#include <business_layer/model/text/text_model_text_item.h>
#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_object.h>
#include <ui/modules/bookmarks/bookmark_dialog.h>

#include <QApplication>
#include <QFileDialog>


namespace ManagementLayer {

namespace {
const QString kSettingsKey = "comic-book-text";
QString cursorPositionFor(Domain::DocumentObject* _item)
{
    return QString("%1/%2/last-cursor").arg(kSettingsKey, _item->uuid().toString());
}
QString verticalScrollFor(Domain::DocumentObject* _item)
{
    return QString("%1/%2/vertical-scroll").arg(kSettingsKey, _item->uuid().toString());
}
} // namespace

class ComicBookTextManager::Implementation
{
public:
    explicit Implementation(ComicBookTextManager* _q);

    /**
     * @brief Создать представление
     */
    Ui::ComicBookTextView* createView(BusinessLayer::AbstractModel* _model);

    /**
     * @brief Связать заданную модель и представление
     */
    void setModelForView(BusinessLayer::AbstractModel* _model, Ui::ComicBookTextView* _view);

    /**
     * @brief Получить модель связанную с заданным представлением
     */
    QPointer<BusinessLayer::ComicBookTextModel> modelForView(Ui::ComicBookTextView* _view) const;

    /**
     * @brief Работа с параметрами отображения представления
     */
    void loadModelAndViewSettings(BusinessLayer::AbstractModel* _model,
                                  Ui::ComicBookTextView* _view);
    void saveModelAndViewSettings(BusinessLayer::AbstractModel* _model,
                                  Ui::ComicBookTextView* _view);


    ComicBookTextManager* q = nullptr;

    /**
     * @brief Предаставление для основного окна
     */
    Ui::ComicBookTextView* view = nullptr;
    Ui::ComicBookTextView* secondaryView = nullptr;

    /**
     * @brief Все созданные представления с моделями, которые в них отображаются
     */
    struct ViewAndModel {
        QPointer<Ui::ComicBookTextView> view;
        QPointer<BusinessLayer::ComicBookTextModel> model;
    };
    QVector<ViewAndModel> allViews;
};

ComicBookTextManager::Implementation::Implementation(ComicBookTextManager* _q)
    : q(_q)
{
}

Ui::ComicBookTextView* ComicBookTextManager::Implementation::createView(
    BusinessLayer::AbstractModel* _model)
{
    auto view = new Ui::ComicBookTextView;
    setModelForView(_model, view);

    auto showBookmarkDialog = [this, view](Ui::BookmarkDialog::DialogType _type) {
        auto item = modelForView(view)->itemForIndex(view->currentModelIndex());
        if (item->type() != BusinessLayer::TextModelItemType::Text) {
            return;
        }

        auto dialog = new Ui::BookmarkDialog(view->topLevelWidget());
        dialog->setDialogType(_type);
        if (_type == Ui::BookmarkDialog::DialogType::Edit) {
            const auto textItem = static_cast<BusinessLayer::TextModelTextItem*>(item);
            dialog->setBookmarkName(textItem->bookmark()->name);
            dialog->setBookmarkColor(textItem->bookmark()->color);
        }
        connect(dialog, &Ui::BookmarkDialog::savePressed, q, [this, view, item, dialog] {
            auto textItem = static_cast<BusinessLayer::TextModelTextItem*>(item);
            textItem->setBookmark({ dialog->bookmarkColor(), dialog->bookmarkName() });
            modelForView(view)->updateItem(textItem);

            dialog->hideDialog();
        });
        connect(dialog, &Ui::BookmarkDialog::disappeared, dialog, &Ui::BookmarkDialog::deleteLater);

        //
        // Отображаем диалог
        //
        dialog->showDialog();
    };
    connect(view, &Ui::ComicBookTextView::addBookmarkRequested, q, [showBookmarkDialog] {
        showBookmarkDialog(Ui::BookmarkDialog::DialogType::CreateNew);
    });
    connect(view, &Ui::ComicBookTextView::editBookmarkRequested, q,
            [showBookmarkDialog] { showBookmarkDialog(Ui::BookmarkDialog::DialogType::Edit); });
    connect(view, &Ui::ComicBookTextView::createBookmarkRequested, q,
            [this, view](const QString& _text, const QColor& _color) {
                auto item = modelForView(view)->itemForIndex(view->currentModelIndex());
                if (item->type() != BusinessLayer::TextModelItemType::Text) {
                    return;
                }

                auto textItem = static_cast<BusinessLayer::TextModelTextItem*>(item);
                textItem->setBookmark({ _color, _text });
                modelForView(view)->updateItem(textItem);
            });
    connect(view, &Ui::ComicBookTextView::changeBookmarkRequested, q,
            [this, view](const QModelIndex& _index, const QString& _text, const QColor& _color) {
                auto item = modelForView(view)->itemForIndex(_index);
                if (item->type() != BusinessLayer::TextModelItemType::Text) {
                    return;
                }

                auto textItem = static_cast<BusinessLayer::TextModelTextItem*>(item);
                textItem->setBookmark({ _color, _text });
                modelForView(view)->updateItem(textItem);
            });
    connect(view, &Ui::ComicBookTextView::removeBookmarkRequested, q, [this, view] {
        auto item = modelForView(view)->itemForIndex(view->currentModelIndex());
        if (item->type() != BusinessLayer::TextModelItemType::Text) {
            return;
        }

        auto textItem = static_cast<BusinessLayer::TextModelTextItem*>(item);
        textItem->clearBookmark();
        modelForView(view)->updateItem(textItem);
    });
    //
    connect(view, &Ui::ComicBookTextView::rephraseTextRequested, q,
            &ComicBookTextManager::rephraseTextRequested);
    connect(view, &Ui::ComicBookTextView::expandTextRequested, q,
            &ComicBookTextManager::expandTextRequested);
    connect(view, &Ui::ComicBookTextView::shortenTextRequested, q,
            &ComicBookTextManager::shortenTextRequested);
    connect(view, &Ui::ComicBookTextView::insertTextRequested, q,
            &ComicBookTextManager::insertTextRequested);
    connect(view, &Ui::ComicBookTextView::summarizeTextRequested, q,
            &ComicBookTextManager::summarizeTextRequested);
    connect(view, &Ui::ComicBookTextView::translateTextRequested, q,
            &ComicBookTextManager::translateTextRequested);
    connect(view, &Ui::ComicBookTextView::translateDocumentRequested, q,
            [this, view](const QString& _languageCode) {
                const auto model = modelForView(view);
                QVector<QString> groups;
                QString group;
                std::function<void(const QModelIndex&)> findGroups;
                findGroups
                    = [&findGroups, model, &groups, &group](const QModelIndex& _parentItemIndex) {
                          for (int row = 0; row < model->rowCount(_parentItemIndex); ++row) {
                              const auto itemIndex = model->index(row, 0, _parentItemIndex);
                              const auto item = model->itemForIndex(itemIndex);
                              switch (item->type()) {
                              case BusinessLayer::TextModelItemType::Folder: {
                                  findGroups(itemIndex);
                                  break;
                              }

                              case BusinessLayer::TextModelItemType::Group: {
                                  if (!group.isEmpty()) {
                                      groups.append(group);
                                      group.clear();
                                  }

                                  findGroups(itemIndex);
                                  break;
                              }

                              case BusinessLayer::TextModelItemType::Text: {
                                  const auto textItem
                                      = static_cast<const BusinessLayer::TextModelTextItem*>(item);
                                  if (!textItem->text().isEmpty()) {
                                      if (!group.isEmpty()) {
                                          group.append("\n");
                                      }
                                      group.append(textItem->text());
                                  }
                                  break;
                              }

                              default: {
                                  break;
                              }
                              }
                          }
                      };
                findGroups({});
                if (!group.isEmpty()) {
                    groups.append(group);
                }
                emit q->translateDocumentRequested(groups, _languageCode,
                                                   Domain::DocumentObjectType::ComicBookText);
            });
    connect(view, &Ui::ComicBookTextView::generateTextRequested, q, [this](const QString& _text) {
        emit q->generateTextRequested({}, _text, ". Write result in fountain format.");
    });
    connect(view, &Ui::ComicBookTextView::buyCreditsRequested, q,
            &ComicBookTextManager::buyCreditsRequested);

    return view;
}

void ComicBookTextManager::Implementation::setModelForView(BusinessLayer::AbstractModel* _model,
                                                           Ui::ComicBookTextView* _view)
{
    constexpr int invalidIndex = -1;
    int viewIndex = invalidIndex;
    for (int index = 0; index < allViews.size(); ++index) {
        if (allViews[index].view == _view) {
            if (allViews[index].model == _model) {
                return;
            }

            viewIndex = index;
            break;
        }
    }

    //
    // Если модель была задана
    //
    if (viewIndex != invalidIndex && allViews[viewIndex].model != nullptr) {
        //
        // ... сохраняем параметры
        //
        saveModelAndViewSettings(allViews[viewIndex].model, _view);
        //
        // ... разрываем соединения
        //
        _view->disconnect(allViews[viewIndex].model);
        allViews[viewIndex].model->disconnect(_view);
    }

    //
    // Определяем новую модель
    //
    auto model = qobject_cast<BusinessLayer::ComicBookTextModel*>(_model);
    _view->setModel(model);

    //
    // Обновляем связь представления с моделью
    //
    if (viewIndex != invalidIndex) {
        allViews[viewIndex].model = model;
    }
    //
    // Или сохраняем связь представления с моделью
    //
    else {
        allViews.append({ _view, model });
    }

    //
    // Если новая модель задана
    //
    if (model != nullptr) {
        //
        // ... загрузим параметры
        //
        loadModelAndViewSettings(model, _view);
        //
        // ... настраиваем соединения
        //
    }
}

QPointer<BusinessLayer::ComicBookTextModel> ComicBookTextManager::Implementation::modelForView(
    Ui::ComicBookTextView* _view) const
{
    for (auto& viewAndModel : allViews) {
        if (viewAndModel.view == _view) {
            return viewAndModel.model;
        }
    }
    return {};
}

void ComicBookTextManager::Implementation::loadModelAndViewSettings(
    BusinessLayer::AbstractModel* _model, Ui::ComicBookTextView* _view)
{
    const auto cursorPosition = settingsValue(cursorPositionFor(_model->document()), 0).toInt();
    _view->setCursorPosition(cursorPosition);
    const auto verticalScroll = settingsValue(verticalScrollFor(_model->document()), 0).toInt();
    _view->setVerticalScroll(verticalScroll);

    _view->loadViewSettings();
}

void ComicBookTextManager::Implementation::saveModelAndViewSettings(
    BusinessLayer::AbstractModel* _model, Ui::ComicBookTextView* _view)
{
    setSettingsValue(cursorPositionFor(_model->document()), _view->cursorPosition());
    setSettingsValue(verticalScrollFor(_model->document()), _view->verticalScroll());

    _view->saveViewSettings();
}


// ****


ComicBookTextManager::ComicBookTextManager(QObject* _parent)
    : QObject(_parent)
    , d(new Implementation(this))
{
}

ComicBookTextManager::~ComicBookTextManager() = default;

QObject* ComicBookTextManager::asQObject()
{
    return this;
}

Ui::IDocumentView* ComicBookTextManager::view()
{
    return d->view;
}

Ui::IDocumentView* ComicBookTextManager::view(BusinessLayer::AbstractModel* _model)
{
    if (d->view == nullptr) {
        d->view = d->createView(_model);

        //
        // Наружу даём сигналы только от первичного представления, только оно может
        // взаимодействовать с навигатором документа
        //
        connect(d->view, &Ui::ComicBookTextView::currentModelIndexChanged, this,
                &ComicBookTextManager::currentModelIndexChanged);
    } else {
        d->setModelForView(_model, d->view);
    }

    return d->view;
}

Ui::IDocumentView* ComicBookTextManager::secondaryView()
{
    return d->secondaryView;
}

Ui::IDocumentView* ComicBookTextManager::secondaryView(BusinessLayer::AbstractModel* _model)
{
    if (d->secondaryView == nullptr) {
        d->secondaryView = d->createView(_model);
    } else {
        d->setModelForView(_model, d->secondaryView);
    }

    return d->secondaryView;
}

Ui::IDocumentView* ComicBookTextManager::createView(BusinessLayer::AbstractModel* _model)
{
    return d->createView(_model);
}

void ComicBookTextManager::resetModels()
{
    for (auto& viewAndModel : d->allViews) {
        if (viewAndModel.view.isNull()) {
            continue;
        }

        d->setModelForView(nullptr, viewAndModel.view);
    }
}

void ComicBookTextManager::reconfigure(const QStringList& _changedSettingsKeys)
{
    for (const auto& view : std::as_const(d->allViews)) {
        if (!view.view.isNull()) {
            view.view->reconfigure(_changedSettingsKeys);
        }
    }
}

void ComicBookTextManager::bind(IDocumentManager* _manager)
{
    //
    // Т.к. навигатор соединяется только с главным инстансом редактора, проверяем создан ли он
    //
    if (d->view == nullptr) {
        return;
    }

    Q_ASSERT(_manager);

    const auto isConnectedFirstTime
        = connect(_manager->asQObject(), SIGNAL(currentModelIndexChanged(QModelIndex)), this,
                  SLOT(setCurrentModelIndex(QModelIndex)), Qt::UniqueConnection);

    //
    // Ставим в очередь событие нотификацию о смене текущей сцены,
    // чтобы навигатор отобразил её при первом открытии
    //
    if (isConnectedFirstTime) {
        QMetaObject::invokeMethod(
            this, [this] { emit currentModelIndexChanged(d->view->currentModelIndex()); },
            Qt::QueuedConnection);
    }
}

void ComicBookTextManager::saveSettings()
{
    for (auto& viewAndModel : d->allViews) {
        if (viewAndModel.model.isNull() || viewAndModel.view.isNull()) {
            continue;
        }

        d->saveModelAndViewSettings(viewAndModel.model, viewAndModel.view);
    }
}

void ComicBookTextManager::setEditingMode(DocumentEditingMode _mode)
{
    for (auto& viewAndModel : d->allViews) {
        if (viewAndModel.view.isNull()) {
            continue;
        }

        viewAndModel.view->setEditingMode(_mode);
    }
}

void ComicBookTextManager::setAvailableCredits(int _credits)
{
    for (auto& viewAndModel : d->allViews) {
        if (viewAndModel.view.isNull()) {
            continue;
        }

        viewAndModel.view->setAvailableCredits(_credits);
    }
}

void ComicBookTextManager::setCurrentModelIndex(const QModelIndex& _index)
{
    QSignalBlocker blocker(this);

    if (!_index.isValid()) {
        return;
    }

    d->view->setCurrentModelIndex(_index);
    d->view->setFocus();
}

} // namespace ManagementLayer
