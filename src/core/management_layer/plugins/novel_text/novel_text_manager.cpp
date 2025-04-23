#include "novel_text_manager.h"

#include "novel_text_view.h"

#include <business_layer/model/novel/novel_dictionaries_model.h>
#include <business_layer/model/novel/text/novel_text_model.h>
#include <business_layer/model/novel/text/novel_text_model_folder_item.h>
#include <business_layer/model/novel/text/novel_text_model_scene_item.h>
#include <business_layer/model/text/text_model_text_item.h>
#include <business_layer/templates/text_template.h>
#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_object.h>
#include <ui/modules/bookmarks/bookmark_dialog.h>
#include <utils/logging.h>

#include <QApplication>
#include <QFileDialog>
#include <QStringListModel>


namespace ManagementLayer {

namespace {

const QLatin1String kSettingsKey("novel-text");
QString cursorPositionFor(Domain::DocumentObject* _item)
{
    return QString("%1/%2/last-cursor").arg(kSettingsKey, _item->uuid().toString());
}
QString verticalScrollFor(Domain::DocumentObject* _item)
{
    return QString("%1/%2/vertical-scroll").arg(kSettingsKey, _item->uuid().toString());
}

} // namespace

class NovelTextManager::Implementation
{
public:
    explicit Implementation(NovelTextManager* _q);

    /**
     * @brief Создать представление
     */
    Ui::NovelTextView* createView(BusinessLayer::AbstractModel* _model);

    /**
     * @brief Связать заданную модель и представление
     */
    void setModelForView(BusinessLayer::AbstractModel* _model, Ui::NovelTextView* _view);


    /**
     * @brief Получить модель связанную с заданным представлением
     */
    QPointer<BusinessLayer::NovelTextModel> modelForView(Ui::NovelTextView* _view) const;


    /**
     * @brief Работа с параметрами отображения представления
     */
    void loadModelAndViewSettings(BusinessLayer::AbstractModel* _model, Ui::NovelTextView* _view);
    void saveModelAndViewSettings(BusinessLayer::AbstractModel* _model, Ui::NovelTextView* _view);

    /**
     * @brief Обновить переводы
     */
    void updateTranslations();


    NovelTextManager* q = nullptr;

    /**
     * @brief Предаставление для основного окна
     */
    Ui::NovelTextView* view = nullptr;
    Ui::NovelTextView* secondaryView = nullptr;

    /**
     * @brief Все созданные представления с моделями, которые в них отображаются
     */
    struct ViewAndModel {
        QPointer<Ui::NovelTextView> view;
        QPointer<BusinessLayer::NovelTextModel> model;
    };
    QVector<ViewAndModel> allViews;
};

NovelTextManager::Implementation::Implementation(NovelTextManager* _q)
    : q(_q)
{
}

Ui::NovelTextView* NovelTextManager::Implementation::createView(
    BusinessLayer::AbstractModel* _model)
{
    Log::info("Create novel text view for model");
    auto view = new Ui::NovelTextView;
    view->installEventFilter(q);
    setModelForView(_model, view);

    connect(view, &Ui::NovelTextView::currentModelIndexChanged, q,
            &NovelTextManager::currentModelIndexChanged);
    //
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
    connect(view, &Ui::NovelTextView::addBookmarkRequested, q, [showBookmarkDialog] {
        showBookmarkDialog(Ui::BookmarkDialog::DialogType::CreateNew);
    });
    connect(view, &Ui::NovelTextView::editBookmarkRequested, q,
            [showBookmarkDialog] { showBookmarkDialog(Ui::BookmarkDialog::DialogType::Edit); });
    connect(view, &Ui::NovelTextView::createBookmarkRequested, q,
            [this, view](const QString& _text, const QColor& _color) {
                auto item = modelForView(view)->itemForIndex(view->currentModelIndex());
                if (item->type() != BusinessLayer::TextModelItemType::Text) {
                    return;
                }

                auto textItem = static_cast<BusinessLayer::TextModelTextItem*>(item);
                textItem->setBookmark({ _color, _text });
                modelForView(view)->updateItem(textItem);
            });
    connect(view, &Ui::NovelTextView::changeBookmarkRequested, q,
            [this, view](const QModelIndex& _index, const QString& _text, const QColor& _color) {
                auto item = modelForView(view)->itemForIndex(_index);
                if (item->type() != BusinessLayer::TextModelItemType::Text) {
                    return;
                }

                auto textItem = static_cast<BusinessLayer::TextModelTextItem*>(item);
                textItem->setBookmark({ _color, _text });
                modelForView(view)->updateItem(textItem);
            });
    connect(view, &Ui::NovelTextView::removeBookmarkRequested, q, [this, view] {
        auto item = modelForView(view)->itemForIndex(view->currentModelIndex());
        if (item->type() != BusinessLayer::TextModelItemType::Text) {
            return;
        }

        auto textItem = static_cast<BusinessLayer::TextModelTextItem*>(item);
        textItem->clearBookmark();
        modelForView(view)->updateItem(textItem);
    });
    //
    connect(view, &Ui::NovelTextView::rephraseTextRequested, q,
            &NovelTextManager::rephraseTextRequested);
    connect(view, &Ui::NovelTextView::expandTextRequested, q,
            &NovelTextManager::expandTextRequested);
    connect(view, &Ui::NovelTextView::shortenTextRequested, q,
            &NovelTextManager::shortenTextRequested);
    connect(view, &Ui::NovelTextView::insertTextRequested, q,
            &NovelTextManager::insertTextRequested);
    connect(view, &Ui::NovelTextView::summarizeTextRequested, q,
            &NovelTextManager::summarizeTextRequested);
    connect(view, &Ui::NovelTextView::translateTextRequested, q,
            &NovelTextManager::translateTextRequested);
    connect(view, &Ui::NovelTextView::translateDocumentRequested, q,
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
                                                   Domain::DocumentObjectType::NovelText);
            });
    connect(view, &Ui::NovelTextView::generateScriptRequested, q, [this, view] {
        const auto model = modelForView(view);
        QVector<QString> chapter;
        std::function<void(const QModelIndex&)> findChapters;
        findChapters = [&findChapters, model, &chapter](const QModelIndex& _parentItemIndex) {
            for (int row = 0; row < model->rowCount(_parentItemIndex); ++row) {
                const auto itemIndex = model->index(row, 0, _parentItemIndex);
                const auto item = model->itemForIndex(itemIndex);
                switch (item->type()) {
                case BusinessLayer::TextModelItemType::Folder: {
                    const auto folderItem
                        = static_cast<const BusinessLayer::NovelTextModelFolderItem*>(item);
                    //
                    // Если в папке много вложений, то обрабатываем каждое по отдельности
                    //
                    if (folderItem->charactersCount().second > 10000) {
                        findChapters(itemIndex);
                    }
                    //
                    // ... а если достаточно, чтобы нейронка переварила, берём текст папки целиком
                    //
                    else {
                        chapter.append(folderItem->text());
                    }
                    break;
                }

                case BusinessLayer::TextModelItemType::Group: {
                    if (item->subtype() == static_cast<int>(BusinessLayer::TextGroupType::Scene)) {
                        const auto sceneItem
                            = static_cast<const BusinessLayer::NovelTextModelSceneItem*>(item);
                        chapter.append(sceneItem->text());
                    }
                    break;
                }

                default: {
                    break;
                }
                }
            }
        };
        findChapters({});
        emit q->generateScriptRequested(chapter, model->wordsCount());
    });
    connect(view, &Ui::NovelTextView::generateTextRequested, q,
            [this](const QString& _text) { emit q->generateTextRequested({}, _text, {}); });
    connect(view, &Ui::NovelTextView::buyCreditsRequested, q,
            &NovelTextManager::buyCreditsRequested);


    updateTranslations();

    Log::info("Novel text view created");

    return view;
}

void NovelTextManager::Implementation::setModelForView(BusinessLayer::AbstractModel* _model,
                                                       Ui::NovelTextView* _view)
{
    Log::info("Set model for view");

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
        _view->disconnect(allViews[viewIndex].model->dictionariesModel());
        allViews[viewIndex].model->disconnect(_view);
        allViews[viewIndex].model->dictionariesModel()->disconnect(_view);
    }

    //
    // Определяем новую модель
    //
    auto model = qobject_cast<BusinessLayer::NovelTextModel*>(_model);
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
    }

    Log::info("Model for view set");
}

QPointer<BusinessLayer::NovelTextModel> NovelTextManager::Implementation::modelForView(
    Ui::NovelTextView* _view) const
{
    for (auto& viewAndModel : allViews) {
        if (viewAndModel.view == _view) {
            return viewAndModel.model;
        }
    }
    return {};
}

void NovelTextManager::Implementation::loadModelAndViewSettings(
    BusinessLayer::AbstractModel* _model, Ui::NovelTextView* _view)
{
    const auto cursorPosition = settingsValue(cursorPositionFor(_model->document()), 0).toInt();
    _view->setCursorPosition(cursorPosition);
    const auto verticalScroll = settingsValue(verticalScrollFor(_model->document()), 0).toInt();
    _view->setVerticalScroll(verticalScroll);

    _view->loadViewSettings();
}

void NovelTextManager::Implementation::saveModelAndViewSettings(
    BusinessLayer::AbstractModel* _model, Ui::NovelTextView* _view)
{
    setSettingsValue(cursorPositionFor(_model->document()), _view->cursorPosition());
    setSettingsValue(verticalScrollFor(_model->document()), _view->verticalScroll());

    _view->saveViewSettings();
}

void NovelTextManager::Implementation::updateTranslations()
{
}


// ****


NovelTextManager::NovelTextManager(QObject* _parent)
    : QObject(_parent)
    , d(new Implementation(this))
{
    Log::info("Init novel text manager");
}

NovelTextManager::~NovelTextManager() = default;

QObject* NovelTextManager::asQObject()
{
    return this;
}

Ui::IDocumentView* NovelTextManager::view()
{
    return d->view;
}

Ui::IDocumentView* NovelTextManager::view(BusinessLayer::AbstractModel* _model)
{
    if (d->view == nullptr) {
        d->view = d->createView(_model);

        connect(d->view, &Ui::NovelTextView::currentModelIndexChanged, this,
                &NovelTextManager::viewCurrentModelIndexChanged);
    } else {
        d->setModelForView(_model, d->view);
    }

    return d->view;
}

Ui::IDocumentView* NovelTextManager::secondaryView()
{
    return d->secondaryView;
}

Ui::IDocumentView* NovelTextManager::secondaryView(BusinessLayer::AbstractModel* _model)
{
    if (d->secondaryView == nullptr) {
        d->secondaryView = d->createView(_model);
    } else {
        d->setModelForView(_model, d->secondaryView);
    }

    return d->secondaryView;
}

Ui::IDocumentView* NovelTextManager::createView(BusinessLayer::AbstractModel* _model)
{
    return d->createView(_model);
}

void NovelTextManager::resetModels()
{
    for (auto& viewAndModel : d->allViews) {
        if (viewAndModel.view.isNull()) {
            continue;
        }

        d->setModelForView(nullptr, viewAndModel.view);
    }
}

void NovelTextManager::reconfigure(const QStringList& _changedSettingsKeys)
{
    for (const auto& view : std::as_const(d->allViews)) {
        if (!view.view.isNull()) {
            view.view->reconfigure(_changedSettingsKeys);
        }
    }
}

void NovelTextManager::bind(IDocumentManager* _manager)
{
    Q_ASSERT(_manager);
    if (_manager == nullptr || _manager == this) {
        return;
    }

    //
    // Т.к. навигатор соединяется только с главным инстансом редактора, проверяем создан ли он
    //
    if (_manager->isNavigationManager()) {
        const auto isConnectedFirstTime
            = connect(_manager->asQObject(), SIGNAL(currentModelIndexChanged(QModelIndex)), this,
                      SLOT(setViewCurrentModelIndex(QModelIndex)), Qt::UniqueConnection);

        //
        // Ставим в очередь событие нотификацию о смене текущей сцены,
        // чтобы навигатор отобразил её при первом открытии
        //
        if (isConnectedFirstTime && d->view != nullptr) {
            QMetaObject::invokeMethod(
                this, [this] { emit viewCurrentModelIndexChanged(d->view->currentModelIndex()); },
                Qt::QueuedConnection);
        }
    }
    //
    // Между собой можно соединить любые менеджеры редакторов
    //
    else {
        connect(_manager->asQObject(), SIGNAL(currentModelIndexChanged(QModelIndex)), this,
                SLOT(setCurrentModelIndex(QModelIndex)), Qt::UniqueConnection);
    }
}

void NovelTextManager::saveSettings()
{
    for (auto& viewAndModel : d->allViews) {
        if (viewAndModel.model.isNull() || viewAndModel.view.isNull()) {
            continue;
        }

        d->saveModelAndViewSettings(viewAndModel.model, viewAndModel.view);
    }
}

void NovelTextManager::setEditingMode(DocumentEditingMode _mode)
{
    for (auto& viewAndModel : d->allViews) {
        if (viewAndModel.view.isNull()) {
            continue;
        }

        viewAndModel.view->setEditingMode(_mode);
    }
}

void NovelTextManager::setAvailableCredits(int _credits)
{
    for (auto& viewAndModel : d->allViews) {
        if (viewAndModel.view.isNull()) {
            continue;
        }

        viewAndModel.view->setAvailableCredits(_credits);
    }
}

bool NovelTextManager::eventFilter(QObject* _watched, QEvent* _event)
{
    if (_event->type() == QEvent::LanguageChange && _watched == d->view) {
        d->updateTranslations();
    }

    return QObject::eventFilter(_watched, _event);
}

void NovelTextManager::setViewCurrentModelIndex(const QModelIndex& _index)
{
    QSignalBlocker blocker(this);

    if (!_index.isValid() || d->view == nullptr) {
        return;
    }

    d->view->setCurrentModelIndex(_index);
    d->view->setFocus();
}

void NovelTextManager::setCurrentModelIndex(const QModelIndex& _index)
{
    QSignalBlocker blocker(this);

    if (!_index.isValid()) {
        return;
    }

    for (const auto& viewAndModel : std::as_const(d->allViews)) {
        if (!viewAndModel.view.isNull()) {
            viewAndModel.view->setCurrentModelIndex(_index);
        }
    }
}

} // namespace ManagementLayer
