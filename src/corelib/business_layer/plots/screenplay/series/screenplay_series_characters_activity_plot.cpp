#include "screenplay_series_characters_activity_plot.h"

#include <3rd_party/qtxlsxwriter/xlsxdocument.h>
#include <business_layer/document/screenplay/text/screenplay_text_document.h>
#include <business_layer/model/screenplay/screenplay_information_model.h>
#include <business_layer/model/screenplay/series/screenplay_series_episodes_model.h>
#include <business_layer/model/screenplay/text/screenplay_text_block_parser.h>
#include <business_layer/model/screenplay/text/screenplay_text_model.h>
#include <business_layer/model/screenplay/text/screenplay_text_model_scene_item.h>
#include <business_layer/model/screenplay/text/screenplay_text_model_text_item.h>
#include <business_layer/templates/screenplay_template.h>
#include <business_layer/templates/templates_facade.h>
#include <ui/widgets/text_edit/page/page_text_edit.h>
#include <utils/helpers/color_helper.h>
#include <utils/helpers/text_helper.h>
#include <utils/helpers/time_helper.h>

#include <QCoreApplication>
#include <QRegularExpression>

#include <cmath>


namespace BusinessLayer {


class ScreenplaySeriesCharactersActivityPlot::Implementation
{
public:
    Plot plot;
    QVector<QString> characters;
    std::optional<QVector<QString>> visibleCharacters;
};


// ****


ScreenplaySeriesCharactersActivityPlot::ScreenplaySeriesCharactersActivityPlot()
    : d(new Implementation)
{
}

ScreenplaySeriesCharactersActivityPlot::~ScreenplaySeriesCharactersActivityPlot() = default;

void ScreenplaySeriesCharactersActivityPlot::build(QAbstractItemModel* _model) const
{
    if (_model == nullptr) {
        return;
    }

    auto episodesModel = qobject_cast<ScreenplaySeriesEpisodesModel*>(_model);
    if (episodesModel == nullptr) {
        return;
    }

    //
    // Подготовим необходимые структуры для сбора статистики
    //
    const int invalidPage = 0;
    struct SceneData {
        QString name;
        int page = invalidPage;
        QString number;
        std::chrono::milliseconds duration = std::chrono::milliseconds{ 0 };
        QSet<QString> characters;
    };
    QVector<SceneData> scenes;
    SceneData lastScene;
    QVector<QString> characters;

    //
    // Сформируем регулярное выражение для выуживания молчаливых персонажей
    //
    QString rxPattern;
    QSet<QString> characterNames;
    for (const auto episode : episodesModel->episodes()) {
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
    // Если в сериях одинаковые шаблоны, то добавляем автоматически номер эпизода
    //
    bool needToAddEpisodeNumber = false;
    QSet<QString> sceneNumbersTemplates;
    for (const auto episode : episodesModel->episodes()) {
        if (sceneNumbersTemplates.contains(episode->informationModel()->scenesNumbersTemplate())) {
            needToAddEpisodeNumber = true;
            break;
        }

        sceneNumbersTemplates.insert(episode->informationModel()->scenesNumbersTemplate());
    }

    //
    // Собираем статистику со всех сценариев
    //
    int episodeNumber = 1;
    for (const auto episode : episodesModel->episodes()) {
        const auto sceneNumberPrefix
            = needToAddEpisodeNumber ? QString("%1.").arg(episodeNumber++) : "";
        //
        // Подготовим текстовый документ, для определения страниц сцен
        //
        const auto& screenplayTemplate
            = TemplatesFacade::screenplayTemplate(episode->informationModel()->templateId());
        PageTextEdit screenplayTextEdit;
        screenplayTextEdit.setUsePageMode(true);
        screenplayTextEdit.setPageSpacing(0);
        screenplayTextEdit.setPageFormat(screenplayTemplate.pageSizeId());
        screenplayTextEdit.setPageMarginsMm(screenplayTemplate.pageMargins());
        ScreenplayTextDocument screenplayDocument;
        screenplayTextEdit.setDocument(&screenplayDocument);
        const bool kCanChangeModel = false;
        screenplayDocument.setModel(episode, kCanChangeModel);
        QTextCursor screenplayCursor(&screenplayDocument);
        auto textItemPage = [&screenplayTextEdit, &screenplayDocument,
                             &screenplayCursor](TextModelTextItem* _item) {
            screenplayCursor.setPosition(screenplayDocument.itemPosition(
                screenplayDocument.model()->indexForItem(_item), true));
            return screenplayTextEdit.cursorPage(screenplayCursor);
        };

        //
        // Собираем статистику
        //
        std::function<void(const TextModelItem*)> includeInReport;
        includeInReport = [&includeInReport, &scenes, &lastScene, &characters, &rxCharacterFinder,
                           textItemPage, invalidPage,
                           sceneNumberPrefix](const TextModelItem* _item) {
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
                    if (textItem->isCorrection()) {
                        break;
                    }

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
                            = static_cast<ScreenplayTextModelSceneItem*>(textItem->parent());
                        lastScene.name = sceneItem->heading();
                        lastScene.number = sceneNumberPrefix + sceneItem->number()->text;
                        lastScene.duration = sceneItem->duration();
                        lastScene.page = textItemPage(textItem);
                        break;
                    }

                    case TextParagraphType::SceneCharacters: {
                        const auto sceneCharacters
                            = ScreenplaySceneCharactersParser::characters(textItem->text());
                        for (const auto& character : sceneCharacters) {
                            lastScene.characters.insert(character);
                            if (!characters.contains(character)) {
                                characters.append(character);
                            }
                        }
                        break;
                    }

                    case TextParagraphType::Character: {
                        const auto character = ScreenplayCharacterParser::name(textItem->text());
                        lastScene.characters.insert(character);
                        if (!characters.contains(character)) {
                            characters.append(character);
                        }
                        break;
                    }

                    case TextParagraphType::Action: {
                        if (rxCharacterFinder.pattern().isEmpty()) {
                            break;
                        }

                        auto match = rxCharacterFinder.match(textItem->text());
                        while (match.hasMatch()) {
                            const auto character = TextHelper::smartToUpper(match.captured(2));
                            lastScene.characters.insert(character);
                            if (!characters.contains(character)) {
                                characters.append(character);
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
        includeInReport(episode->itemForIndex({}));
    }
    if (lastScene.page != invalidPage) {
        scenes.append(lastScene);
    }

    //
    // Формируем данные для визуализации
    //
    QVector<qreal> initializedVector = { 0.0 };
    // ... х - общий для всех
    auto x = initializedVector;
    // ... y
    QList<QVector<qreal>> charactersY;
    for (int characterIndex = 0; characterIndex < characters.size(); ++characterIndex) {
        charactersY.append(QVector<qreal>{ std::numeric_limits<qreal>::quiet_NaN() });
    }
    //
    const auto millisecondsInMinute = 60000.0;
    auto lastX = 0.0;
    QMap<qreal, QStringList> info;
    info.insert(lastX, {});
    for (int sceneIndex = 0; sceneIndex < scenes.size(); ++sceneIndex) {
        const auto& scene = scenes.at(sceneIndex);
        //
        // Информация
        //
        QString infoTitle = QString("%1 %2").arg(scene.number, scene.name);
        QString infoText;
        for (const auto& character : scene.characters) {
            if (!infoText.isEmpty()) {
                infoText.append("\n");
            }
            infoText.append(character);
        }
        info.insert(lastX, { infoTitle, infoText });

        //
        // По иксу откладываем длительность
        //
        x << lastX + 1000 / millisecondsInMinute;
        x << lastX + scene.duration.count() / millisecondsInMinute;
        lastX = x.last();
        //
        // По игрику активность персонажей
        //
        // Позицию по игрику наращиваем, потому что мы будем строить графики один над другим
        //
        int lastY = 0;
        for (int characterIndex = 0; characterIndex < characters.size(); ++characterIndex) {
            const auto characterName = characters.at(characterIndex);
            if (d->visibleCharacters.has_value()
                && !d->visibleCharacters->contains(characterName)) {
                continue;
            }

            auto currentY = std::numeric_limits<qreal>::quiet_NaN();
            if (scene.characters.contains(characterName)) {
                currentY = lastY;
            }
            charactersY[characterIndex] << currentY << currentY;

            lastY += 1;
        }
    }
    info.insert(lastX, {});
    //
    d->plot = {};
    d->plot.info = info;
    //
    // ... Формируем список графиков снизу вверх, чтоби они не закрашивались при выводе
    //
    int plotColorIndex = 0;
    for (int characterIndex = characters.size() - 1; characterIndex >= 0; --characterIndex) {
        const auto characterName = characters.at(characterIndex);
        if (d->visibleCharacters.has_value() && !d->visibleCharacters->contains(characterName)) {
            continue;
        }

        PlotData data;
        data.name = characters.at(characterIndex);
        data.color = ColorHelper::forNumber(plotColorIndex++);
        data.x = x;
        data.y = charactersY.at(characterIndex);
        d->plot.data.append(data);
    }
    //
    d->characters = characters;
    std::sort(d->characters.begin(), d->characters.end());
}

Plot ScreenplaySeriesCharactersActivityPlot::plot() const
{
    return d->plot;
}

void ScreenplaySeriesCharactersActivityPlot::saveToFile(const QString& _fileName) const
{
    QXlsx::Document xlsx;
    QXlsx::Format headerFormat;
    headerFormat.setFontBold(true);

    constexpr int firstRow = 1;
    constexpr int firstColumn = 1;
    int reportRow = firstRow;
    auto writeHeader = [&xlsx, &headerFormat, &reportRow](int _column, const QVariant& _text) {
        xlsx.write(reportRow, _column, _text, headerFormat);
    };
    auto writeText = [&xlsx, &reportRow](int _column, const QVariant& _text) {
        xlsx.write(reportRow, _column, _text);
    };

    int column = firstColumn;
    writeHeader(column++,
                QCoreApplication::translate("BusinessLayer::ScreenplaySeriesCharactersActivityPlot",
                                            "Scene"));
    for (const auto& plotData : std::as_const(d->plot.data)) {
        writeHeader(column++, plotData.name);
    }

    ++reportRow;
    int firstColumnWidth = 10;
    if (!d->plot.data.isEmpty()) {
        const auto scenePoints = d->plot.data.constFirst().x;
        QStringList lastSceneInfo;
        for (int index = 0; index < scenePoints.size(); ++index) {
            const auto& scenePoint = scenePoints.at(index);
            const auto sceneInfo = d->plot.info.lowerBound(scenePoint);
            if (sceneInfo == d->plot.info.end() || sceneInfo.value() == lastSceneInfo
                || sceneInfo.value().isEmpty()) {
                continue;
            }

            lastSceneInfo = sceneInfo.value();
            column = firstColumn;
            writeText(column++, sceneInfo->constFirst());
            for (const auto& plotData : std::as_const(d->plot.data)) {
                //
                // +2, т.к. нужно взять конец полосы с информацией о участии персонажа в сцене
                //
                if (!std::isnan(plotData.y.at(index + 2))) {
                    writeText(column, "+");
                }
                ++column;
            }
            ++reportRow;

            firstColumnWidth
                = std::max(firstColumnWidth, static_cast<int>(sceneInfo->constFirst().size()));
        }
    }

    xlsx.setColumnWidth(firstColumn, firstColumnWidth);

    xlsx.saveAs(_fileName);
}

QVector<QString> ScreenplaySeriesCharactersActivityPlot::characters() const
{
    return d->characters;
}

void ScreenplaySeriesCharactersActivityPlot::setParameters(const QVector<QString>& _characters)
{
    d->visibleCharacters = _characters;
}

} // namespace BusinessLayer
