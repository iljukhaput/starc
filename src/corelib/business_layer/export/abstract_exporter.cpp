#include "abstract_exporter.h"

#include <business_layer/document/text/text_cursor.h>
#include <business_layer/document/text/text_document.h>
#include <business_layer/export/export_options.h>
#include <business_layer/model/simple_text/simple_text_model.h>
#include <business_layer/model/text/text_model.h>
#include <business_layer/templates/text_template.h>
#include <ui/widgets/text_edit/page/page_text_edit.h>
#include <utils/helpers/measurement_helper.h>

#include <QGuiApplication>
#include <QTextBlock>


namespace BusinessLayer {

TextDocument* AbstractExporter::prepareDocument(TextModel* _model,
                                                const ExportOptions& _exportOptions) const
{
    //
    // Настраиваем документ
    //
    PageTextEdit textEdit;
    textEdit.setUsePageMode(true);
    textEdit.setPageSpacing(0);
    auto audioplayText = createDocument();
    textEdit.setDocument(audioplayText);
    //
    // ... параметры страницы
    //
    const auto& exportTemplate = documentTemplate(_exportOptions);
    textEdit.setPageFormat(exportTemplate.pageSizeId());
    textEdit.setPageMarginsMm(exportTemplate.pageMargins());
    textEdit.setPageNumbersAlignment(exportTemplate.pageNumbersAlignment());
    //
    // ... формируем текст сценария
    //
    audioplayText->setModel(_model, false);
    //
    // ... отсоединяем документ от модели, что изменения в документе не привели к изменениям модели
    //
    audioplayText->disconnect();
    //
    // ... корректируем текст сценария
    //
    TextCursor cursor(audioplayText);
    //
    // ... вставляем титульную страницу
    //
    if (_exportOptions.includeTiltePage) {
        //
        // Переносим основной текст на следующую страницу
        //
        cursor.insertBlock(cursor.blockFormat(), cursor.blockCharFormat());
        auto blockFormat = cursor.blockFormat();
        blockFormat.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
        blockFormat.setTopMargin(0);
        cursor.setBlockFormat(blockFormat);

        //
        // Собственно добавляем текст титульной страницы
        //
        auto titlePageText = new TextDocument;
        titlePageText->setModel(_model->titlePageModel(), false);
        //
        cursor.movePosition(TextCursor::Start);
        auto block = titlePageText->begin();
        while (block.isValid()) {
            //
            // Донастроим стиль блока
            //
            auto blockFormat = block.blockFormat();
            //
            // ... сбросим тип
            //
            blockFormat.setProperty(TextBlockStyle::PropertyType,
                                    static_cast<int>(TextParagraphType::Undefined));
            //
            // ... и уравняем отступы
            //
            if (exportTemplate.pageMargins().left() < exportTemplate.pageMargins().right()) {
                blockFormat.setLeftMargin(MeasurementHelper::mmToPx(
                    exportTemplate.pageMargins().right() - exportTemplate.pageMargins().left()));
            } else if (exportTemplate.pageMargins().right() < exportTemplate.pageMargins().left()) {
                blockFormat.setRightMargin(MeasurementHelper::mmToPx(
                    exportTemplate.pageMargins().left() - exportTemplate.pageMargins().right()));
            }
            //
            // ... вставляем блок
            //
            if (cursor.atStart()) {
                cursor.setBlockFormat(blockFormat);
                cursor.setBlockCharFormat(block.charFormat());
            } else {
                cursor.insertBlock(blockFormat, block.charFormat());
            }
            //
            // ... вставляем текст
            //
            const auto formats = block.textFormats();
            for (const auto& format : formats) {
                cursor.insertText(block.text().mid(format.start, format.length), format.format);
            }
            //
            // ... если первый блок пуст, добавим пробел, чтобы избежать косяка с сохранением в PDF
            //
            if (cursor.atStart()) {
                cursor.insertText(" ");
            }

            block = block.next();
        }

        //
        // Убираем нижний отступ у поледнего блока титульной страницы
        //
        blockFormat = cursor.blockFormat();
        blockFormat.setBottomMargin(0);
        cursor.setBlockFormat(blockFormat);

        //
        // Переходим к тексту сценария
        //
        cursor.movePosition(TextCursor::NextBlock);
        cursor.movePosition(TextCursor::StartOfBlock);
    }
    //
    // ... для первого блока убираем принудительный перенос страницы,
    //     если он есть и если не печатается титульная страница
    //
    else if (cursor.block().blockFormat().pageBreakPolicy()
             == QTextFormat::PageBreak_AlwaysBefore) {
        auto blockFormat = cursor.blockFormat();
        blockFormat.setPageBreakPolicy(QTextFormat::PageBreak_Auto);
        cursor.setBlockFormat(blockFormat);
    }
    //
    do {
        const auto blockType = TextBlockStyle::forBlock(cursor.block());

        //
        // Если не нужно печатать папки, то удаляем их
        //
        if (!_exportOptions.includeFolders) {
            if (blockType == TextParagraphType::SequenceHeading
                || blockType == TextParagraphType::SequenceFooter) {
                cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                if (cursor.hasSelection()) {
                    cursor.deleteChar();
                }
                cursor.deleteChar();
                continue;
            }
        }
        //
        // В противном случае подставляем текст для пустых завершающих блоков
        //
        else if (blockType == TextParagraphType::SequenceFooter) {
            if (cursor.block().text().isEmpty()) {
                auto headerBlock = cursor.block().previous();
                int openedFolders = 0;
                while (headerBlock.isValid()) {
                    const auto headerBlockType = TextBlockStyle::forBlock(headerBlock);
                    if (headerBlockType == TextParagraphType::SequenceHeading) {
                        if (openedFolders > 0) {
                            --openedFolders;
                        } else {
                            break;
                        }
                    } else if (headerBlockType == TextParagraphType::SequenceFooter) {
                        ++openedFolders;
                    }

                    headerBlock = headerBlock.previous();
                }

                const auto footerText = QString("%1 %2").arg(
                    QGuiApplication::translate("KeyProcessingLayer::FolderFooterHandler", "END OF"),
                    headerBlock.text());
                cursor.insertText(footerText);
            }
        }

        //
        // Если не нужно печатать заметки по тексту, то удаляем их
        //
        if (!_exportOptions.includeInlineNotes && blockType == TextParagraphType::InlineNote) {
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            if (cursor.hasSelection()) {
                cursor.deleteChar();
            }
            cursor.deleteChar();
            continue;
        }

        cursor.movePosition(QTextCursor::EndOfBlock);
        cursor.movePosition(QTextCursor::NextBlock);
    } while (!cursor.atEnd());

    return audioplayText;
}

} // namespace BusinessLayer