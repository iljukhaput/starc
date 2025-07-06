#include "audioplay_scene_report.h"

#include <3rd_party/qtxlsxwriter/xlsxdocument.h>
#include <3rd_party/qtxlsxwriter/xlsxrichstring.h>
#include <business_layer/document/audioplay/text/audioplay_text_document.h>
#include <business_layer/model/audioplay/audioplay_information_model.h>
#include <business_layer/model/audioplay/text/audioplay_text_block_parser.h>
#include <business_layer/model/audioplay/text/audioplay_text_model.h>
#include <business_layer/model/audioplay/text/audioplay_text_model_scene_item.h>
#include <business_layer/model/audioplay/text/audioplay_text_model_text_item.h>
#include <business_layer/templates/audioplay_template.h>
#include <business_layer/templates/templates_facade.h>
#include <ui/design_system/design_system.h>
#include <ui/widgets/text_edit/page/page_text_edit.h>
#include <utils/helpers/color_helper.h>
#include <utils/helpers/text_helper.h>
#include <utils/helpers/time_helper.h>

#include <QCoreApplication>
#include <QPdfWriter>
#include <QPointer>
#include <QRegularExpression>
#include <QStandardItemModel>

#include <set>

namespace BusinessLayer {

class AudioplaySceneReport::Implementation
{
public:
    /**
     * @brief Модель аудиопостановки
     */
    QPointer<AudioplayTextModel> audioplayModel;

    /**
     * @brief Модель сцен
     */
    QScopedPointer<QStandardItemModel> sceneModel;

    /**
     * @brief Необходимо ли отображать персонажей
     */
    bool showCharacters = true;

    /**
     * @brief Тип сортировки
     */
    int sortBy = 0;
};


// ****


AudioplaySceneReport::AudioplaySceneReport()
    : d(new Implementation)
{
}

AudioplaySceneReport::~AudioplaySceneReport() = default;

bool AudioplaySceneReport::isValid() const
{
    return d->sceneModel->rowCount() > 0;
}

void AudioplaySceneReport::build(QAbstractItemModel* _model)
{
    if (_model == nullptr) {
        return;
    }

    d->audioplayModel = qobject_cast<AudioplayTextModel*>(_model);
    if (d->audioplayModel == nullptr) {
        return;
    }

    //
    // Подготовим модель к наполнению
    //
    if (d->sceneModel.isNull()) {
        d->sceneModel.reset(new QStandardItemModel);
    } else {
        d->sceneModel->clear();
    }

    //
    // Подготовим необходимые структуры для сбора статистики
    //
    struct CharacterData {
        QString name;
        bool isFirstAppearance = false;
        int totalDialogues = 0;
    };
    const int invalidPage = 0;
    struct SceneData {
        QString name;
        int page = invalidPage;
        QString number;
        std::chrono::milliseconds duration;
        QVector<CharacterData> characters;

        CharacterData& character(const QString& _name)
        {
            for (int index = 0; index < characters.size(); ++index) {
                if (characters[index].name == _name) {
                    return characters[index];
                }
            }

            characters.append({ _name });
            return characters.last();
        }
    };
    QVector<SceneData> scenes;
    SceneData lastScene;
    QSet<QString> characters;

    //
    // Сформируем регулярное выражение для выуживания молчаливых персонажей
    //
    QString rxPattern;
    auto charactersModel = d->audioplayModel->charactersList();
    for (int index = 0; index < charactersModel->rowCount(); ++index) {
        auto characterName = charactersModel->index(index, 0).data().toString();
        if (!rxPattern.isEmpty()) {
            rxPattern.append("|");
        }
        rxPattern.append(TextHelper::toRxEscaped(characterName));
    }
    if (!rxPattern.isEmpty()) {
        rxPattern.prepend("(^|\\W)(");
        rxPattern.append(")($|\\W)");
    }
    const QRegularExpression rxCharacterFinder(
        rxPattern,
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);

    //
    // Подготовим текстовый документ, для определения страниц сцен
    //
    const auto& audioplayTemplate
        = TemplatesFacade::audioplayTemplate(d->audioplayModel->informationModel()->templateId());
    PageTextEdit audioplayTextEdit;
    audioplayTextEdit.setUsePageMode(true);
    audioplayTextEdit.setPageSpacing(0);
    audioplayTextEdit.setPageFormat(audioplayTemplate.pageSizeId());
    audioplayTextEdit.setPageMarginsMm(audioplayTemplate.pageMargins());
    AudioplayTextDocument audioplayDocument;
    audioplayTextEdit.setDocument(&audioplayDocument);
    const bool kCanChangeModel = false;
    audioplayDocument.setModel(d->audioplayModel, kCanChangeModel);
    QTextCursor audioplayCursor(&audioplayDocument);
    auto textItemPage = [&audioplayTextEdit, &audioplayDocument,
                         &audioplayCursor](TextModelTextItem* _item) {
        audioplayCursor.setPosition(
            audioplayDocument.itemPosition(audioplayDocument.model()->indexForItem(_item), true));
        return audioplayTextEdit.cursorPage(audioplayCursor);
    };

    //
    // Собираем статистику
    //
    std::function<void(const TextModelItem*)> includeInReport;
    includeInReport = [&includeInReport, &scenes, &lastScene, &characters, &rxCharacterFinder,
                       textItemPage, invalidPage](const TextModelItem* _item) {
        for (int childIndex = 0; childIndex < _item->childCount(); ++childIndex) {
            auto childItem = _item->childAt(childIndex);
            switch (childItem->type()) {
            case TextModelItemType::Folder:
            case TextModelItemType::Group: {
                includeInReport(childItem);
                break;
            }

            case TextModelItemType::Text: {
                auto textItem = static_cast<AudioplayTextModelTextItem*>(childItem);
                //
                // ... стата по объектам
                //
                switch (textItem->paragraphType()) {
                case TextParagraphType::SceneHeading: {
                    //
                    // Началась новая сцена
                    //
                    if (lastScene.page != invalidPage) {
                        scenes.append(lastScene);
                        lastScene = SceneData();
                    }

                    const auto sceneItem
                        = static_cast<AudioplayTextModelSceneItem*>(textItem->parent());
                    lastScene.name = sceneItem->heading();
                    lastScene.number = sceneItem->number()->text;
                    lastScene.duration = sceneItem->duration();
                    lastScene.page = textItemPage(textItem);
                    break;
                }

                case TextParagraphType::Character: {
                    if (!textItem->isCorrection()) {
                        const auto character = AudioplayCharacterParser::name(textItem->text());
                        auto& characterData = lastScene.character(character);
                        ++characterData.totalDialogues;
                        if (!characters.contains(character)) {
                            characters.insert(character);
                            characterData.isFirstAppearance = true;
                        }
                    }
                    break;
                }

                case TextParagraphType::Action: {
                    if (rxCharacterFinder.pattern().isEmpty()) {
                        break;
                    }

                    auto match = rxCharacterFinder.match(textItem->text());
                    while (match.hasMatch()) {
                        const QString character = TextHelper::smartToUpper(match.captured(2));
                        auto& characterData = lastScene.character(character);
                        if (!characters.contains(character)) {
                            characters.insert(character);
                            characterData.isFirstAppearance = true;
                        }

                        //
                        // Ищем дальше
                        //
                        match = rxCharacterFinder.match(textItem->text(), match.capturedEnd());
                    }
                    break;
                }

                default:
                    break;
                }

                break;
            }

            default:
                break;
            }
        }
    };
    includeInReport(d->audioplayModel->itemForIndex({}));
    if (lastScene.page != invalidPage) {
        scenes.append(lastScene);
    }

    //
    // Прерываем выполнение, если в сценарии нет сцен
    //
    if (scenes.isEmpty() || (scenes.size() == 1 && scenes.constFirst().name.isEmpty())) {
        return;
    }

    //
    // Сортируем
    //
    switch (d->sortBy) {
    default:
    case 0: {
        break;
    }

    case 1: {
        std::sort(scenes.begin(), scenes.end(), [](const SceneData& _lhs, const SceneData& _rhs) {
            return _lhs.name < _rhs.name;
        });
        break;
    }

    case 2: {
        std::sort(scenes.begin(), scenes.end(), [](const SceneData& _lhs, const SceneData& _rhs) {
            return _lhs.duration > _rhs.duration;
        });
        break;
    }

    case 3: {
        std::sort(scenes.begin(), scenes.end(), [](const SceneData& _lhs, const SceneData& _rhs) {
            return _lhs.duration < _rhs.duration;
        });
        break;
    }

    case 4: {
        std::sort(scenes.begin(), scenes.end(), [](const SceneData& _lhs, const SceneData& _rhs) {
            return _lhs.characters.size() > _rhs.characters.size();
        });
        break;
    }

    case 5: {
        std::sort(scenes.begin(), scenes.end(), [](const SceneData& _lhs, const SceneData& _rhs) {
            return _lhs.characters.size() < _rhs.characters.size();
        });
        break;
    }
    }

    //
    // Формируем отчёт
    //
    auto createModelItem = [](const QString& _text, const QVariant& _backgroundColor = {}) {
        auto item = new QStandardItem(_text);
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        if (_backgroundColor.isValid()) {
            item->setData(_backgroundColor, Qt::BackgroundRole);
        }
        return item;
    };
    //
    // ... наполняем таблицу
    //
    const auto titleBackgroundColor = d->showCharacters
        ? QVariant::fromValue(ColorHelper::transparent(Ui::DesignSystem::color().onBackground(),
                                                       Ui::DesignSystem::elevationEndOpacity()))
        : QVariant();
    for (const auto& scene : scenes) {
        auto sceneItem = createModelItem(scene.name, titleBackgroundColor);
        if (d->showCharacters) {
            for (const auto& character : scene.characters) {
                auto characterItem = createModelItem(QString("%1 (%2)").arg(
                    character.name, QString::number(character.totalDialogues)));
                if (character.isFirstAppearance) {
                    characterItem->setData(u8"\U000F09DE", Qt::DecorationRole);
                    characterItem->setData(Ui::DesignSystem::color().accent(),
                                           Qt::DecorationPropertyRole);
                }
                sceneItem->appendRow({ characterItem, createModelItem({}), createModelItem({}),
                                       createModelItem({}), createModelItem({}) });
            }
        }

        d->sceneModel->appendRow({
            sceneItem,
            createModelItem(scene.number, titleBackgroundColor),
            createModelItem(QString::number(scene.page), titleBackgroundColor),
            createModelItem(QString::number(scene.characters.size()), titleBackgroundColor),
            createModelItem(TimeHelper::toString(scene.duration), titleBackgroundColor),
        });
    }
    //
    d->sceneModel->setHeaderData(
        0, Qt::Horizontal,
        QCoreApplication::translate("BusinessLayer::AudioplaySceneReport", "Scene/characters"),
        Qt::DisplayRole);
    d->sceneModel->setHeaderData(
        1, Qt::Horizontal,
        QCoreApplication::translate("BusinessLayer::AudioplaySceneReport", "Number"),
        Qt::DisplayRole);
    d->sceneModel->setHeaderData(
        2, Qt::Horizontal,
        QCoreApplication::translate("BusinessLayer::AudioplaySceneReport", "Page"),
        Qt::DisplayRole);
    d->sceneModel->setHeaderData(
        3, Qt::Horizontal,
        QCoreApplication::translate("BusinessLayer::AudioplaySceneReport", "Characters"),
        Qt::DisplayRole);
    d->sceneModel->setHeaderData(
        4, Qt::Horizontal,
        QCoreApplication::translate("BusinessLayer::AudioplaySceneReport", "Duration"),
        Qt::DisplayRole);
}

void AudioplaySceneReport::setParameters(bool _showCharacters, int _sortBy)
{
    d->showCharacters = _showCharacters;
    d->sortBy = _sortBy;
}

QAbstractItemModel* AudioplaySceneReport::sceneModel() const
{
    return d->sceneModel.data();
}

void AudioplaySceneReport::saveToPdf(const QString& _fileName) const
{
    const auto& exportTemplate
        = TemplatesFacade::audioplayTemplate(d->audioplayModel->informationModel()->templateId());

    //
    // Настраиваем документ
    //
    PageTextEdit textEdit;
    textEdit.setUsePageMode(true);
    textEdit.setPageSpacing(0);
    QTextDocument report;
    report.setDefaultFont(exportTemplate.baseFont());
    textEdit.setDocument(&report);
    //
    // ... параметры страницы
    //
    textEdit.setPageFormat(exportTemplate.pageSizeId());
    textEdit.setPageMarginsMm(exportTemplate.pageMargins());
    textEdit.setPageNumbersAlignment(exportTemplate.pageNumbersAlignment());

    //
    // Формируем отчёт
    //
    QTextCursor cursor(&report);
    QTextCharFormat titleFormat;
    auto titleFont = report.defaultFont();
    titleFont.setBold(true);
    titleFormat.setFont(titleFont);
    cursor.setCharFormat(titleFormat);
    cursor.insertText(QString("%1 - %2").arg(
        d->audioplayModel->informationModel()->name(),
        QCoreApplication::translate("BusinessLayer::AudioplaySceneReport", "Scene report")));
    cursor.insertBlock();
    cursor.insertBlock();
    QTextTableFormat tableFormat;
    tableFormat.setBorder(0);
    tableFormat.setBorderStyle(QTextFrameFormat::BorderStyle_None);
    tableFormat.setColumnWidthConstraints({
        QTextLength{ QTextLength::PercentageLength, 60 },
        QTextLength{ QTextLength::PercentageLength, 10 },
        QTextLength{ QTextLength::PercentageLength, 10 },
        QTextLength{ QTextLength::PercentageLength, 10 },
        QTextLength{ QTextLength::PercentageLength, 10 },
    });
    const auto beforeTablePosition = cursor.position();
    cursor.insertTable(sceneModel()->rowCount() + 1, sceneModel()->columnCount(), tableFormat);
    cursor.setPosition(beforeTablePosition);
    cursor.movePosition(QTextCursor::NextBlock);
    cursor.beginEditBlock();
    //
    for (int column = 0; column < sceneModel()->columnCount(); ++column) {
        QTextTableCellFormat cellFormat;
        cellFormat.setBottomBorder(1);
        cellFormat.setVerticalAlignment(QTextCharFormat::AlignBottom);
        cellFormat.setBottomBorderStyle(QTextFrameFormat::BorderStyle_Solid);
        cellFormat.setBottomBorderBrush(Qt::black);
        cursor.mergeBlockCharFormat(cellFormat);
        QTextBlockFormat blockFormat = cursor.blockFormat();
        blockFormat.setAlignment(column == 0 ? Qt::AlignLeft : Qt::AlignRight);
        cursor.setBlockFormat(blockFormat);
        cursor.insertText(sceneModel()->headerData(column, Qt::Horizontal).toString());

        cursor.movePosition(QTextCursor::NextBlock);
    }
    for (int row = 0; row < sceneModel()->rowCount(); ++row) {
        const auto sceneIndex = sceneModel()->index(row, 0);
        for (int column = 0; column < sceneModel()->columnCount(); ++column) {
            const auto hasCharacters = sceneModel()->rowCount(sceneIndex) > 0;

            QTextBlockFormat blockFormat = cursor.blockFormat();
            blockFormat.setAlignment(column == 0 ? Qt::AlignLeft : Qt::AlignRight);
            cursor.setBlockFormat(blockFormat);
            QTextCharFormat textFormat = cursor.blockCharFormat();
            textFormat.setFontWeight(hasCharacters ? QFont::Weight::Bold : QFont::Weight::Normal);
            cursor.insertText(sceneModel()->index(row, column).data().toString(), textFormat);

            //
            // Для первой колонки добавляем список персонажей
            //
            if (column == 0 && d->showCharacters) {
                cursor.insertText({ QChar::LineSeparator });
                for (int childRow = 0; childRow < sceneModel()->rowCount(sceneIndex); ++childRow) {
                    const auto childIndex = sceneModel()->index(childRow, 0, sceneIndex);
                    textFormat = cursor.blockCharFormat();
                    textFormat.setFontUnderline(!childIndex.data(Qt::DecorationRole).isNull());
                    //
                    // ... убираем пробел между именем персонажа и открывающей скобкой, чтобы в
                    //     отчёте она не переносилась на новую строку отдельно от имени
                    //
                    auto character = childIndex.data().toString();
                    character = character.replace(" (", "(");
                    cursor.insertText(character, textFormat);
                    cursor.insertText(", ", cursor.blockCharFormat());
                }
                if (hasCharacters) {
                    cursor.insertText({ QChar::LineSeparator });
                }
            }

            cursor.movePosition(QTextCursor::NextBlock);
        }
    }
    cursor.endEditBlock();

    //
    // Печатаем
    //
    QPdfWriter printer(_fileName);
    printer.setPageSize(QPageSize(exportTemplate.pageSizeId()));
    printer.setPageMargins({});
    report.print(&printer);
}

void AudioplaySceneReport::saveToXlsx(const QString& _fileName) const
{
    QXlsx::Document xlsx;
    QXlsx::Format headerFormat;
    headerFormat.setFontBold(true);
    QXlsx::Format textHeaderFormat;
    textHeaderFormat.setFillPattern(QXlsx::Format::PatternLightUp);
    QXlsx::Format underlineFormat;
    underlineFormat.setFontUnderline(QXlsx::Format::FontUnderlineSingle);

    constexpr int firstRow = 1;
    constexpr int firstColumn = 1;
    int reportRow = firstRow;
    auto writeHeader = [&xlsx, &headerFormat, &reportRow](int _column, const QVariant& _text) {
        xlsx.write(reportRow, _column, _text, headerFormat);
    };
    auto writeTextHeader
        = [&xlsx, &reportRow, &textHeaderFormat](int _column, const QVariant& _text) {
              xlsx.write(reportRow, _column, _text, textHeaderFormat);
          };

    for (int column = firstColumn; column < firstColumn + sceneModel()->columnCount(); ++column) {
        writeHeader(column, sceneModel()->headerData(column - firstColumn, Qt::Horizontal));
    }
    for (int row = 0; row < sceneModel()->rowCount(); ++row) {
        ++reportRow;
        for (int column = firstColumn; column < firstColumn + sceneModel()->columnCount();
             ++column) {
            writeTextHeader(column, sceneModel()->index(row, column - firstColumn).data());
        }

        ++reportRow;
        QXlsx::RichString rich;
        const auto itemIndex = sceneModel()->index(row, 0);
        for (int childRow = 0; childRow < sceneModel()->rowCount(itemIndex); ++childRow) {
            if (childRow > 0) {
                rich.addFragment(", ", {});
            }
            const auto childIndex = sceneModel()->index(childRow, 0, itemIndex);
            if (!childIndex.data(Qt::DecorationRole).isNull()) {
                rich.addFragment(childIndex.data().toString(), underlineFormat);
            } else {
                rich.addFragment(childIndex.data().toString(), {});
            }
        }
        xlsx.write(reportRow, firstColumn, rich);
    }

    xlsx.saveAs(_fileName);
}

} // namespace BusinessLayer
