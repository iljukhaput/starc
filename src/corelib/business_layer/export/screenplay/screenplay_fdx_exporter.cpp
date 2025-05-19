#include "screenplay_fdx_exporter.h"

#include "screenplay_export_options.h"

#include <business_layer/document/screenplay/text/screenplay_text_document.h>
#include <business_layer/document/text/text_block_data.h>
#include <business_layer/model/screenplay/text/screenplay_text_model_scene_item.h>
#include <business_layer/templates/screenplay_template.h>
#include <business_layer/templates/templates_facade.h>
#include <utils/helpers/measurement_helper.h>

#include <QFile>
#include <QXmlStreamWriter>


namespace BusinessLayer {

namespace {

/**
 * @brief Шаблон экспорта
 */
const TextTemplate& exportTemplate(const ScreenplayExportOptions& _exportOptions)
{
    return TemplatesFacade::screenplayTemplate(_exportOptions.templateId);
}

/**
 * @brief Размер страницы в экспортируемом шаблоне
 */
QSizeF exportTemplatePageSize(const ScreenplayExportOptions& _exportOptions)
{
    return QPageSize(exportTemplate(_exportOptions).pageSizeId()).size(QPageSize::Inch);
}

/**
 * @brief Поля экспортируемого документа
 */
QMarginsF exportTemplatePageMargins(const ScreenplayExportOptions& _exportOptions)
{
    const auto pageMargins = exportTemplate(_exportOptions).pageMargins();
    return { MeasurementHelper::mmToInch(pageMargins.left()),
             MeasurementHelper::mmToInch(pageMargins.top()),
             MeasurementHelper::mmToInch(pageMargins.right()),
             MeasurementHelper::mmToInch(pageMargins.bottom()) };
}

/**
 * @brief Записать строку с контентом сценария
 */
void writeLine(QXmlStreamWriter& _writer, const QTextBlock& _block,
               const ScreenplayExportOptions& _exportOptions)
{
    QString paragraphTypeName;
    QString sceneNumber;
    const auto paragraphType = TextBlockStyle::forBlock(_block);
    switch (paragraphType) {
    case TextParagraphType::SceneHeading: {
        paragraphTypeName = "Scene Heading";

        //
        // ... если надо, то выводим номера сцен
        //
        if (_exportOptions.showScenesNumbers) {
            const auto blockData = static_cast<TextBlockData*>(_block.userData());
            if (blockData != nullptr && blockData->item()->parent() != nullptr
                && blockData->item()->parent()->type() == TextModelItemType::Group
                && static_cast<TextGroupType>(blockData->item()->parent()->subtype())
                    == TextGroupType::Scene) {
                const auto sceneItem
                    = static_cast<ScreenplayTextModelSceneItem*>(blockData->item()->parent());
                sceneNumber = sceneItem->number()->text;
            }
        }

        break;
    }

    case TextParagraphType::Action: {
        paragraphTypeName = "Action";
        break;
    }

    case TextParagraphType::Character: {
        paragraphTypeName = "Character";
        break;
    }

    case TextParagraphType::Parenthetical: {
        paragraphTypeName = "Parenthetical";
        break;
    }

    case TextParagraphType::Dialogue: {
        paragraphTypeName = "Dialogue";
        break;
    }

    case TextParagraphType::Transition: {
        paragraphTypeName = "Transition";
        break;
    }

    case TextParagraphType::Shot: {
        paragraphTypeName = "Shot";
        break;
    }

    case TextParagraphType::SceneCharacters: {
        paragraphTypeName = "Cast List";
        break;
    }

    case TextParagraphType::Lyrics: {
        paragraphTypeName = "Lyrics";
        break;
    }

    default: {
        paragraphTypeName = "General";
        break;
    }
    }

    _writer.writeStartElement("Paragraph");
    if (!sceneNumber.isEmpty()) {
        _writer.writeAttribute("Number", sceneNumber);
    }
    _writer.writeAttribute("Type", paragraphTypeName);
    _writer.writeAttribute(
        "Alignment",
        _block.blockFormat().alignment().testFlag(Qt::AlignLeft)
            ? "Left"
            : (_block.blockFormat().alignment().testFlag(Qt::AlignHCenter) ? "Center" : "Right"));
    if (paragraphTypeName == "General") {
        _writer.writeAttribute(
            "LeftIndent",
            QString::number(MeasurementHelper::pxToInch(_block.blockFormat().leftMargin())));
        _writer.writeAttribute(
            "RightIndent",
            QString::number(exportTemplatePageSize(_exportOptions).width()
                            - MeasurementHelper::pxToInch(_block.blockFormat().rightMargin())));
        _writer.writeAttribute(
            "SpaceBefore",
            QString::number(MeasurementHelper::pxToInch(_block.blockFormat().topMargin())));
        //
        // TODO: поресёрчить
        //
        _writer.writeAttribute("Spacing", "1.0");
        if (_block.blockFormat().pageBreakPolicy() == QTextFormat::PageBreak_AlwaysBefore) {
            _writer.writeAttribute("StartsNewPage", "Yes");
        }
    }
    const auto formatRanges = _block.textFormats();
    for (const auto& formatRange : formatRanges) {
        _writer.writeStartElement("Text");
        //
        // Пишем стиль блока
        //
        QString style;
        QString styleDivider;
        if (formatRange.format.fontWeight() == QFont::Bold) {
            style = "Bold";
            styleDivider = "+";
        }
        if (formatRange.format.fontItalic()) {
            style += styleDivider + "Italic";
            styleDivider = "+";
        }
        if (formatRange.format.fontUnderline()) {
            style += styleDivider + "Underline";
        }
        //
        if (!style.isEmpty()) {
            _writer.writeAttribute("Style", style);
        }
        //
        // Пишем текст
        //
        if (formatRange == formatRanges.constFirst()
            && paragraphType == TextParagraphType::Parenthetical) {
            _writer.writeCharacters("(");
        }
        _writer.writeCharacters(_block.text().mid(formatRange.start, formatRange.length));
        if (formatRange == formatRanges.constLast()
            && paragraphType == TextParagraphType::Parenthetical) {
            _writer.writeCharacters(")");
        }
        //
        _writer.writeEndElement();
    }
    _writer.writeEndElement(); // Paragraph
}

/**
 * @brief Записать текст документа без титульной страницы
 */
void writeContent(QXmlStreamWriter& _writer, TextDocument* _screenplayText,
                  const ScreenplayExportOptions& _exportOptions)
{
    _writer.writeStartElement("Content");

    //
    // Данные титульной страницы считываются из исходного документа, до момент начала сценария
    //
    auto block = _screenplayText->begin();
    bool titlePageSkipped = false;
    while (block.isValid()) {
        if (_exportOptions.includeTitlePage && !titlePageSkipped) {
            if (TextBlockStyle::forBlock(block) == TextParagraphType::Undefined) {
                block = block.next();
                continue;
            } else {
                titlePageSkipped = true;
            }
        }

        writeLine(_writer, block, _exportOptions);

        block = block.next();
    }

    _writer.writeEndElement(); // Content
}

/**
 * @brief Записать параметры шаблона
 */
void writeSettings(QXmlStreamWriter& _writer, const ScreenplayExportOptions& _exportOptions)
{
    //
    // TODO: Реализовать качественно параметры страницы
    //

    //
    // Информация о параметрах страницы
    //
    const auto exportStyle = exportTemplate(_exportOptions);

    //
    // Пишем параметры страницы
    //
    _writer.writeStartElement("PageLayout");
    _writer.writeAttribute("BackgroundColor", "#FFFFFFFFFFFF");
    const auto pageMargins = exportTemplatePageMargins(_exportOptions) * 76;
    _writer.writeAttribute("BottomMargin", QString::number(pageMargins.bottom() * 2 / 3));
    _writer.writeAttribute("BreakDialogueAndActionAtSentences", "Yes");
    _writer.writeAttribute("DocumentLeading", "Normal");
    _writer.writeAttribute("FooterMargin", QString::number(pageMargins.bottom() * 1 / 3));
    _writer.writeAttribute("ForegroundColor", "#000000000000");
    _writer.writeAttribute("HeaderMargin", QString::number(pageMargins.top() * 1 / 3));
    _writer.writeAttribute("InvisiblesColor", "#808080808080");
    _writer.writeAttribute("TopMargin", QString::number(pageMargins.top() * 2 / 3));
    _writer.writeAttribute("UsesSmartQuotes", "Yes");
    //
    _writer.writeEmptyElement("PageSize");
    const auto pageSize = exportTemplatePageSize(_exportOptions);
    _writer.writeAttribute("Height", QString::number(pageSize.height()));
    _writer.writeAttribute("Width", QString::number(pageSize.width()));
    //
    _writer.writeEmptyElement("AutoCastList");
    _writer.writeAttribute("AddParentheses", "Yes");
    _writer.writeAttribute("AutomaticallyGenerate", "Yes");
    _writer.writeAttribute("CastListElement", "Cast List");
    //
    _writer.writeEndElement(); // PageLayout

    //
    // TODO: Параметры стилей сценария записывать тоже
    //
}

/**
 * @brief Записать титульную страницу
 */
void writeTitlePage(QXmlStreamWriter& _writer, TextDocument* _screenplayText,
                    const ScreenplayExportOptions& _exportOptions)
{
    if (!_exportOptions.includeTitlePage) {
        return;
    }

    _writer.writeStartElement("TitlePage");
    _writer.writeStartElement("Content");

    //
    // Данные титульной страницы считываются из исходного документа, до момент начала сценария
    //
    auto block = _screenplayText->begin();
    while (block.isValid() && TextBlockStyle::forBlock(block) == TextParagraphType::Undefined) {
        writeLine(_writer, block, _exportOptions);

        block = block.next();
    }

    _writer.writeEndElement(); // Content
    _writer.writeEndElement(); // Title Page
}

} // namespace


void ScreenplayFdxExporter::exportTo(AbstractModel* _model, ExportOptions& _exportOptions) const
{
    //
    // Открываем документ на запись
    //
    QFile fdxFile(_exportOptions.filePath);
    if (!fdxFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }

    QXmlStreamWriter writer(&fdxFile);
    //
    // Начало документа
    //
    writer.writeStartDocument();
    writer.writeStartElement("FinalDraft");
    writer.writeAttribute("DocumentType", "Script");
    writer.writeAttribute("Template", "No");
    writer.writeAttribute("Version", "1");
    //
    QScopedPointer<TextDocument> document(prepareDocument(_model, _exportOptions));
    const auto& exportOptions = static_cast<const ScreenplayExportOptions&>(_exportOptions);
    writeContent(writer, document.data(), exportOptions);
    writeSettings(writer, exportOptions);
    writeTitlePage(writer, document.data(), exportOptions);
    //
    // Конец документа
    //
    writer.writeEndDocument();

    fdxFile.close();
}

} // namespace BusinessLayer
