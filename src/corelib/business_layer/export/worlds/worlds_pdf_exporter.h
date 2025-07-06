#pragma once

#include "worlds_exporter.h"

#include <business_layer/export/abstract_pdf_exporter.h>


namespace BusinessLayer {

class CORE_LIBRARY_EXPORT WorldsPdfExporter : public WorldsExporter, public AbstractPdfExporter
{
public:
    WorldsPdfExporter();

protected:
    /**
     * @brief Дописать в параметры экспорта данные зависящие от модели
     */
    void updateExportOptions(AbstractModel* _model, ExportOptions& _exportOptions) const override;

    /**
     * @brief Нарисовать декорацию блока
     */
    void printBlockDecorations(QPainter* _painter, qreal _pageYPos, const QRectF& _body,
                               TextParagraphType _paragraphType, const QRectF& _blockRect,
                               const QTextBlock& _block,
                               const ExportOptions& _exportOptions) const override;
};

} // namespace BusinessLayer
