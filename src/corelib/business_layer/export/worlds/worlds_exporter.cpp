#include "worlds_exporter.h"

#include <business_layer/document/simple_text/simple_text_document.h>
#include <business_layer/document/text/text_cursor.h>
#include <business_layer/export/export_options.h>
#include <business_layer/model/worlds/world_model.h>
#include <business_layer/model/worlds/worlds_model.h>
#include <business_layer/templates/simple_text_template.h>
#include <business_layer/templates/templates_facade.h>
#include <domain/document_object.h>
#include <ui/design_system/design_system.h>
#include <ui/widgets/text_edit/page/page_text_edit.h>
#include <utils/helpers/color_helper.h>
#include <utils/helpers/measurement_helper.h>
#include <utils/helpers/text_helper.h>

#include <QCoreApplication>
#include <QPainter>
#include <QPainterPath>
#include <QTextBlock>


namespace BusinessLayer {

TextDocument* WorldsExporter::createDocument(const ExportOptions& _exportOptions) const
{
    Q_UNUSED(_exportOptions)
    return new SimpleTextDocument;
}

const TextTemplate& WorldsExporter::documentTemplate(const ExportOptions& _exportOptions) const
{
    Q_UNUSED(_exportOptions)
    return TemplatesFacade::simpleTextTemplate();
}

TextDocument* WorldsExporter::prepareDocument(AbstractModel* _model,
                                              const ExportOptions& _exportOptions) const
{

    //
    // Настраиваем документ
    //
    PageTextEdit textEdit;
    textEdit.setUsePageMode(true);
    textEdit.setPageSpacing(0);
    auto textDocument = createDocument(_exportOptions);
    textEdit.setDocument(textDocument);
    //
    // ... параметры страницы
    //
    const auto& exportTemplate = documentTemplate(_exportOptions);
    textEdit.setPageFormat(exportTemplate.pageSizeId());
    textEdit.setPageMarginsMm(exportTemplate.pageMargins());
    textEdit.setPageNumbersAlignment(exportTemplate.pageNumbersAlignment());

    //
    // Работаем от стандартного шрифта шаблона
    //
    auto baseFont = exportTemplate.baseFont();
    baseFont.setPixelSize(MeasurementHelper::ptToPx(12));
    auto titleFont = baseFont;
    titleFont.setPixelSize(MeasurementHelper::ptToPx(20));
    titleFont.setBold(true);
    auto headingFont = baseFont;
    headingFont.setPixelSize(MeasurementHelper::ptToPx(16));
    headingFont.setBold(true);

    const auto worlds = qobject_cast<WorldsModel*>(_model);
    auto blockFormat = [](qreal _topMargin = 0.0, qreal _bottomMargin = 0.0) {
        QTextBlockFormat format;
        format.setTopMargin(_topMargin);
        format.setBottomMargin(_bottomMargin);
        return format;
    };
    auto charFormat = [](const QFont& _font, const QColor& _textColor = {},
                         const QColor& _backgroundColor = {}) {
        QTextCharFormat format;
        format.setFont(_font);
        if (_textColor.isValid()) {
            format.setForeground(_textColor);
        }
        if (_backgroundColor.isValid()) {
            format.setBackground(_backgroundColor);
        }
        return format;
    };

    const auto& exportOptions = static_cast<const DocumentsExportOptions&>(_exportOptions);

    TextCursor cursor(textDocument);
    int worldStartPosition = 0;
    int worldEndPosition = 0;
    for (int index = 0; index < worlds->rowCount(); ++index) {
        auto world = worlds->world(index);
        if (!exportOptions.documents.contains(world->name())) {
            continue;
        }

        if (index > 0) {
            cursor.insertBlock(
                blockFormat(MeasurementHelper::ptToPx(cursor.block().text().isEmpty() ? 0 : 20)),
                charFormat(baseFont));
        }

        worldStartPosition = cursor.position();
        cursor.beginEditBlock();
        //
        // Фотка локации
        //
        int photoHeight = 0;
        if (exportOptions.includeMainPhoto && !world->mainPhoto().image.isNull()) {
            const auto mainPhotoScaled = world->mainPhoto().image.scaled(
                MeasurementHelper::mmToPx(20), MeasurementHelper::mmToPx(20),
                Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            const auto borderMargin = MeasurementHelper::mmToPx(3);
            QImage mainPhoto(mainPhotoScaled.size() + QSize(borderMargin, borderMargin),
                             QImage::Format_ARGB32_Premultiplied);
            QPainter painter(&mainPhoto);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.fillRect(mainPhoto.rect(), Qt::white);
            const auto targetRect
                = mainPhoto.rect().marginsRemoved(QMargins(borderMargin, 0, 0, borderMargin));
            QPainterPath clipPath;
            clipPath.addRoundedRect(targetRect, borderMargin, borderMargin);
            painter.setClipPath(clipPath);
            painter.drawPixmap(targetRect, mainPhotoScaled, mainPhotoScaled.rect());
            const QString imageName = "world_photo_" + QString::number(index);
            textDocument->addResource(QTextDocument::ImageResource, QUrl(imageName), mainPhoto);
            QTextImageFormat format;
            format.setName(imageName);
            cursor.insertImage(format, QTextFrameFormat::FloatRight);

            photoHeight = mainPhoto.height();
        }
        //
        // Название
        //
        const auto titleFormat = charFormat(titleFont);
        cursor.insertText(world->name(), titleFormat);
        //
        // Краткое описание
        //
        const auto baseFormat = charFormat(baseFont);
        if (exportOptions.includeOneLineDescription && !world->oneSentenceDescription().isEmpty()) {
            cursor.insertBlock(blockFormat(MeasurementHelper::ptToPx(4)), baseFormat);
            cursor.insertText(world->oneSentenceDescription());
        }
        //
        // Детальное описание
        //
        if (exportOptions.includeLongDescription && !world->longDescription().isEmpty()) {
            cursor.insertBlock(blockFormat(MeasurementHelper::ptToPx(6)), baseFormat);
            cursor.insertText(world->longDescription());
        }

        //
        // Если после картинки не было добавлено ни одного блока, то сделаем это, чтобы корректно
        // высчитать высоту недостающего контента и сделать правильный отступ между персонажами,
        // иначе Qt считает, что позиция курсора в блоке выше, чем реальная позиция
        //
        if (photoHeight > 0 && worldStartPosition == cursor.block().position()) {
            cursor.insertBlock(blockFormat(), baseFormat);
            cursor.insertText(" ");
        }

        cursor.endEditBlock();
        worldEndPosition = cursor.position();

        const auto worldHeight = textEdit.cursorRectAt(worldEndPosition).bottom()
            - textEdit.cursorRectAt(worldStartPosition).top();
        if (photoHeight > 0 && photoHeight > worldHeight) {
            cursor.insertBlock(blockFormat(photoHeight - worldHeight));
        }
    }

    return textDocument;
}

} // namespace BusinessLayer
