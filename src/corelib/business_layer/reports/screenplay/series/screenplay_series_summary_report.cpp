#include "screenplay_series_summary_report.h"

#include <3rd_party/qtxlsxwriter/xlsxdocument.h>
#include <business_layer/document/screenplay/text/screenplay_text_document.h>
#include <business_layer/model/screenplay/screenplay_information_model.h>
#include <business_layer/model/screenplay/series/screenplay_series_episodes_model.h>
#include <business_layer/model/screenplay/series/screenplay_series_information_model.h>
#include <business_layer/model/screenplay/text/screenplay_text_block_parser.h>
#include <business_layer/model/screenplay/text/screenplay_text_model.h>
#include <business_layer/model/screenplay/text/screenplay_text_model_text_item.h>
#include <business_layer/templates/screenplay_template.h>
#include <business_layer/templates/templates_facade.h>
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

class ScreenplaySeriesSummaryReport::Implementation
{
public:
    /**
     * @brief Модель эпизодов
     */
    QPointer<ScreenplaySeriesEpisodesModel> episodesModel;

    std::chrono::milliseconds duration;
    int pagesCount = 0;
    int wordsCount = 0;
    CharactersCount charactersCount;

    QScopedPointer<QStandardItemModel> textInfoModel;
    QScopedPointer<QStandardItemModel> scenesInfoModel;
    QScopedPointer<QStandardItemModel> locationsInfoModel;
    QScopedPointer<QStandardItemModel> charactersInfoModel;
};


// ****


ScreenplaySeriesSummaryReport::ScreenplaySeriesSummaryReport()
    : d(new Implementation)
{
}

ScreenplaySeriesSummaryReport::~ScreenplaySeriesSummaryReport() = default;

bool ScreenplaySeriesSummaryReport::isValid() const
{
    return d->wordsCount > 0;
}

void ScreenplaySeriesSummaryReport::build(QAbstractItemModel* _model)
{
    if (_model == nullptr) {
        return;
    }

    d->episodesModel = qobject_cast<ScreenplaySeriesEpisodesModel*>(_model);
    if (d->episodesModel == nullptr) {
        return;
    }

    //
    // Подготовим необходимые структуры для сбора статистики
    //
    struct Counters {
        int occurrences = 0;
        int words = 0;
    };
    QHash<TextParagraphType, Counters> paragraphsToCounters;
    const QVector<TextParagraphType> paragraphTypes
        = { TextParagraphType::SceneHeading,  TextParagraphType::SceneCharacters,
            TextParagraphType::Action,        TextParagraphType::Character,
            TextParagraphType::Parenthetical, TextParagraphType::Dialogue,
            TextParagraphType::Lyrics,        TextParagraphType::Transition,
            TextParagraphType::Shot };
    for (const auto type : paragraphTypes) {
        paragraphsToCounters.insert(type, {});
    }
    int totalWords = 0;
    CharactersCount totalCharacters;
    // - список сцен
    QVector<QString> scenes;
    // - персонаж - кол-во реплик
    QHash<QString, int> charactersToDialogues;

    //
    // Сформируем регулярное выражение для выуживания молчаливых персонажей
    //
    QString rxPattern;
    QSet<QString> characterNames;
    for (const auto episode : d->episodesModel->episodes()) {
        auto charactersModel = episode->charactersList();
        for (int index = 0; index < charactersModel->rowCount(); ++index) {
            characterNames.insert(charactersModel->index(index, 0).data().toString());
        }
    }
    for (const auto& characterName : std::as_const(characterNames)) {
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
    // Собираем статистику
    //
    std::function<void(const TextModelItem*)> includeInReport;
    includeInReport = [&includeInReport, &paragraphsToCounters, &totalWords, &totalCharacters,
                       &scenes, &charactersToDialogues,
                       &rxCharacterFinder](const TextModelItem* _item) {
        for (int childIndex = 0; childIndex < _item->childCount(); ++childIndex) {
            auto childItem = _item->childAt(childIndex);
            switch (childItem->type()) {
            case TextModelItemType::Folder:
            case TextModelItemType::Group: {
                includeInReport(childItem);
                break;
            }

            case TextModelItemType::Text: {
                auto textItem = static_cast<ScreenplayTextModelTextItem*>(childItem);
                //
                // ... счётчики
                //
                if (paragraphsToCounters.contains(textItem->paragraphType())) {
                    auto& paragraphCounters = paragraphsToCounters[textItem->paragraphType()];
                    ++paragraphCounters.occurrences;
                    const auto paragraphWords = TextHelper::wordsCount(textItem->text());
                    paragraphCounters.words += paragraphWords;
                    totalWords += paragraphWords;
                    totalCharacters.withSpaces += textItem->text().length();
                    totalCharacters.withoutSpaces
                        += textItem->text().length() - textItem->text().count(' ');
                }

                //
                // ... стата по объектам
                //
                switch (textItem->paragraphType()) {
                case TextParagraphType::SceneHeading: {
                    scenes.append(TextHelper::smartToUpper(textItem->text()));
                    break;
                }

                case TextParagraphType::SceneCharacters: {
                    const auto sceneCharacters
                        = ScreenplaySceneCharactersParser::characters(textItem->text());
                    for (const auto& character : sceneCharacters) {
                        if (!charactersToDialogues.contains(character)) {
                            charactersToDialogues.insert(character, 0);
                        }
                    }
                    break;
                }

                case TextParagraphType::Character: {
                    const auto character = ScreenplayCharacterParser::name(textItem->text());
                    if (!charactersToDialogues.contains(character)) {
                        charactersToDialogues.insert(character, 1);
                    } else {
                        ++charactersToDialogues[character];
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
                        if (!charactersToDialogues.contains(character)) {
                            charactersToDialogues.insert(character, 0);
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
    for (const auto episode : d->episodesModel->episodes()) {
        includeInReport(episode->itemForIndex({}));
    }

    //
    // Формируем отчёт
    //
    auto createModelItem = [](const QString& _text) {
        auto item = new QStandardItem(_text);
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        return item;
    };
    auto createPercentModelItem = [&createModelItem](qreal _percent) {
        auto item = createModelItem(QString::number(_percent, 'g', 3) + "%");
        item->setData(_percent);
        return item;
    };
    //
    // ... сводка
    //
    {
        d->duration = {};
        d->pagesCount = {};
        for (const auto episode : d->episodesModel->episodes()) {
            d->duration += episode->duration();
            d->pagesCount += [episode] {
                const auto& screenplayTemplate = TemplatesFacade::screenplayTemplate(
                    episode->informationModel()->templateId());

                PageTextEdit textEdit;
                textEdit.setUsePageMode(true);
                textEdit.setPageSpacing(0);
                textEdit.setPageFormat(screenplayTemplate.pageSizeId());
                textEdit.setPageMarginsMm(screenplayTemplate.pageMargins());
                ScreenplayTextDocument screenplayDocument;
                textEdit.setDocument(&screenplayDocument);

                const bool kCanChangeModel = false;
                screenplayDocument.setModel(episode, kCanChangeModel);

                return screenplayDocument.pageCount();
            }();
        }
        d->wordsCount = totalWords;
        d->charactersCount = totalCharacters;
        //
        // ... иформация по тексту
        //
        if (d->textInfoModel.isNull()) {
            d->textInfoModel.reset(new QStandardItemModel);
        } else {
            d->textInfoModel->clear();
        }
        //
        for (int index = 0; index < paragraphTypes.size(); ++index) {
            const auto& paragraphType = paragraphTypes.at(index);
            const auto& paragraphCounters = paragraphsToCounters[paragraphType];
            if (paragraphCounters.occurrences == 0) {
                continue;
            }

            auto paragraphItem = createModelItem(toDisplayString(paragraphType));
            paragraphItem->setData(u8"\U000F0766", Qt::DecorationRole);
            paragraphItem->setData(ColorHelper::forNumber(index), Qt::DecorationPropertyRole);

            d->textInfoModel->appendRow(
                { paragraphItem, createModelItem(QString::number(paragraphCounters.words)),
                  createModelItem(QString::number(paragraphCounters.occurrences)),
                  createPercentModelItem(paragraphCounters.words * 100.0 / totalWords) });
        }
        //
        d->textInfoModel->setHeaderData(
            0, Qt::Horizontal,
            QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Paragraph"),
            Qt::DisplayRole);
        d->textInfoModel->setHeaderData(
            1, Qt::Horizontal,
            QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Words"),
            Qt::DisplayRole);
        d->textInfoModel->setHeaderData(
            2, Qt::Horizontal,
            QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Occurrences"),
            Qt::DisplayRole);
        d->textInfoModel->setHeaderData(
            3, Qt::Horizontal,
            QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Percents"),
            Qt::DisplayRole);
    }
    //
    // ... сцены
    //
    {
        QHash<QString, int> sceneTimesToCount;
        for (const auto& scene : scenes) {
            const QString time = ScreenplaySceneHeadingParser::sceneTime(scene);
            if (!sceneTimesToCount.contains(time)) {
                sceneTimesToCount.insert(time, 0);
            }
            sceneTimesToCount[time] += 1;
        }

        //
        // Сортируем по наибольшему вхождению и удаляем дубликаты
        //
        const std::set<int, std::greater<int>> counts(sceneTimesToCount.begin(),
                                                      sceneTimesToCount.end());

        //
        // Формируем таблицу
        //
        if (d->scenesInfoModel.isNull()) {
            d->scenesInfoModel.reset(new QStandardItemModel);
        } else {
            d->scenesInfoModel->clear();
        }
        //
        const auto totalCount
            = std::accumulate(sceneTimesToCount.begin(), sceneTimesToCount.end(), 0);
        int index = 0;
        for (const auto count : counts) {
            auto sceneTimes = sceneTimesToCount.keys(count);
            std::sort(sceneTimes.begin(), sceneTimes.end());
            for (const auto& time : sceneTimes) {
                auto timeName
                    = createModelItem(time.isEmpty() ? QCoreApplication::translate(
                                          "BusinessLogic::ScreenplaySummaryReport", "[UNDEFINED]")
                                                     : time);
                timeName->setData(u8"\U000F0766", Qt::DecorationRole);
                timeName->setData(ColorHelper::forNumber(index++), Qt::DecorationPropertyRole);

                d->scenesInfoModel->appendRow(
                    { timeName, createModelItem(QString::number(count)),
                      createPercentModelItem(count * 100.0 / totalCount) });
            }
        }
        //
        d->scenesInfoModel->setHeaderData(
            0, Qt::Horizontal,
            QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Scene time"),
            Qt::DisplayRole);
        d->scenesInfoModel->setHeaderData(
            1, Qt::Horizontal,
            QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Occurrences"),
            Qt::DisplayRole);
        d->scenesInfoModel->setHeaderData(
            2, Qt::Horizontal,
            QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Percents"),
            Qt::DisplayRole);
    }
    //
    // ... локации
    //
    {
        QMap<QString, int> locationPlacesToCount;
        for (const auto& scene : scenes) {
            const auto place = ScreenplaySceneHeadingParser::sceneIntro(scene);
            if (!locationPlacesToCount.contains(place)) {
                locationPlacesToCount.insert(place, 0);
            }
            locationPlacesToCount[place] += 1;
        }

        //
        // Сортируем по наибольшему вхождению и удаляем дубликаты
        //
        const std::set<int, std::greater<int>> counts(locationPlacesToCount.begin(),
                                                      locationPlacesToCount.end());

        //
        // Формируем таблицу
        //
        if (d->locationsInfoModel.isNull()) {
            d->locationsInfoModel.reset(new QStandardItemModel);
        } else {
            d->locationsInfoModel->clear();
        }
        //
        const auto totalCount
            = std::accumulate(locationPlacesToCount.begin(), locationPlacesToCount.end(), 0);
        int index = 0;
        for (const auto count : counts) {
            auto locationPlaces = locationPlacesToCount.keys(count);
            std::sort(locationPlaces.begin(), locationPlaces.end());
            for (const auto& place : locationPlaces) {
                auto placeName
                    = createModelItem(place.isEmpty() ? QCoreApplication::translate(
                                          "BusinessLogic::ScreenplaySummaryReport", "[UNDEFINED]")
                                                      : place);
                placeName->setData(u8"\U000F0766", Qt::DecorationRole);
                placeName->setData(ColorHelper::forNumber(index++), Qt::DecorationPropertyRole);

                d->locationsInfoModel->appendRow(
                    { placeName, createModelItem(QString::number(count)),
                      createPercentModelItem(count * 100.0 / totalCount) });
            }
        }
        //
        d->locationsInfoModel->setHeaderData(
            0, Qt::Horizontal,
            QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Scene intro"),
            Qt::DisplayRole);
        d->locationsInfoModel->setHeaderData(
            1, Qt::Horizontal,
            QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Occurrences"),
            Qt::DisplayRole);
        d->locationsInfoModel->setHeaderData(
            2, Qt::Horizontal,
            QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Percents"),
            Qt::DisplayRole);
    }
    //
    // ... персонажи
    //
    {
        //
        // Группируем персонажей
        //
        int nonspeaking = 0, speakAbout10 = 0, speakMore10 = 0;
        for (const auto& characterDialogues : std::as_const(charactersToDialogues)) {
            if (characterDialogues == 0) {
                nonspeaking += 1;
            } else if (characterDialogues <= 10) {
                speakAbout10 += 1;
            } else {
                speakMore10 += 1;
            }
        }

        //
        // Формируем таблицу
        //
        if (d->charactersInfoModel.isNull()) {
            d->charactersInfoModel.reset(new QStandardItemModel);
        } else {
            d->charactersInfoModel->clear();
        }
        //
        const auto totalCount = nonspeaking + speakAbout10 + speakMore10;
        int index = 0;
        auto addCharacterItemToReport = [this, &createModelItem, &createPercentModelItem,
                                         &totalCount, &index](const QString& _name, int _count) {
            auto characterItem = createModelItem(_name);
            characterItem->setData(u8"\U000F0766", Qt::DecorationRole);
            characterItem->setData(ColorHelper::forNumber(index++), Qt::DecorationPropertyRole);

            d->charactersInfoModel->appendRow(
                { characterItem, createModelItem(QString::number(_count)),
                  createPercentModelItem(_count * 100.0 / totalCount) });
        };
        addCharacterItemToReport(
            QCoreApplication::translate("BusinessLogic::ScreenplaySummaryReport",
                                        "More than 10 dialogues"),
            speakMore10);
        addCharacterItemToReport(
            QCoreApplication::translate("BusinessLogic::ScreenplaySummaryReport",
                                        "About 10 dialogues"),
            speakAbout10);
        addCharacterItemToReport(
            QCoreApplication::translate("BusinessLogic::ScreenplaySummaryReport", "Nonspeaking"),
            nonspeaking);
        //
        d->charactersInfoModel->setHeaderData(
            0, Qt::Horizontal,
            QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Character type"),
            Qt::DisplayRole);
        d->charactersInfoModel->setHeaderData(
            1, Qt::Horizontal,
            QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Occurrences"),
            Qt::DisplayRole);
        d->charactersInfoModel->setHeaderData(
            2, Qt::Horizontal,
            QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Percents"),
            Qt::DisplayRole);
    }
}

std::chrono::milliseconds ScreenplaySeriesSummaryReport::duration() const
{
    return d->duration;
}

int ScreenplaySeriesSummaryReport::pagesCount() const
{
    return d->pagesCount;
}

int ScreenplaySeriesSummaryReport::wordsCount() const
{
    return d->wordsCount;
}

ScreenplaySeriesSummaryReport::CharactersCount ScreenplaySeriesSummaryReport::charactersCount()
    const
{
    return d->charactersCount;
}

QAbstractItemModel* ScreenplaySeriesSummaryReport::textInfoModel() const
{
    return d->textInfoModel.data();
}

QAbstractItemModel* ScreenplaySeriesSummaryReport::scenesInfoModel() const
{
    return d->scenesInfoModel.data();
}

QAbstractItemModel* ScreenplaySeriesSummaryReport::locationsInfoModel() const
{
    return d->locationsInfoModel.data();
}

QAbstractItemModel* ScreenplaySeriesSummaryReport::charactersInfoModel() const
{
    return d->charactersInfoModel.data();
}

void ScreenplaySeriesSummaryReport::saveToPdf(const QString& _fileName) const
{
    const auto& exportTemplate
        = TemplatesFacade::screenplayTemplate(d->episodesModel->informationModel()->templateId());

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
    cursor.beginEditBlock();
    QTextCharFormat titleFormat;
    auto titleFont = report.defaultFont();
    titleFont.setBold(true);
    titleFormat.setFont(titleFont);
    cursor.setCharFormat(titleFormat);
    cursor.insertText(QString("%1 - %2").arg(
        d->episodesModel->informationModel()->name(),
        QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Summary report")));
    cursor.insertBlock();
    cursor.insertBlock();

    cursor.insertText(
        QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Duration") + ": "
        + TimeHelper::toString(duration()));
    cursor.insertBlock();
    cursor.insertText(QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Pages")
                      + ": " + QString::number(pagesCount()));
    cursor.insertBlock();
    cursor.insertText(QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Words")
                      + ": " + QString::number(wordsCount()));
    cursor.insertBlock();
    cursor.insertText(
        QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport",
                                    "Characters with/without spaces")
        + ": "
        + QString("%1/%2").arg(charactersCount().withSpaces).arg(charactersCount().withoutSpaces));
    cursor.insertBlock();
    cursor.insertBlock();

    QTextTableFormat tableFormat;
    tableFormat.setBorder(0);
    tableFormat.setBorderStyle(QTextFrameFormat::BorderStyle_None);
    tableFormat.setColumnWidthConstraints({
        QTextLength{ QTextLength::PercentageLength, 52 },
        QTextLength{ QTextLength::PercentageLength, 16 },
        QTextLength{ QTextLength::PercentageLength, 16 },
        QTextLength{ QTextLength::PercentageLength, 16 },
    });
    auto beforeTablePosition = cursor.position();
    cursor.insertTable(textInfoModel()->rowCount() + 1, textInfoModel()->columnCount(),
                       tableFormat);
    cursor.setPosition(beforeTablePosition);
    cursor.movePosition(QTextCursor::NextBlock);
    //
    for (int column = 0; column < textInfoModel()->columnCount(); ++column) {
        QTextTableCellFormat cellFormat;
        cellFormat.setBottomBorder(1);
        cellFormat.setVerticalAlignment(QTextCharFormat::AlignBottom);
        cellFormat.setBottomBorderStyle(QTextFrameFormat::BorderStyle_Solid);
        cellFormat.setBottomBorderBrush(Qt::black);
        cursor.mergeBlockCharFormat(cellFormat);
        QTextBlockFormat blockFormat = cursor.blockFormat();
        blockFormat.setAlignment(column == 0 ? Qt::AlignLeft : Qt::AlignRight);
        cursor.setBlockFormat(blockFormat);
        cursor.insertText(textInfoModel()->headerData(column, Qt::Horizontal).toString());

        cursor.movePosition(QTextCursor::NextBlock);
    }
    for (int row = 0; row < textInfoModel()->rowCount(); ++row) {
        for (int column = 0; column < textInfoModel()->columnCount(); ++column) {
            QTextBlockFormat blockFormat = cursor.blockFormat();
            blockFormat.setAlignment(column == 0 ? Qt::AlignLeft : Qt::AlignRight);
            cursor.setBlockFormat(blockFormat);
            cursor.insertText(textInfoModel()->index(row, column).data().toString());
            cursor.movePosition(QTextCursor::NextBlock);
        }
    }
    cursor.insertBlock();
    cursor.insertBlock();

    tableFormat.setColumnWidthConstraints({
        QTextLength{ QTextLength::PercentageLength, 68 },
        QTextLength{ QTextLength::PercentageLength, 16 },
        QTextLength{ QTextLength::PercentageLength, 16 },
    });
    beforeTablePosition = cursor.position();
    cursor.insertTable(scenesInfoModel()->rowCount() + 1, scenesInfoModel()->columnCount(),
                       tableFormat);
    cursor.setPosition(beforeTablePosition);
    cursor.movePosition(QTextCursor::NextBlock);
    //
    for (int column = 0; column < scenesInfoModel()->columnCount(); ++column) {
        QTextTableCellFormat cellFormat;
        cellFormat.setBottomBorder(1);
        cellFormat.setVerticalAlignment(QTextCharFormat::AlignBottom);
        cellFormat.setBottomBorderStyle(QTextFrameFormat::BorderStyle_Solid);
        cellFormat.setBottomBorderBrush(Qt::black);
        cursor.mergeBlockCharFormat(cellFormat);
        QTextBlockFormat blockFormat = cursor.blockFormat();
        blockFormat.setAlignment(column == 0 ? Qt::AlignLeft : Qt::AlignRight);
        cursor.setBlockFormat(blockFormat);
        cursor.insertText(scenesInfoModel()->headerData(column, Qt::Horizontal).toString());

        cursor.movePosition(QTextCursor::NextBlock);
    }
    for (int row = 0; row < scenesInfoModel()->rowCount(); ++row) {
        for (int column = 0; column < scenesInfoModel()->columnCount(); ++column) {
            QTextBlockFormat blockFormat = cursor.blockFormat();
            blockFormat.setAlignment(column == 0 ? Qt::AlignLeft : Qt::AlignRight);
            cursor.setBlockFormat(blockFormat);
            cursor.insertText(scenesInfoModel()->index(row, column).data().toString());
            cursor.movePosition(QTextCursor::NextBlock);
        }
    }
    cursor.insertBlock();
    cursor.insertBlock();

    tableFormat.setColumnWidthConstraints({
        QTextLength{ QTextLength::PercentageLength, 68 },
        QTextLength{ QTextLength::PercentageLength, 16 },
        QTextLength{ QTextLength::PercentageLength, 16 },
    });
    beforeTablePosition = cursor.position();
    cursor.insertTable(locationsInfoModel()->rowCount() + 1, locationsInfoModel()->columnCount(),
                       tableFormat);
    cursor.setPosition(beforeTablePosition);
    cursor.movePosition(QTextCursor::NextBlock);
    //
    for (int column = 0; column < locationsInfoModel()->columnCount(); ++column) {
        QTextTableCellFormat cellFormat;
        cellFormat.setBottomBorder(1);
        cellFormat.setVerticalAlignment(QTextCharFormat::AlignBottom);
        cellFormat.setBottomBorderStyle(QTextFrameFormat::BorderStyle_Solid);
        cellFormat.setBottomBorderBrush(Qt::black);
        cursor.mergeBlockCharFormat(cellFormat);
        QTextBlockFormat blockFormat = cursor.blockFormat();
        blockFormat.setAlignment(column == 0 ? Qt::AlignLeft : Qt::AlignRight);
        cursor.setBlockFormat(blockFormat);
        cursor.insertText(locationsInfoModel()->headerData(column, Qt::Horizontal).toString());

        cursor.movePosition(QTextCursor::NextBlock);
    }
    for (int row = 0; row < locationsInfoModel()->rowCount(); ++row) {
        for (int column = 0; column < locationsInfoModel()->columnCount(); ++column) {
            QTextBlockFormat blockFormat = cursor.blockFormat();
            blockFormat.setAlignment(column == 0 ? Qt::AlignLeft : Qt::AlignRight);
            cursor.setBlockFormat(blockFormat);
            cursor.insertText(locationsInfoModel()->index(row, column).data().toString());
            cursor.movePosition(QTextCursor::NextBlock);
        }
    }
    cursor.insertBlock();
    cursor.insertBlock();

    tableFormat.setColumnWidthConstraints({
        QTextLength{ QTextLength::PercentageLength, 68 },
        QTextLength{ QTextLength::PercentageLength, 16 },
        QTextLength{ QTextLength::PercentageLength, 16 },
    });
    beforeTablePosition = cursor.position();
    cursor.insertTable(charactersInfoModel()->rowCount() + 1, charactersInfoModel()->columnCount(),
                       tableFormat);
    cursor.setPosition(beforeTablePosition);
    cursor.movePosition(QTextCursor::NextBlock);
    //
    for (int column = 0; column < charactersInfoModel()->columnCount(); ++column) {
        QTextTableCellFormat cellFormat;
        cellFormat.setBottomBorder(1);
        cellFormat.setVerticalAlignment(QTextCharFormat::AlignBottom);
        cellFormat.setBottomBorderStyle(QTextFrameFormat::BorderStyle_Solid);
        cellFormat.setBottomBorderBrush(Qt::black);
        cursor.mergeBlockCharFormat(cellFormat);
        QTextBlockFormat blockFormat = cursor.blockFormat();
        blockFormat.setAlignment(column == 0 ? Qt::AlignLeft : Qt::AlignRight);
        cursor.setBlockFormat(blockFormat);
        cursor.insertText(charactersInfoModel()->headerData(column, Qt::Horizontal).toString());

        cursor.movePosition(QTextCursor::NextBlock);
    }
    for (int row = 0; row < charactersInfoModel()->rowCount(); ++row) {
        for (int column = 0; column < charactersInfoModel()->columnCount(); ++column) {
            QTextBlockFormat blockFormat = cursor.blockFormat();
            blockFormat.setAlignment(column == 0 ? Qt::AlignLeft : Qt::AlignRight);
            cursor.setBlockFormat(blockFormat);
            cursor.insertText(charactersInfoModel()->index(row, column).data().toString());
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

void ScreenplaySeriesSummaryReport::saveToXlsx(const QString& _fileName) const
{
    QXlsx::Document xlsx;
    QXlsx::Format headerFormat;
    headerFormat.setFontBold(true);

    constexpr int firstRow = 1;
    constexpr int firstColumn = 1;
    int reportRow = firstRow;
    auto writeTitle = [&xlsx, &headerFormat, &reportRow](const QString& _text) {
        xlsx.write(reportRow++, 1, _text, headerFormat);
        ++reportRow;
    };
    auto writeHeader = [&xlsx, &headerFormat, &reportRow](int _column, const QVariant& _text) {
        xlsx.write(reportRow, _column, _text, headerFormat);
    };
    auto writeText = [&xlsx, &reportRow](int _column, const QVariant& _text) {
        xlsx.write(reportRow, _column, _text);
    };

    //
    // Сводка
    //
    writeTitle(QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport",
                                           "Summary statistics"));
    int reportColumn = firstColumn;
    writeHeader(reportColumn++,
                QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Duration"));
    writeText(reportColumn, TimeHelper::toString(duration()));
    ++reportRow;
    reportColumn = firstColumn;
    writeHeader(reportColumn++,
                QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Pages"));
    writeText(reportColumn, QString::number(pagesCount()));
    ++reportRow;
    reportColumn = firstColumn;
    writeHeader(reportColumn++,
                QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Words"));
    writeText(reportColumn, QString::number(wordsCount()));
    ++reportRow;
    reportColumn = firstColumn;
    writeHeader(reportColumn++,
                QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport",
                                            "Characters with/without spaces"));
    writeText(
        reportColumn,
        QString("%1/%2").arg(charactersCount().withSpaces).arg(charactersCount().withoutSpaces));

    //
    // Статистика по тексту
    //
    reportRow += 2;
    writeTitle(
        QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Text statistics"));
    for (int column = firstColumn; column < firstColumn + textInfoModel()->columnCount();
         ++column) {
        writeHeader(column, textInfoModel()->headerData(column - firstColumn, Qt::Horizontal));
    }
    for (int row = 0; row < textInfoModel()->rowCount(); ++row) {
        ++reportRow;
        for (int column = firstColumn; column < firstColumn + 4; ++column) {
            writeText(column, textInfoModel()->index(row, column - firstColumn).data());
        }
    }

    //
    // Статистика по сценам
    //
    reportRow += 2;
    writeTitle(
        QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport", "Scenes statistics"));
    for (int column = firstColumn; column < firstColumn + scenesInfoModel()->columnCount();
         ++column) {
        writeHeader(column, scenesInfoModel()->headerData(column - firstColumn, Qt::Horizontal));
    }
    for (int row = 0; row < scenesInfoModel()->rowCount(); ++row) {
        ++reportRow;
        for (int column = firstColumn; column < firstColumn + 3; ++column) {
            writeText(column, scenesInfoModel()->index(row, column - firstColumn).data());
        }
    }

    //
    // Статистика по локациям
    //
    reportRow += 2;
    writeTitle(QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport",
                                           "Locations statistics"));
    for (int column = firstColumn; column < firstColumn + locationsInfoModel()->columnCount();
         ++column) {
        writeHeader(column, locationsInfoModel()->headerData(column - firstColumn, Qt::Horizontal));
    }
    for (int row = 0; row < locationsInfoModel()->rowCount(); ++row) {
        ++reportRow;
        for (int column = firstColumn; column < firstColumn + 3; ++column) {
            writeText(column, locationsInfoModel()->index(row, column - firstColumn).data());
        }
    }

    //
    // Статистика по персонажам
    //
    reportRow += 2;
    writeTitle(QCoreApplication::translate("BusinessLayer::ScreenplaySummaryReport",
                                           "Characters statistics"));
    for (int column = firstColumn; column < firstColumn + charactersInfoModel()->columnCount();
         ++column) {
        writeHeader(column,
                    charactersInfoModel()->headerData(column - firstColumn, Qt::Horizontal));
    }
    for (int row = 0; row < charactersInfoModel()->rowCount(); ++row) {
        ++reportRow;
        for (int column = firstColumn; column < firstColumn + 3; ++column) {
            writeText(column, charactersInfoModel()->index(row, column - firstColumn).data());
        }
    }

    xlsx.saveAs(_fileName);
}

} // namespace BusinessLayer
