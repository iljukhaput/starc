#pragma once

#include "QtCore/qbasictimer.h"
#include "QtCore/qpropertyanimation.h"
#include "QtCore/qurl.h"
#include "QtGui/qabstracttextdocumentlayout.h"
#include "QtGui/qtextcursor.h"
#include "QtGui/qtextdocumentfragment.h"
#include "QtGui/qtextformat.h"
#include "QtWidgets/qmenu.h"
#include "abstract_scroll_area_p.h"
#include "page_metrics.h"
#include "page_text_edit.h"
#include "page_text_edit_scroll_bar.h"
#include "private/qwidgettextcontrol_p.h"

class QMimeData;
class PageTextEditPrivate : public AbstractScrollAreaPrivate
{
    Q_DECLARE_PUBLIC(PageTextEdit)
public:
    PageTextEditPrivate();

    void init(const QString& html = QString());
    void paint(QPainter* p, QPaintEvent* e);
    void _q_repaintContents(const QRectF& contentsRect);

    inline QPoint mapToContents(const QPoint& point) const
    {
        return QPoint(point.x() + horizontalOffset(), point.y() + verticalOffset());
    }

    void _q_adjustScrollbars();
    void _q_ensureVisible(const QRectF& rect);
    void relayoutDocument();

    void createAutoBulletList();
    void pageUpDown(QTextCursor::MoveOperation op, QTextCursor::MoveMode moveMode);

    inline int horizontalOffset() const
    {
        return q_func()->isRightToLeft() ? (hbar->maximum() - hbar->value()) : hbar->value();
    }
    inline int verticalOffset() const
    {
        return vbar->value();
    }

    inline void sendControlEvent(QEvent* e)
    {
        control->processEvent(e, QPointF(horizontalOffset(), verticalOffset()), viewport);
    }

    void _q_currentCharFormatChanged(const QTextCharFormat& format);
    void _q_cursorPositionChanged();
    void _q_hoveredBlockWithMarkerChanged(const QTextBlock& block);

    void updateDefaultTextOption();

    // re-implemented by QTextBrowser, called by QTextDocument::loadResource
    virtual QUrl resolveUrl(const QUrl& url) const
    {
        return url;
    }

    QWidgetTextControl* control;

    PageTextEdit::AutoFormatting autoFormatting;
    bool tabChangesFocus;

    QBasicTimer autoScrollTimer;
    QPoint autoScrollDragPos;

    PageTextEdit::LineWrapMode lineWrap;
    int lineWrapColumnOrWidth;
    QTextOption::WrapMode wordWrap;

    uint ignoreAutomaticScrollbarAdjustment : 1;
    uint preferRichText : 1;
    uint showCursorOnInitialShow : 1;
    uint inDrag : 1;
    uint clickCausedFocus : 1;

    // Qt3 COMPAT only, for setText
    Qt::TextFormat textFormat;

    QString anchorToScrollToWhenVisible;

    QString placeholderText;

    Qt::CursorShape cursorToRestoreAfterHover = Qt::IBeamCursor;

#ifdef QT_KEYPAD_NAVIGATION
    QBasicTimer deleteAllTimer;
#endif


    //
    // Дополнения, необходимые для того, чтобы превратить простой QTextEdit в постраничный редактор
    //

    /**
     * @brief Обновить отступы вьювпорта
     */
    void updateViewportMargins();

    /**
     * @brief Обновить геометрию документа
     * @note Стандартная реализация QTextEdit такова, что она всё время сбрасывает установленный
     *		 размер документа, что приводит к нежелательным последствиям
     */
    void updateDocumentGeometry();

    /**
     * @brief Нарисовать оформление страниц документа
     */
    void paintPagesView(QPainter* _painter);

    /**
     * @brief Нарисовать содержимое полей страниц (номера и колонтитулы)
     */
    void paintPageMargins(QPainter* _painter);

    /**
     * @brief Нарисовать номер страницы с заданными парамтерами расположения
     */
    void paintPageNumber(QPainter* _painter, const QRectF& _rect, bool _isHeader, int _number);

    /**
     * @brief Нарисовать верхний колонтитул
     */
    void paintHeader(QPainter* _painter, const QRectF& _rect);

    /**
     * @brief Нарисовать нижний колонтитул
     */
    void paintFooter(QPainter* _painter, const QRectF& _rect);

    /**
     * @brief Нарисовать подсветку строки
     */
    void paintLineHighlighting(QPainter* _painter);

    /**
     * @brief Затемнить текст за пределами текущего абзаца
     */
    void paintTextBlocksOverlay(QPainter* _painter);

    /**
     * @brief Установить область обрезки так, чтобы вырезалось всё, что выходит на поля страницы
     */
    void clipPageDecorationRegions(QPainter* _painter);

    /**
     * @brief Скорректировать положение блока с курсором для режима печатной машинки
     */
    void updateBlockWithCursorPlacement();
    void prepareBlockWithCursorPlacementUpdate(QKeyEvent* _event);


    /**
     * @brief Включена ли возможность выделения текста
     */
    bool textSelectionEnabled = true;

    /**
     * @brief Режим отображения текста
     *
     * true - постраничный
     * false - сплошной
     */
    bool usePageMode = false;

    /**
     * @brief Необходимо ли добавлять пространство снизу в обычном режиме
     */
    bool addBottomSpace = false;

    /**
     * @brief Необходимо ли показывать номера страниц
     */
    bool showPageNumbers = false;
    bool showPageNumberAtFirstPage = true;

    /**
     * @brief Где показывать номера страниц
     */
    Qt::Alignment pageNumbersAlignment;

    /**
     * @brief Метрика страницы редактора
     */
    PageMetrics pageMetrics;

    /**
     * @brief Расстояние между страницами
     */
    qreal pageSpacing = 10.0;

    /**
     * @brief Колонтитулы
     */
    QString header;
    QString footer;

    /**
     * @brief Подсвечивать ли текущую строку
     */
    bool highlightCurrentLine = false;

    /**
     * @brief Фокусировать ли текущий абзац
     */
    bool focusCurrentParagraph = false;

    /**
     * @brief Использовать ли прокрутку в стиле печатной машинки
     */
    bool useTypewriterScrolling = false;
    bool needUpdateBlockWithCursorPlacement = false;

    /**
     * @brief Анимация скроллирования
     */
    QPropertyAnimation scrollAnimation;

    //
    // Дополнения для корректной работы с мышью при наличии невидимых текстовых блоков в документе
    //
public:
    /**
     * @brief Отправить скорректированное событие о взаимодействии мышью
     */
    /** @{ */
    void sendControlMouseEvent(QMouseEvent* e);
    void sendControlContextMenuEvent(QContextMenuEvent* e);
    /** @} */

private:
    /**
     * @brief Скорректировать позицию мыши
     * @note Используется в событиях связанных с мышью из-за того, что при наличии невидимых блоков
     *		 в документе, стандартная реализация иногда скачет сильно вниз
     */
    QPoint correctMousePosition(const QPoint& _eventPos) const;
};
