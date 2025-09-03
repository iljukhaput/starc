#include "comic_book_text_corrector.h"

#include <business_layer/document/text/text_block_data.h>
#include <business_layer/document/text/text_cursor.h>
#include <business_layer/model/comic_book/text/comic_book_text_block_parser.h>
#include <business_layer/model/comic_book/text/comic_book_text_model_page_item.h>
#include <business_layer/model/comic_book/text/comic_book_text_model_panel_item.h>
#include <business_layer/model/text/text_model_text_item.h>
#include <business_layer/templates/comic_book_template.h>
#include <business_layer/templates/templates_facade.h>
#include <ui/widgets/text_edit/page/page_text_edit.h>
#include <utils/shugar.h>
#include <utils/tools/run_once.h>

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextDocument>

#include <cmath>

using BusinessLayer::ComicBookCharacterParser;
using BusinessLayer::TemplatesFacade;
using BusinessLayer::TextBlockStyle;
using BusinessLayer::TextParagraphType;


namespace BusinessLayer {

namespace {
/**
 * @brief Список символов пунктуации, разделяющие предложения
 */
const QRegularExpression kPunctuationCharacter("([.]|[!]|[?]|[:]|[;]|[…])[^\"']");

/**
 * @brief Автоматически добавляемые продолжения в диалогах
 */
//: Continued
static const char* kContinuedTerm
    = QT_TRANSLATE_NOOP("BusinessLogic::ScriptTextCorrector", "CONT'D");

/**
 * @brief Автоматически добавляемые продолжения в диалогах
 */
static const char* MORE = QT_TRANSLATE_NOOP("BusinessLogic::ScriptTextCorrector", "MORE");
static const QString moreTerm()
{
    return QString("(%1)").arg(QApplication::translate("BusinessLogic::ScriptTextCorrector", MORE));
}

/**
 * @brief Обновить компановку текста для блока
 */
void updateBlockLayout(qreal _pageWidth, const QTextBlock& _block)
{
    _block.layout()->setText(_block.text());
    _block.layout()->beginLayout();
    forever
    {
        QTextLine line = _block.layout()->createLine();
        if (!line.isValid()) {
            break;
        }

        line.setLineWidth(_pageWidth - _block.blockFormat().leftMargin()
                          - _block.blockFormat().rightMargin());
    }
    _block.layout()->endLayout();
}

/**
 * @brief Вставить блок, обновив при этом лэйаут вставленного блока
 */
void insertBlock(int _pageWidth, QTextCursor& _cursor)
{
    _cursor.insertBlock();
    updateBlockLayout(_pageWidth, _cursor.block());
}
} // namespace


class ComicBookTextCorrector::Implementation
{
public:
    explicit Implementation(ComicBookTextCorrector* _q);

    /**
     * @brief Корректируемый документ
     */
    QTextDocument* document() const;

    /**
     * @brief Обновить видимость блоков в заданном интервале
     */
    void updateBlocksVisibility(int _from, int _charsChanged);

    /**
     * @brief Скорректировать имена персонажей
     */
    void correctCharactersNames(int _position = -1, int _charsChanged = 0);

    /**
     * @brief Скорректировать номера блоков
     */
    void correctBlocksNumbers(int _position = -1, int _charsChanged = 0);

    /**
     * @brief Скорректировать текст сценария
     */
    void correctPageBreaks(int _position = -1);

    //
    // Функции работающие в рамках текущей коррекции
    //

    /**
     * @brief Сместить текущий блок вместе с тремя предыдущими на следующую страницу
     */
    void moveCurrentBlockWithThreePreviousToNextPage(const QTextBlock& _prePrePreviousBlock,
                                                     const QTextBlock& _prePreviousBlock,
                                                     const QTextBlock& _previousBlock,
                                                     qreal _pageHeight, qreal _pageWidth,
                                                     TextCursor& _cursor, QTextBlock& _block,
                                                     qreal& _lastBlockHeight);

    /**
     * @brief Сместить текущий блок вместе с двумя предыдущими на следующую страницу
     */
    void moveCurrentBlockWithTwoPreviousToNextPage(const QTextBlock& _prePreviousBlock,
                                                   const QTextBlock& _previousBlock,
                                                   qreal _pageHeight, qreal _pageWidth,
                                                   TextCursor& _cursor, QTextBlock& _block,
                                                   qreal& _lastBlockHeight);

    /**
     * @brief Сместить текущий блок вместе с предыдущим на следующую страницу
     */
    void moveCurrentBlockWithPreviousToNextPage(const QTextBlock& _previousBlock, qreal _pageHeight,
                                                qreal _pageWidth, TextCursor& _cursor,
                                                QTextBlock& _block, qreal& _lastBlockHeight);

    /**
     * @brief Сместить текущий блок на следующую страницу
     */
    void moveCurrentBlockToNextPage(const QTextBlockFormat& _blockFormat, qreal _blockHeight,
                                    qreal _pageHeight, qreal _pageWidth, TextCursor& _cursor,
                                    QTextBlock& _block, qreal& _lastBlockHeight);

    /**
     * @brief Разорвать блок диалога
     */
    void breakDialogue(const QTextBlockFormat& _blockFormat, qreal _blockHeight, qreal _pageHeight,
                       qreal _pageWidth, TextCursor& _cursor, QTextBlock& _block,
                       qreal& _lastBlockHeight);

    //
    // Вспомогательные функции
    //

    /**
     * @brief Найти предыдущий блок
     */
    QTextBlock findPreviousBlock(const QTextBlock& _block);

    /**
     * @brief Найти следующий блок, который не является декорацией
     */
    QTextBlock findNextBlock(const QTextBlock& _block);

    /**
     * @brief Сместить блок в начало следующей страницы
     * @param _block - блок для смещения
     * @param _spaceToPageEnd - количество места до конца страницы
     * @param _pageHeight - высота страницы
     * @param _cursor - курсор редактироуемого документа
     */
    void moveBlockToNextPage(const QTextBlock& _block, qreal _spaceToPageEnd, qreal _pageHeight,
                             qreal _pageWidth, TextCursor& _cursor);

    //
    // Данные
    //

    /**
     * @brief Документ который будем корректировать
     */
    ComicBookTextCorrector* q = nullptr;

    /**
     * @brief Необходимо ли корректировать текст блоков имён персонажей
     */
    bool needToCorrectCharactersNames = true;

    /**
     * @brief Необходимо ли добавлять номера репликам персонажей
     */
    bool needToShowDialogueNumber = true;

    /**
     * @brief Нужно ли делать заголовок пнели жирным
     */
    bool needToBoldPanelTitle = true;

    /**
     * @brief Необходимо ли корректировать текст на разрывах страниц
     */
    bool needToCorrectPageBreaks = true;

    /**
     * @brief Размер документа при последней проверке
     */
    QSizeF lastDocumentSize;

    /**
     * @brief Вспомогательная информация о текущем блоке
     */
    struct {
        /**
         * @brief Номер текущего блока при корректировке
         * @note Используем собственный счётчик номеров, т.к. во время
         *       коррекций номера блоков могут скакать в QTextBlock
         */
        int number = 0;

        /**
         * @brief Находится ли блок в таблице
         */
        bool inTable = false;

        /**
         * @brief Находится ли блок в первой колонке таблицы
         */
        bool inFirstColumn = false;

        /**
         * @brief Положение верхней и нижней границ таблицы
         */
        qreal tableTop = std::numeric_limits<qreal>::min();
        qreal tableBottom = std::numeric_limits<qreal>::min();
    } currentBlockInfo;

    /**
     * @brief Структура элемента модели блоков
     */
    struct BlockInfo {
        BlockInfo() = default;
        explicit BlockInfo(qreal _height, qreal _top)
            : height(_height)
            , top(_top)
        {
        }

        /**
         * @brief Валиден ли блок
         */
        bool isValid() const
        {
            return !std::isnan(height) && !std::isnan(top);
        }

        /**
         * @brief Высота блока
         */
        qreal height = std::numeric_limits<qreal>::quiet_NaN();

        /**
         * @brief Позиция блока от начала страницы
         */
        qreal top = std::numeric_limits<qreal>::quiet_NaN();
    };

    /**
     * @brief Модель параметров блоков
     */
    QVector<BlockInfo> blockItems;
};

ComicBookTextCorrector::Implementation::Implementation(ComicBookTextCorrector* _q)
    : q(_q)
{
}

QTextDocument* ComicBookTextCorrector::Implementation::document() const
{
    return q->document();
}

void ComicBookTextCorrector::Implementation::updateBlocksVisibility(int _from, int _charsChanged)
{
    //
    // Пробегаем документ и настраиваем видимые и невидимые блоки в соответствии с шаблоном
    //
    const auto& currentTemplate = TemplatesFacade::comicBookTemplate(q->templateId());
    const auto startPosition = std::max(0, _from);
    const auto endPosition
        = _charsChanged > 0 ? (startPosition + _charsChanged) : document()->characterCount();
    bool isTextChanged = false;

    TextCursor cursor(document());
    cursor.setPosition(startPosition);
    bool isFirstVisibleBlock = cursor.block() == document()->begin();
    bool isFirstBlockAfterInvisible = true;
    auto block = cursor.block();
    while (block.isValid() && block.position() < endPosition) {
        const auto blockType = TextBlockStyle::forBlock(block);

        //
        // В некоторых случаях, мы попадаем сюда, когда документ не до конца настроен, поэтому
        // когда обнаруживается такая ситация, завершаем выполнение
        //
        if (blockType == TextParagraphType::Undefined) {
            break;
        }

        //
        // При необходимости корректируем видимость блока
        //
        const auto isBlockShouldBeVisible = [this, block] {
            //
            // Если не задан верхнеуровневый видимый элемент, то покажем
            //
            if (q->visibleTopLevelItem() == nullptr) {
                return true;
            }

            //
            // Если верхнеуровневый элемент задан и текущий элемент не является его дитём, то скроем
            //
            if (auto screenplayBlockData
                = static_cast<BusinessLayer::TextBlockData*>(block.userData());
                screenplayBlockData == nullptr || screenplayBlockData->item() == nullptr
                || !screenplayBlockData->item()->isChildOf(q->visibleTopLevelItem())) {
                return false;
            }

            //
            // А если является дитём, то покажем
            //
            return true;
        }();

        cursor.setPosition(block.position());
        auto blockFormat = cursor.blockFormat();
        const auto& paragraphStyleBlockFormat
            = currentTemplate.paragraphStyle(TextBlockStyle::forBlock(block))
                  .blockFormat(cursor.inTable());

        //
        // Корректируем параметры в кейсах
        // - сменилась видимость блока
        // - это первый видимый блок (у него не должно быть дополнительных отступов сверху)
        // - это первый блок который шёл после невидимых (он был первым видимым в предыдущем проходе
        //   и поэтому у него сброшены отступы)
        // - это не первый видимый блок и его высота отличается от канонической
        //
        if (block.isVisible() != isBlockShouldBeVisible
            || (isBlockShouldBeVisible && isFirstVisibleBlock)
            || (block.isVisible() && isBlockShouldBeVisible && isFirstBlockAfterInvisible)
            || (block.isVisible() && !isFirstVisibleBlock
                && !qFuzzyCompare(blockFormat.topMargin(),
                                  paragraphStyleBlockFormat.topMargin()))) {
            //
            // ... если это кейс с обновлением формата первого блока следующего за невидимым,
            //     то запомним, что мы его выполнили
            //
            if (block.isVisible() && isBlockShouldBeVisible && isFirstBlockAfterInvisible) {
                isFirstBlockAfterInvisible = false;
            }
            //
            // ... если блоку нужно настроить видимость, запустим операцию изменения
            //
            if (isTextChanged == false) {
                isTextChanged = true;
                cursor.beginEditBlock();
            }
            //
            // ... уберём отступы у скрытых блоков, чтобы они не ломали компоновку документа
            //
            if (!isBlockShouldBeVisible) {
                blockFormat.setTopMargin(0);
                blockFormat.setBottomMargin(0);
                blockFormat.setPageBreakPolicy(QTextFormat::PageBreak_Auto);
            }
            //
            // ... а для блоков, которые возвращаются для отображения, настроим отступы
            //
            else {
                blockFormat.setTopMargin(
                    (isFirstVisibleBlock
                     || blockFormat.boolProperty(TextBlockStyle::PropertyIsCorrectionContinued))
                        ? 0
                        : paragraphStyleBlockFormat.topMargin());
                blockFormat.setBottomMargin(paragraphStyleBlockFormat.bottomMargin());
                blockFormat.setPageBreakPolicy(isFirstVisibleBlock
                                                   ? QTextFormat::PageBreak_Auto
                                                   : paragraphStyleBlockFormat.pageBreakPolicy());
            }
            //
            // ... применим настроенный стиль блока
            //
            cursor.setBlockFormat(blockFormat);
            //
            // ... собственно настраиваем видимость
            //
            block.setVisible(isBlockShouldBeVisible);
        }

        if (isFirstVisibleBlock && isBlockShouldBeVisible) {
            isFirstVisibleBlock = false;
        }

        block = block.next();
    }

    if (isTextChanged) {
        cursor.endEditBlock();
    }
}

void ComicBookTextCorrector::Implementation::correctCharactersNames(int _position,
                                                                    int _charsChanged)
{
    //
    // Определим границы работы алгоритма
    //
    int startPosition = _position;
    int endPosition = _position + _charsChanged;
    if (startPosition == -1) {
        startPosition = 0;
        endPosition = document()->characterCount();
    }

    //
    // Начинаем работу с документом
    //
    TextCursor cursor(document());
    cursor.beginEditBlock();

    //
    // Расширим выделение
    //
    // ... от начала сцены
    //
    QVector<TextParagraphType> sceneBorders
        = { TextParagraphType::PageHeading, TextParagraphType::PanelHeading,
            TextParagraphType::SequenceHeading, TextParagraphType::SequenceFooter };
    QTextBlock block = document()->findBlock(startPosition);
    while (block != document()->begin()) {
        const auto blockType = TextBlockStyle::forBlock(block);
        if (sceneBorders.contains(blockType)) {
            break;
        }

        block = block.previous();
    }
    //
    // ... и до конца
    //
    {
        QTextBlock endBlock = document()->findBlock(endPosition);
        while (endBlock.isValid() && endBlock != document()->end()) {
            const TextParagraphType blockType = TextBlockStyle::forBlock(endBlock);
            if (sceneBorders.contains(blockType)) {
                break;
            }

            endBlock = endBlock.next();
        }
        endPosition = endBlock.previous().position();
    }

    //
    // Корректируем имена пресонажей в изменённой части документа
    //
    QString lastCharacterName;
    do {
        const auto blockType = TextBlockStyle::forBlock(block);
        //
        // Если дошли до новой сцены, очищаем последнее найдённое имя персонажа
        //
        if (sceneBorders.contains(blockType)) {
            lastCharacterName.clear();
        }
        //
        // Корректируем имя персонажа при необходимости
        //
        else if (blockType == TextParagraphType::Character) {
            const QString characterName = ComicBookCharacterParser::name(block.text());
            const bool isStartPositionInBlock = block.position() < startPosition
                && block.position() + block.length() > startPosition;
            //
            // Если имя текущего персонажа не пусто и курсор не находится в редактируемом блоке
            //
            if (!characterName.isEmpty() && !isStartPositionInBlock) {
                //
                // Не второе подряд появление, удаляем из него вспомогательный текст, если есть
                //
                if (lastCharacterName.isEmpty() || characterName != lastCharacterName) {
                    QTextBlockFormat characterFormat = block.blockFormat();
                    if (characterFormat.boolProperty(
                            TextBlockStyle::PropertyIsCharacterContinued)) {
                        characterFormat.setProperty(TextBlockStyle::PropertyIsCharacterContinued,
                                                    false);
                        cursor.setPosition(block.position());
                        cursor.setBlockFormat(characterFormat);
                    }
                }
                //
                // Если второе подряд и ещё не настроено, добавляем вспомогательный текст
                //
                else if (characterName == lastCharacterName) {
                    const QString characterState
                        = ComicBookCharacterParser::extension(block.text());
                    QTextBlockFormat characterFormat = block.blockFormat();
                    if (characterState.isEmpty()
                        && !characterFormat.boolProperty(
                            TextBlockStyle::PropertyIsCharacterContinued)) {
                        characterFormat.setProperty(TextBlockStyle::PropertyIsCharacterContinued,
                                                    true);
                        cursor.setPosition(block.position());
                        cursor.setBlockFormat(characterFormat);
                    }
                }

                lastCharacterName = characterName;
            }
        }

        block = block.next();
    } while (block.isValid() && block.position() < endPosition);

    cursor.endEditBlock();
}

void ComicBookTextCorrector::Implementation::correctBlocksNumbers(int _position, int _charsChanged)
{
    //
    // Определим границы работы алгоритма
    //
    int startPosition = 0;
    int endPosition = document()->characterCount();

    //
    // Начинаем работу с документом
    //
    TextCursor cursor(document());
    cursor.beginEditBlock();

    //
    // Расширим выделение
    //
    // ... от начала сцены
    //
    QVector<TextParagraphType> sceneBorders
        = { TextParagraphType::PageHeading, TextParagraphType::PanelHeading,
            TextParagraphType::SequenceHeading, TextParagraphType::SequenceFooter };
    QTextBlock block = document()->findBlock(startPosition);
    while (block != document()->begin()) {
        const auto blockType = TextBlockStyle::forBlock(block);
        if (sceneBorders.contains(blockType)) {
            break;
        }

        block = block.previous();
    }

    //
    // Корректируем номера блоков в изменённой части документа
    //
    auto itemFromBlock = [](const QTextBlock& _block) -> TextModelItem* {
        if (_block.userData() == nullptr) {
            return {};
        }

        const auto blockData = static_cast<TextBlockData*>(_block.userData());
        if (blockData == nullptr) {
            return {};
        }

        return blockData->item();
    };
    const auto changeEnd = _position + _charsChanged;
    const auto isMultipleBlocksChanged
        = _charsChanged > 1 && document()->findBlock(_position) != document()->findBlock(changeEnd);
    do {
        const auto blockType = TextBlockStyle::forBlock(block);

        //
        // Пропускаем пустые блоки и блоки, которые редактируется в данный момент
        //
        const auto blockEndPosition = block.position() + block.text().length() + 1;
        if ( // пустые строки
            block.text().isEmpty()
            // строки, которые редактируются в данный момент, за исключением кейсов, когда
            // нажимается энтер/таб в конце строки и курсор переходит на следующую строку, а также
            // при вставке текста из буфера обмена - кода изменение затрагивает несколько блоков
            || (!isMultipleBlocksChanged
                && ((block.position() <= _position
                     && changeEnd <= blockEndPosition) // изменение внутри блока
                    || (_position <= block.position()
                        && blockEndPosition <= changeEnd))) // входит в блок слева
        ) {
            block = block.next();
            if (block.isValid()) {
                continue;
            }
            break;
        }

        const int blockTextSourceLength = block.text().length();

        switch (blockType) {
        case TextParagraphType::PageHeading: {
            const auto item = itemFromBlock(block);
            do {
                if (item == nullptr || item->parent() == nullptr
                    || item->parent()->type() != TextModelItemType::Group
                    || static_cast<TextModelGroupItem*>(item->parent())->groupType()
                        != TextGroupType::Page) {
                    break;
                }

                const auto pageItem = static_cast<ComicBookTextModelPageItem*>(item->parent());
                const auto sourcePageTitle = ComicBookPanelParser::panelTitle(block.text());
                auto newPageTitle = sourcePageTitle.trimmed();

                //
                // При необходимости обновляем номер страницы
                //
                if (pageItem->pageNumber().has_value()
                    && !newPageTitle.endsWith(pageItem->pageNumber()->text)) {
                    newPageTitle.remove(QRegularExpression("(\\d|-)*$"));
                    if (!newPageTitle.endsWith(' ')) {
                        newPageTitle.append(' ');
                    }
                    newPageTitle.append(pageItem->pageNumber()->text);

                    cursor.setPosition(block.position());
                    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor,
                                        sourcePageTitle.length());
                    cursor.insertText(newPageTitle);
                }
            }
            once;
            break;
        }

        case TextParagraphType::PanelHeading: {
            const auto item = itemFromBlock(block);
            do {
                if (item == nullptr || item->parent() == nullptr
                    || item->parent()->type() != TextModelItemType::Group
                    || static_cast<TextModelGroupItem*>(item->parent())->groupType()
                        != TextGroupType::Panel) {
                    break;
                }

                const auto panelItem = static_cast<ComicBookTextModelPanelItem*>(item->parent());
                const auto sourcePanelTitle = ComicBookPanelParser::panelTitle(block.text());
                auto newPanelTitle = sourcePanelTitle;

                //
                // При необходимости обновляем номер панели
                //
                if (panelItem->panelNumber().has_value()
                    && !sourcePanelTitle.trimmed().endsWith(panelItem->panelNumber()->text)) {
                    newPanelTitle.remove(QRegularExpression("(\\d|-)*$"));
                    if (!newPanelTitle.endsWith(' ')) {
                        newPanelTitle.append(' ');
                    }
                    newPanelTitle.append(panelItem->panelNumber()->text);

                    cursor.setPosition(block.position());
                    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor,
                                        sourcePanelTitle.length());
                    cursor.insertText(newPanelTitle);
                }

                //
                // Отформатируем заголовок панели жирным
                //
                if (needToBoldPanelTitle) {
                    cursor.setPosition(block.position());
                    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor,
                                        newPanelTitle.length());
                    //
                    // Если не в конце, то подвинем ещё на один символ, чтобы захватить двоеточие
                    //
                    if (!cursor.atBlockEnd()) {
                        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
                    }
                    auto charFormat = cursor.charFormat();
                    if (charFormat.fontWeight() != QFont::Bold) {
                        charFormat.setFontWeight(QFont::Bold);
                        cursor.mergeCharFormat(charFormat);
                    }
                }

                //
                // Если это абзац с одним только заголовком, то добавим пробел с форматированием,
                // как в блоке
                //
                cursor.clearSelection();
                if (cursor.atBlockEnd()) {
                    cursor.insertText(" ", cursor.blockCharFormat());
                }
            }
            once;
            break;
        }

        case TextParagraphType::Character: {
            const auto item = itemFromBlock(block);
            do {
                if (item == nullptr || item->type() != TextModelItemType::Text) {
                    break;
                }

                const auto textItem = static_cast<TextModelTextItem*>(item);
                if (textItem->paragraphType() != TextParagraphType::Character) {
                    break;
                }

                if (!textItem->number().has_value()) {
                    break;
                }

                //
                // При необходимости обновляем номер диалога
                //
                const auto sourceDialogueNumber = ComicBookCharacterParser::number(block.text());
                const QString decoration = ". ";
                const auto textItemNumber = needToShowDialogueNumber
                    ? QString("%1%2").arg(textItem->number()->value).arg(decoration)
                    : "";
                if (sourceDialogueNumber != textItemNumber) {
                    cursor.setPosition(block.position());
                    if (!sourceDialogueNumber.isEmpty()) {
                        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor,
                                            sourceDialogueNumber.length() + decoration.length());
                    }
                    cursor.insertText(textItemNumber);
                }
            }
            once;
            break;
        }

        default: {
            break;
        }
        }

        //
        // Если длина текста изменилась, корректируем конечную позицию, чтобы учесть изменение
        //
        const int blockTextLength = block.text().length();
        if (blockTextLength != blockTextSourceLength) {
            endPosition += blockTextLength - blockTextSourceLength;
        }

        block = block.next();
    } while (block.isValid() && block.position() < endPosition);

    cursor.endEditBlock();
}

void ComicBookTextCorrector::Implementation::correctPageBreaks(int _position)
{
    return;

    //
    // Определим высоту страницы
    //
    const QTextFrameFormat rootFrameFormat = document()->rootFrame()->frameFormat();
    //
    const qreal pageWidth = document()->pageSize().width() - rootFrameFormat.leftMargin()
        - rootFrameFormat.rightMargin();
    if (pageWidth < 0) {
        return;
    }
    qreal leftHalfWidth = 0.0;
    qreal rightHalfWidth = 0.0;
    {
        const auto currentTemplate = TemplatesFacade::comicBookTemplate(q->templateId());
        leftHalfWidth = pageWidth * currentTemplate.leftHalfOfPageWidthPercents() / 100.0
            - currentTemplate.pageSplitterWidth() / 2;
        rightHalfWidth = pageWidth - leftHalfWidth - currentTemplate.pageSplitterWidth();
    }
    auto currentBlockWidth = [this, pageWidth, leftHalfWidth, rightHalfWidth] {
        if (!currentBlockInfo.inTable) {
            return pageWidth;
        }

        if (currentBlockInfo.inFirstColumn) {
            return leftHalfWidth;
        }

        return rightHalfWidth;
    };
    //
    const qreal pageHeight = document()->pageSize().height() - rootFrameFormat.topMargin()
        - rootFrameFormat.bottomMargin();
    if (pageHeight < 0) {
        return;
    }
    //
    // Если размер изменился, необходимо пересчитать все блоки
    //
    if (!qFuzzyCompare(lastDocumentSize.width(), pageWidth)
        || !qFuzzyCompare(lastDocumentSize.height(), pageHeight)) {
        blockItems.clear();
        lastDocumentSize = QSizeF(pageWidth, pageHeight);
    }

    //
    // При необходимости скорректируем размер модели параметров блоков для дальнейшего использования
    //
    {
        //
        // ... для сравнения, используем минимальный запас в 10 процентов + 200 дополнительных
        //     абзацев для обработки переносов, если некоторые абзацы длинее одной страницы
        //
        const int blocksCount = document()->blockCount() * 1.1 + 200;
        if (blockItems.size() <= blocksCount) {
            blockItems.resize(blocksCount * 2);
        }
    }

    //
    // Определим список блоков для принудительной ручной проверки для случая, когда пользователь
    // редактирует текст на границе страниц в разорванном блоке. Это необходимо для того,
    // чтобы корректно обрабатывать изменение текста в предыдущих и следующих за переносом блоках
    //
    if (_position != -1) {
        auto block = document()->findBlock(_position);
        if (block.blockFormat().boolProperty(TextBlockStyle::PropertyIsBreakCorrectionStart)
            || block.blockFormat().boolProperty(TextBlockStyle::PropertyIsBreakCorrectionEnd)) {
            //
            // ... два вперёд и два назад, включая текущий получается 5
            //
            auto replies = 3;
            for (; replies < 5; ++replies) {
                if (!block.next().isValid()) {
                    break;
                }
                block = block.next();
            }
            //
            // ... собственно сбрасываем рассчитанные параметры блоков для перепроверки
            //
            do {
                blockItems[block.blockNumber()] = {};
                block = block.previous();
            } while (block.isValid()
                     && (replies-- > 0
                         || block.blockFormat().boolProperty(TextBlockStyle::PropertyIsCorrection)
                         || block.blockFormat().boolProperty(
                             TextBlockStyle::PropertyIsBreakCorrectionStart)
                         || block.blockFormat().boolProperty(
                             TextBlockStyle::PropertyIsBreakCorrectionEnd)));
        }
    }

    //
    // Начинаем работу с документом
    //
    TextCursor cursor(document());
    cursor.beginEditBlock();

    //
    // Идём по каждому блоку документа с самого начала
    //
    QTextBlock block = document()->begin();
    //
    // ... значение нижней позиции последнего блока относительно начала страницы
    //
    qreal lastBlockHeight = 0.0;
    currentBlockInfo = {};
    bool isFirstChangedBlock = true;
    while (block.isValid()) {
        //
        // Запомним самый нижний блок, когда находимся в таблице
        //
        if (currentBlockInfo.inTable) {
            currentBlockInfo.tableBottom = std::max(currentBlockInfo.tableBottom, lastBlockHeight);
        }

        //
        // Если вошли в таблицу, или вышли из неё
        //
        if (TextBlockStyle::forBlock(block) == TextParagraphType::PageSplitter) {
            if (!currentBlockInfo.inTable) {
                currentBlockInfo.inTable = true;
                currentBlockInfo.inFirstColumn = true;
                currentBlockInfo.tableTop = lastBlockHeight;
            }
            //
            // При выходе из таблицы восстанавливаем значение высоты последнего блока
            // и очищаем параметры относящиеся к таблице
            //
            else {
                lastBlockHeight = currentBlockInfo.tableBottom;
                currentBlockInfo.inTable = false;
                currentBlockInfo.inFirstColumn = false;
                currentBlockInfo.tableTop = std::numeric_limits<qreal>::min();
                currentBlockInfo.tableBottom = std::numeric_limits<qreal>::min();
            }

            blockItems[currentBlockInfo.number] = {};

            block = block.next();
            ++currentBlockInfo.number;
            continue;
        }

        //
        // Если в первой колонке внутри таблицы
        //
        if (currentBlockInfo.inTable && currentBlockInfo.inFirstColumn) {
            //
            // Когда перешли во вторую колонку, восстановим высоту последнего блока и запомним
            // смещение
            //
            cursor.setPosition(block.position());
            if (!cursor.inFirstColumn()) {
                currentBlockInfo.inFirstColumn = false;
                lastBlockHeight = currentBlockInfo.tableTop;
            }
        }

        //
        // Для блока, который всегда находится в начале страницы очищаем информацию
        // о высоте предыдущего блока, какой бы она ни была
        //
        if (block.blockFormat().pageBreakPolicy() == QTextFormat::PageBreak_AlwaysBefore) {
            lastBlockHeight = 0;
        }

        //
        // Определить высоту текущего блока
        //
        const QTextBlockFormat blockFormat = block.blockFormat();
        const qreal blockLineHeight = blockFormat.lineHeight();
        const int blockLineCount = block.layout()->lineCount();
        //
        // ... если блок первый на странице, то для него не нужно учитывать верхний отступ
        //
        const qreal blockHeight = qFuzzyCompare(lastBlockHeight, 0.0)
            ? blockLineHeight * blockLineCount + blockFormat.bottomMargin()
            : blockLineHeight * blockLineCount + blockFormat.topMargin()
                + blockFormat.bottomMargin();
        //
        // ... прерываем обработку в случае
        //     - если блок ну ни как не должен быть больше одной страницы размером
        //     - если блок настолько огромный, что не влезает на три страницы
        //
        const QSet<TextParagraphType> blockWhichCantBeWiderThenPage = {
            TextParagraphType::PageHeading,
            TextParagraphType::PanelHeading,
            TextParagraphType::Shot,
            TextParagraphType::ActHeading,
            TextParagraphType::SequenceHeading,
            TextParagraphType::ActFooter,
            TextParagraphType::SequenceFooter,
            TextParagraphType::SceneCharacters,
            TextParagraphType::BeatHeading,
            TextParagraphType::Character,
            TextParagraphType::Parenthetical,

        };
        if ((blockHeight >= pageHeight
             && blockWhichCantBeWiderThenPage.contains(TextBlockStyle::forBlock(block)))
            || blockHeight >= pageHeight * 3) {
            return;
        }
        //
        // ... и высоту одной строки следующего
        //
        qreal nextBlockOneLineHeight = 0;
        if (block.next().isValid()) {
            const qreal nextBlockLineHeight = block.next().blockFormat().lineHeight();
            const QTextBlockFormat nextBlockFormat = block.next().blockFormat();
            nextBlockOneLineHeight = nextBlockLineHeight + nextBlockFormat.topMargin();
        }


        //
        // Определим, заканчивается ли блок на последней строке страницы
        //
        const bool atPageEnd =
            // сам блок влезает
            (lastBlockHeight + blockHeight <= pageHeight)
            // и
            && (
                // добавление одной строки перекинет его уже на новую страницу
                lastBlockHeight + blockHeight + blockLineHeight > pageHeight
                // или 1 строка следующего блока уже не влезет на эту страницу
                || lastBlockHeight + blockHeight + nextBlockOneLineHeight > pageHeight);
        //
        // ... или, может попадает на разрыв
        //
        const bool atPageBreak =
            // сам блок не влезает
            (lastBlockHeight + blockHeight > pageHeight)
            // но влезает хотя бы одна строка
            && (lastBlockHeight + blockFormat.topMargin() + blockLineHeight < pageHeight);

        //
        // Проверяем, изменилась ли позиция блока,
        // и что текущий блок это не изменённый блок под курсором
        // и что текущий блок это не пустая декорация в начале страницы
        //
        const bool isBlockEmptyDecorationOnTopOfThePage
            = blockFormat.boolProperty(TextBlockStyle::PropertyIsCorrection)
            && block.text().isEmpty()
            && qFuzzyCompare(blockItems[currentBlockInfo.number].top, 0.0);
        if (blockItems[currentBlockInfo.number].isValid() && !isBlockEmptyDecorationOnTopOfThePage
            && qFuzzyCompare(blockItems[currentBlockInfo.number].height, blockHeight)
            && qFuzzyCompare(blockItems[currentBlockInfo.number].top, lastBlockHeight)) {
            //
            // Если не изменилась
            //
            // ... в данном случае блок не может быть на разрыве
            //
            Q_ASSERT_X(atPageBreak == false, Q_FUNC_INFO,
                       "Normally cached blocks can't be placed on page breaks");
            //
            // ... то корректируем позицию
            //
            if (atPageEnd) {
                lastBlockHeight = 0;
            } else if (atPageBreak) {
                lastBlockHeight += blockHeight - pageHeight;
            } else {
                lastBlockHeight += blockHeight;
            }
            //
            // ... и переходим к следующему блоку
            //
            block = block.next();
            ++currentBlockInfo.number;
            continue;
        }


        //
        // Если позиция блока изменилась, то работаем по алгоритму корректировки текста
        //


        //
        // Если это первый из блоков, в котором сменилось расположение или высота, проверяем также
        // ближайшие блоки, чтобы корректно обработать ситуацию, когда в блоке удалили текст
        // и теперь он может быть помещён на предыдущей странице
        //
        if (isFirstChangedBlock && blockItems[currentBlockInfo.number].isValid()) {
            isFirstChangedBlock = false;
            int topIndex = currentBlockInfo.number;
            const auto maxDecorationBlocks = 2;
            for (int i = 0; i < maxDecorationBlocks; ++i) {
                if (topIndex == 0) {
                    lastBlockHeight = 0;
                    break;
                }

                //
                // Контролируем, чтобы расположение блоков не переходило через границу таблицы
                //
                if (TextBlockStyle::forBlock(block.previous()) == TextParagraphType::PageSplitter) {
                    break;
                }

                block = block.previous();
                --topIndex;
                lastBlockHeight = blockItems[topIndex].top;
            }
            currentBlockInfo.number = topIndex;

            const int bottomIndex = currentBlockInfo.number + maxDecorationBlocks;
            for (int index = topIndex; index <= bottomIndex; ++index) {
                if (index >= blockItems.size()) {
                    break;
                }

                blockItems[index] = {};
            }

            continue;
        }


        //
        // Работаем с декорациями
        //
        // ... если блок декорация, то удаляем его
        //
        if (blockFormat.boolProperty(TextBlockStyle::PropertyIsCorrection)) {
            blockItems[currentBlockInfo.number] = {};
            cursor.setPosition(block.position());
            if (cursor.block().next() != cursor.document()->end()) {
                cursor.movePosition(TextCursor::EndOfBlock, TextCursor::KeepAnchor);
                cursor.movePosition(TextCursor::NextCharacter, TextCursor::KeepAnchor);
            } else {
                cursor.movePosition(TextCursor::PreviousCharacter);
                cursor.movePosition(TextCursor::NextBlock, TextCursor::KeepAnchor);
                cursor.movePosition(TextCursor::EndOfBlock, TextCursor::KeepAnchor);
            }
            if (cursor.hasSelection()) {
                cursor.deleteChar();
                //
                // ... если текст в получившемся блоке нет, то восстанавливаем его формат, т.к. в
                //     этом случае в блоке будет сохранятся форматирование удалённого блока
                //
                if (cursor.block().text().isEmpty()) {
                    const auto blockType = TextBlockStyle::forCursor(cursor);
                    const auto& blockStyle = TemplatesFacade::comicBookTemplate(q->templateId())
                                                 .paragraphStyle(blockType);
                    cursor.setBlockCharFormat(blockStyle.charFormat());
                }
            } else {
                QTextBlockFormat breakStartFormat = cursor.blockFormat();
                breakStartFormat.clearProperty(TextBlockStyle::PropertyIsCorrection);
                cursor.setBlockFormat(breakStartFormat);
            }
            //
            // ... и продолжим со следующего блока
            //
            block = cursor.block();
            continue;
        }
        //
        // ... если в текущем блоке есть разрыв, пробуем его вернуть
        //
        else if (blockFormat.boolProperty(TextBlockStyle::PropertyIsBreakCorrectionStart)
                 || blockFormat.boolProperty(TextBlockStyle::PropertyIsBreakCorrectionEnd)) {
            cursor.setPosition(block.position());

            //
            // Если в конце разрыва, вернёмся к началу
            //
            if (blockFormat.boolProperty(TextBlockStyle::PropertyIsBreakCorrectionEnd)) {
                //
                // Началом может быть элемент с соответствующим флагом, либо простой элемент,
                // в случае, когда было удаление переноса строки между абзацем с переносом и
                // предыдущим
                //
                do {
                    blockItems[currentBlockInfo.number] = {};
                    cursor.movePosition(TextCursor::PreviousBlock);
                    --currentBlockInfo.number;
                    //
                    // ... восстанавливаем последнюю высоту от предыдущего элемента
                    //
                    lastBlockHeight = blockItems[currentBlockInfo.number].top;
                } while (cursor.blockFormat().boolProperty(TextBlockStyle::PropertyIsCorrection)
                         && !cursor.blockFormat().boolProperty(
                             TextBlockStyle::PropertyIsBreakCorrectionStart));
            }

            //
            // Начинаем склеивать разрыв
            //
            cursor.movePosition(TextCursor::EndOfBlock);
            do {
                cursor.movePosition(TextCursor::NextBlock, TextCursor::KeepAnchor);
            } while (cursor.blockFormat().boolProperty(TextBlockStyle::PropertyIsCorrection));
            //
            // ... если дошли до конца разрыва, то сшиваем его
            //
            if (cursor.blockFormat().boolProperty(TextBlockStyle::PropertyIsBreakCorrectionEnd)) {
                cursor.insertText(" ");
            }
            //
            // ... а если после начала разрыва идёт другой блок
            //
            else {
                cursor.clearSelection();
                if (cursor.movePosition(QTextCursor::NextBlock)) {
                    //
                    // ... если последующий блок корректировка или конец разрыва, то это кейс,
                    //     когда был нажат энтер в блоке начала разрыва и нужно перенести флаг
                    //     разрыва в следующий за текущим блок
                    //
                    if (cursor.blockFormat().boolProperty(TextBlockStyle::PropertyIsCorrection)
                        || cursor.blockFormat().boolProperty(
                            TextBlockStyle::PropertyIsBreakCorrectionEnd)) {
                        cursor.movePosition(QTextCursor::PreviousBlock);
                        QTextBlockFormat breakStartFormat = cursor.blockFormat();
                        breakStartFormat.setProperty(TextBlockStyle::PropertyIsBreakCorrectionStart,
                                                     true);
                        cursor.setBlockFormat(breakStartFormat);
                    }
                    //
                    // ... в противном случае это был кейс объединения двух блоков и нужно просто
                    // убрать флаги разрыва
                    //
                    else {
                        cursor.movePosition(QTextCursor::PreviousBlock);
                    }

                    cursor.movePosition(TextCursor::PreviousBlock);
                }

                Q_ASSERT(cursor.blockFormat().boolProperty(
                    TextBlockStyle::PropertyIsBreakCorrectionStart));
            }
            //
            // ... очищаем значения обрывов
            //
            QTextBlockFormat cleanFormat = blockFormat;
            cleanFormat.clearProperty(PageTextEdit::PropertyDontShowCursor);
            cleanFormat.clearProperty(TextBlockStyle::PropertyIsBreakCorrectionStart);
            cleanFormat.clearProperty(TextBlockStyle::PropertyIsBreakCorrectionEnd);
            cursor.setBlockFormat(cleanFormat);
            //
            // ... и проработаем текущий блок с начала
            //
            block = cursor.block();
            updateBlockLayout(currentBlockWidth(), block);
            continue;
        }


        //
        // Работаем с переносами
        //
        if (needToCorrectPageBreaks && (atPageEnd || atPageBreak)) {
            switch (TextBlockStyle::forBlock(block)) {
            //
            // Если это время и место или начало папки
            //
            case TextParagraphType::PageHeading:
            case TextParagraphType::PanelHeading:
            case TextParagraphType::SequenceHeading: {
                //
                // Переносим на следующую страницу
                //
                moveCurrentBlockToNextPage(blockFormat, blockHeight, pageHeight,
                                           currentBlockWidth(), cursor, block, lastBlockHeight);

                break;
            }

            //
            // Конец папки распологаем либо только в конце страницы, либо целиком переносим на
            // следующую страницу
            //
            case TextParagraphType::SequenceFooter: {
                //
                // Если в конце страницы, оставляем как есть
                //
                if (atPageEnd) {
                    //
                    // Запоминаем параметры текущего блока
                    //
                    blockItems[currentBlockInfo.number++]
                        = BlockInfo{ blockHeight, lastBlockHeight };
                    //
                    // и идём дальше
                    //
                    lastBlockHeight = 0;
                }
                //
                // В противном случае, просто переносим блок на следующую страницу
                //
                else {
                    moveCurrentBlockToNextPage(blockFormat, blockHeight, pageHeight, pageWidth,
                                               cursor, block, lastBlockHeight);
                }

                break;
            }

            //
            // Если это имя персонажа
            //
            case TextParagraphType::Character: {
                //
                // Определим предыдущий блок
                //
                QTextBlock previousBlock = findPreviousBlock(block);
                //
                // Если перед ним идёт время и место, переносим его тоже
                //
                if (previousBlock.isValid()
                    && TextBlockStyle::forBlock(previousBlock) == TextParagraphType::PanelHeading) {
                    moveCurrentBlockWithPreviousToNextPage(previousBlock, pageHeight,
                                                           currentBlockWidth(), cursor, block,
                                                           lastBlockHeight);
                }
                //
                // В противном случае, просто переносим блок на следующую страницу
                //
                else {
                    moveCurrentBlockToNextPage(blockFormat, blockHeight, pageHeight,
                                               currentBlockWidth(), cursor, block, lastBlockHeight);
                }

                break;
            }


            //
            // Если это реплика или лирика и попадает на разрыв
            // - если можно, то оставляем текст так, чтобы он занимал не менее 2 строк,
            //	 добавляем ДАЛЬШЕ и на следующей странице имя персонажа с (ПРОД) и остальной текст
            // - в противном случае
            // -- если перед диалогом идёт имя персонажа, то переносим их вместе на след.
            // -- если перед диалогом идёт ремарка
            // --- если перед ремаркой идёт имя персонажа, то переносим их всех вместе
            // --- если перед ремаркой идёт диалог, то разрываем по ремарке, пишем вместо неё
            //	   ДАЛЬШЕ, а на следующей странице имя персонажа с (ПРОД), ремарку и сам диалог
            //
            case TextParagraphType::Dialogue: {
                //
                // Проверяем следующий блок
                //
                QTextBlock nextBlock = findNextBlock(block);
                //
                // Если реплика в конце страницы и после ней идёт не ремарка, то оставляем как есть
                //
                if (atPageEnd && (!nextBlock.isValid() || nextBlock.isValid())) {
                    //
                    // Запоминаем параметры текущего блока
                    //
                    blockItems[currentBlockInfo.number++]
                        = BlockInfo{ blockHeight, lastBlockHeight };
                    //
                    // и идём дальше
                    //
                    lastBlockHeight = 0;
                }
                //
                // В противном случае разрываем
                //
                else {
                    //
                    // Если влезает 2 или более строк
                    //
                    const int minPlacedLines = 2;
                    const int linesCanBePlaced
                        = (pageHeight - lastBlockHeight - blockFormat.topMargin())
                        / blockLineHeight;
                    int lineToBreak
                        = linesCanBePlaced - 1; // Резервируем одну строку, под блок (ДАЛЬШЕ)
                    //
                    // ... пробуем разорвать на максимально низкой строке
                    //
                    bool isBreaked = false;
                    while (lineToBreak >= minPlacedLines && !isBreaked) {
                        const QTextLine line = block.layout()->lineAt(
                            lineToBreak - 1); // -1 т.к. нужен индекс, а не порядковый номер
                        const QString lineText
                            = block.text().mid(line.textStart(), line.textLength());
                        const int punctuationIndex = lineText.lastIndexOf(kPunctuationCharacter);
                        //
                        // ... нашлось место, где можно разорвать
                        //
                        if (punctuationIndex != -1) {
                            //
                            // ... разрываем
                            //
                            // +1, т.к. символ пунктуации нужно оставить в текущем блоке
                            cursor.setPosition(block.position() + line.textStart()
                                               + punctuationIndex + 1);
                            insertBlock(currentBlockWidth(), cursor);
                            //
                            // ... запоминаем параметры блока оставшегося в конце страницы
                            //
                            const qreal breakStartBlockHeight = lineToBreak * blockLineHeight
                                + blockFormat.topMargin() + blockFormat.bottomMargin();
                            blockItems[currentBlockInfo.number++]
                                = BlockInfo{ breakStartBlockHeight, lastBlockHeight };
                            lastBlockHeight += breakStartBlockHeight;
                            //
                            // ... если после разрыва остался пробел, уберём его
                            //
                            if (cursor.block().text().startsWith(" ")) {
                                cursor.deleteChar();
                            }
                            //
                            // ... помечаем блоки, как разорванные
                            //
                            QTextBlockFormat breakStartFormat = blockFormat;
                            breakStartFormat.setProperty(
                                TextBlockStyle::PropertyIsBreakCorrectionStart, true);
                            cursor.movePosition(TextCursor::PreviousBlock);
                            cursor.setBlockFormat(breakStartFormat);
                            //
                            QTextBlockFormat breakEndFormat = blockFormat;
                            breakEndFormat.setProperty(TextBlockStyle::PropertyIsBreakCorrectionEnd,
                                                       true);
                            cursor.movePosition(TextCursor::NextBlock);
                            cursor.setBlockFormat(breakEndFormat);
                            //
                            // ... обновим лэйаут оторванного блока
                            //
                            block = cursor.block();
                            updateBlockLayout(currentBlockWidth(), block);
                            const qreal breakEndBlockHeight
                                = block.layout()->lineCount() * blockLineHeight
                                + blockFormat.topMargin() + blockFormat.bottomMargin();
                            //
                            // ... и декорируем разрыв диалога по правилам
                            //
                            breakDialogue(blockFormat, breakEndBlockHeight, pageHeight,
                                          currentBlockWidth(), cursor, block, lastBlockHeight);
                            //
                            // ... сохраняем оторванный конец блока и корректируем последнюю высоту
                            //
                            block = block.next();
                            blockItems[currentBlockInfo.number++]
                                = BlockInfo{ breakEndBlockHeight, lastBlockHeight };
                            lastBlockHeight += breakEndBlockHeight;
                            //
                            // ... помечаем, что разорвать удалось
                            //
                            isBreaked = true;
                            break;
                        }

                        //
                        // На текущей строке разорвать не удалось, перейдём к предыдущей
                        //
                        --lineToBreak;
                    }

                    //
                    // Разорвать не удалось, переносим целиком
                    //
                    if (!isBreaked) {
                        //
                        // Определим предыдущий блок
                        //
                        QTextBlock previousBlock = findPreviousBlock(block);
                        //
                        // Если перед ним идёт персонаж, переносим его тоже
                        //
                        if (previousBlock.isValid()
                            && TextBlockStyle::forBlock(previousBlock)
                                == TextParagraphType::Character) {
                            //
                            // Проверяем предыдущий блок
                            //
                            QTextBlock prePreviousBlock = findPreviousBlock(previousBlock);
                            //
                            // Если перед именем идёт время и место, переносим его тоже
                            //
                            if (prePreviousBlock.isValid()
                                && TextBlockStyle::forBlock(prePreviousBlock)
                                    == TextParagraphType::PanelHeading) {
                                moveCurrentBlockWithTwoPreviousToNextPage(
                                    prePreviousBlock, previousBlock, pageHeight,
                                    currentBlockWidth(), cursor, block, lastBlockHeight);
                            }
                            //
                            // В противном случае просто переносим вместе с персонажем
                            //
                            else {
                                moveCurrentBlockWithPreviousToNextPage(previousBlock, pageHeight,
                                                                       currentBlockWidth(), cursor,
                                                                       block, lastBlockHeight);
                            }
                        }
                        //
                        // В противном случае, просто переносим блок на следующую страницу
                        //
                        else {
                            moveCurrentBlockToNextPage(blockFormat, blockHeight, pageHeight,
                                                       currentBlockWidth(), cursor, block,
                                                       lastBlockHeight);
                        }
                    }
                }

                break;
            }

            //
            // Если это описание действия или любой другой блок, для которого нет собственных правил
            //
            default: {
                //
                // Если в конце страницы, оставляем как есть
                //
                if (atPageEnd) {
                    //
                    // Запоминаем параметры текущего блока
                    //
                    blockItems[currentBlockInfo.number++]
                        = BlockInfo{ blockHeight, lastBlockHeight };
                    //
                    // и идём дальше
                    //
                    lastBlockHeight = 0;
                }
                //
                // Если на разрыве между страниц
                //
                else {
                    //
                    // Если влезает 2 или более строк
                    //
                    const int minPlacedLines = 2;
                    const int linesCanBePlaced
                        = (pageHeight - lastBlockHeight - blockFormat.topMargin())
                        / blockLineHeight;
                    int lineToBreak = linesCanBePlaced;
                    //
                    // ... пробуем разорвать на максимально низкой строке
                    //
                    bool isBreaked = false;
                    while (lineToBreak >= minPlacedLines && !isBreaked) {
                        const QTextLine line = block.layout()->lineAt(
                            lineToBreak - 1); // -1 т.к. нужен индекс, а не порядковый номер
                        const QString lineText
                            = block.text().mid(line.textStart(), line.textLength());
                        const int punctuationIndex = lineText.lastIndexOf(kPunctuationCharacter);
                        //
                        // ... нашлось место, где можно разорвать
                        //
                        if (punctuationIndex != -1) {
                            //
                            // ... разрываем
                            //
                            // +1, т.к. символ пунктуации нужно оставить в текущем блоке
                            cursor.setPosition(block.position() + line.textStart()
                                               + punctuationIndex + 1);
                            insertBlock(currentBlockWidth(), cursor);
                            //
                            // ... запоминаем параметры блока оставшегося в конце страницы
                            //
                            const qreal breakStartBlockHeight = lineToBreak * blockLineHeight
                                + blockFormat.topMargin() + blockFormat.bottomMargin();
                            blockItems[currentBlockInfo.number++]
                                = BlockInfo{ breakStartBlockHeight, lastBlockHeight };
                            //
                            // ... если после разрыва остался пробел, уберём его
                            //
                            if (cursor.block().text().startsWith(" ")) {
                                cursor.deleteChar();
                            }
                            //
                            // ... помечаем блоки, как разорванные
                            //
                            QTextBlockFormat breakStartFormat = blockFormat;
                            breakStartFormat.setProperty(
                                TextBlockStyle::PropertyIsBreakCorrectionStart, true);
                            cursor.movePosition(TextCursor::PreviousBlock);
                            cursor.setBlockFormat(breakStartFormat);
                            //
                            QTextBlockFormat breakEndFormat = blockFormat;
                            breakEndFormat.setProperty(TextBlockStyle::PropertyIsBreakCorrectionEnd,
                                                       true);
                            cursor.movePosition(TextCursor::NextBlock);
                            cursor.setBlockFormat(breakEndFormat);
                            //
                            // ... переносим оторванный конец на следующую страницу,
                            //     если на текущую влезает ещё хотя бы одна строка текста
                            //
                            block = cursor.block();
                            const qreal sizeToPageEnd
                                = pageHeight - lastBlockHeight - breakStartBlockHeight;
                            if (sizeToPageEnd >= blockFormat.topMargin() + blockLineHeight) {
                                moveBlockToNextPage(block, sizeToPageEnd, pageHeight,
                                                    currentBlockWidth(), cursor);
                                block = cursor.block();
                            }
                            updateBlockLayout(currentBlockWidth(), block);
                            const qreal breakEndBlockHeight
                                = block.layout()->lineCount() * blockLineHeight
                                + blockFormat.bottomMargin();
                            //
                            // ... запоминаем параметры блока перенесённого на следующую страницу
                            //
                            blockItems[currentBlockInfo.number++]
                                = BlockInfo{ breakEndBlockHeight, 0 };
                            //
                            // ... обозначаем последнюю высоту
                            //
                            lastBlockHeight = breakEndBlockHeight;
                            //
                            // ... помечаем, что разорвать удалось
                            //
                            isBreaked = true;
                            break;
                        }

                        //
                        // На текущей строке разорвать не удалось, перейдём к предыдущей
                        //
                        --lineToBreak;
                    }

                    //
                    // Разорвать не удалось, переносим целиком
                    //
                    if (!isBreaked) {
                        //
                        // Определим предыдущий блок
                        //
                        QTextBlock previousBlock = findPreviousBlock(block);
                        //
                        // Если перед ним идёт время и место, переносим его тоже
                        //
                        if (previousBlock.isValid()
                            && TextBlockStyle::forBlock(previousBlock)
                                == TextParagraphType::PanelHeading) {
                            moveCurrentBlockWithPreviousToNextPage(previousBlock, pageHeight,
                                                                   currentBlockWidth(), cursor,
                                                                   block, lastBlockHeight);
                        }
                        //
                        // В противном случае, просто переносим блок на следующую страницу
                        //
                        else {
                            moveCurrentBlockToNextPage(blockFormat, blockHeight, pageHeight,
                                                       currentBlockWidth(), cursor, block,
                                                       lastBlockHeight);
                        }
                    }
                }

                break;
            }
            }
        }
        //
        // Если блок находится посередине страницы, просто переходим к следующему
        //
        else {
            //
            // Запоминаем параметры текущего блока
            //
            blockItems[currentBlockInfo.number++] = BlockInfo{ blockHeight, lastBlockHeight };
            //
            // и идём дальше
            //
            lastBlockHeight += blockHeight;
        }

        block = block.next();
    }

    cursor.endEditBlock();
}

void ComicBookTextCorrector::Implementation::moveCurrentBlockWithThreePreviousToNextPage(
    const QTextBlock& _prePrePreviousBlock, const QTextBlock& _prePreviousBlock,
    const QTextBlock& _previousBlock, qreal _pageHeight, qreal _pageWidth, TextCursor& _cursor,
    QTextBlock& _block, qreal& _lastBlockHeight)
{
    --currentBlockInfo.number;
    --currentBlockInfo.number;
    --currentBlockInfo.number;

    const QTextBlockFormat previousBlockFormat = _previousBlock.blockFormat();
    const qreal previousBlockHeight
        = previousBlockFormat.lineHeight() * _previousBlock.layout()->lineCount()
        + previousBlockFormat.topMargin() + previousBlockFormat.bottomMargin();
    const QTextBlockFormat prePreviousBlockFormat = _prePreviousBlock.blockFormat();
    const qreal prePreviousBlockHeight
        = prePreviousBlockFormat.lineHeight() * _prePreviousBlock.layout()->lineCount()
        + prePreviousBlockFormat.topMargin() + prePreviousBlockFormat.bottomMargin();
    const QTextBlockFormat prePrePreviousBlockFormat = _prePrePreviousBlock.blockFormat();
    const qreal prePrePreviousBlockHeight
        = prePrePreviousBlockFormat.lineHeight() * _prePrePreviousBlock.layout()->lineCount()
        + prePrePreviousBlockFormat.topMargin() + prePrePreviousBlockFormat.bottomMargin();
    const qreal sizeToPageEnd = _pageHeight - _lastBlockHeight + previousBlockHeight
        + prePreviousBlockHeight + prePrePreviousBlockHeight;
    moveBlockToNextPage(_prePrePreviousBlock, sizeToPageEnd, _pageHeight, _pageWidth, _cursor);
    _block = _cursor.block();
    //
    // Запоминаем параметры блока время и места
    //
    blockItems[currentBlockInfo.number++]
        = BlockInfo{ prePreviousBlockHeight - prePreviousBlockFormat.topMargin(), 0 };
    //
    // Обозначаем последнюю высоту, как высоту предпредпредыдущего блока
    //
    _lastBlockHeight = prePrePreviousBlockHeight - prePrePreviousBlockFormat.topMargin();
    //
    // Текущий блок будет обработан, как очередной блок посередине страницы при следующем проходе
    //
}

void ComicBookTextCorrector::Implementation::moveCurrentBlockWithTwoPreviousToNextPage(
    const QTextBlock& _prePreviousBlock, const QTextBlock& _previousBlock, qreal _pageHeight,
    qreal _pageWidth, TextCursor& _cursor, QTextBlock& _block, qreal& _lastBlockHeight)
{
    --currentBlockInfo.number;
    --currentBlockInfo.number;

    const QTextBlockFormat previousBlockFormat = _previousBlock.blockFormat();
    const qreal previousBlockHeight
        = previousBlockFormat.lineHeight() * _previousBlock.layout()->lineCount()
        + previousBlockFormat.topMargin() + previousBlockFormat.bottomMargin();
    const QTextBlockFormat prePreviousBlockFormat = _prePreviousBlock.blockFormat();
    const qreal prePreviousBlockHeight
        = prePreviousBlockFormat.lineHeight() * _prePreviousBlock.layout()->lineCount()
        + prePreviousBlockFormat.topMargin() + prePreviousBlockFormat.bottomMargin();
    const qreal sizeToPageEnd
        = _pageHeight - _lastBlockHeight + previousBlockHeight + prePreviousBlockHeight;
    moveBlockToNextPage(_prePreviousBlock, sizeToPageEnd, _pageHeight, _pageWidth, _cursor);
    _block = _cursor.block();
    //
    // Запоминаем параметры блока время и места
    //
    blockItems[currentBlockInfo.number++]
        = BlockInfo{ prePreviousBlockHeight - prePreviousBlockFormat.topMargin(), 0 };
    //
    // Обозначаем последнюю высоту, как высоту предпредыдущего блока
    //
    _lastBlockHeight = prePreviousBlockHeight - prePreviousBlockFormat.topMargin();
    //
    // Текущий блок будет обработан, как очередной блок посередине страницы при следующем проходе
    //
}

void ComicBookTextCorrector::Implementation::moveCurrentBlockWithPreviousToNextPage(
    const QTextBlock& _previousBlock, qreal _pageHeight, qreal _pageWidth, TextCursor& _cursor,
    QTextBlock& _block, qreal& _lastBlockHeight)
{
    --currentBlockInfo.number;

    const QTextBlockFormat previousBlockFormat = _previousBlock.blockFormat();
    const qreal previousBlockHeight
        = previousBlockFormat.lineHeight() * _previousBlock.layout()->lineCount()
        + previousBlockFormat.topMargin() + previousBlockFormat.bottomMargin();
    const qreal sizeToPageEnd = _pageHeight - _lastBlockHeight + previousBlockHeight;
    moveBlockToNextPage(_previousBlock, sizeToPageEnd, _pageHeight, _pageWidth, _cursor);
    _block = _cursor.block();
    //
    // Запоминаем параметры предыдущего блока
    //
    blockItems[currentBlockInfo.number++]
        = BlockInfo{ previousBlockHeight - previousBlockFormat.topMargin(), 0 };
    //
    // Обозначаем последнюю высоту, как высоту предыдущего блока
    //
    _lastBlockHeight = previousBlockHeight - previousBlockFormat.topMargin();
    //
    // Текущий блок будет обработан, как очередной блок посередине страницы при следующем проходе
    //
}

void ComicBookTextCorrector::Implementation::moveCurrentBlockToNextPage(
    const QTextBlockFormat& _blockFormat, qreal _blockHeight, qreal _pageHeight, qreal _pageWidth,
    TextCursor& _cursor, QTextBlock& _block, qreal& _lastBlockHeight)
{
    const qreal sizeToPageEnd = _pageHeight - _lastBlockHeight;
    moveBlockToNextPage(_block, sizeToPageEnd, _pageHeight, _pageWidth, _cursor);
    _block = _cursor.block();
    //
    // Запоминаем параметры текущего блока
    //
    blockItems[currentBlockInfo.number++] = BlockInfo{ _blockHeight - _blockFormat.topMargin(), 0 };
    //
    // Обозначаем последнюю высоту, как высоту текущего блока
    //
    _lastBlockHeight = _blockHeight - _blockFormat.topMargin();
}

void ComicBookTextCorrector::Implementation::breakDialogue(const QTextBlockFormat& _blockFormat,
                                                           qreal _blockHeight, qreal _pageHeight,
                                                           qreal _pageWidth, TextCursor& _cursor,
                                                           QTextBlock& _block,
                                                           qreal& _lastBlockHeight)
{
    //
    // Вставить блок
    //
    _cursor.setPosition(_block.position());
    insertBlock(_pageWidth, _cursor);
    updateBlockLayout(_pageWidth, _cursor.block());
    _cursor.movePosition(TextCursor::PreviousBlock);
    //
    // Оформить его, как персонажа, но без отступа сверху
    //
    const auto& moreKeywordStyle = TemplatesFacade::comicBookTemplate(q->templateId())
                                       .paragraphStyle(TextParagraphType::Character);
    QTextBlockFormat moreKeywordFormat = moreKeywordStyle.blockFormat(_cursor.inTable());
    moreKeywordFormat.setTopMargin(0);
    moreKeywordFormat.setProperty(TextBlockStyle::PropertyIsCorrection, true);
    moreKeywordFormat.setProperty(TextBlockStyle::PropertyIsCorrectionContinued, true);
    moreKeywordFormat.setProperty(PageTextEdit::PropertyDontShowCursor, true);
    _cursor.setBlockFormat(moreKeywordFormat);
    _cursor.setBlockCharFormat(moreKeywordStyle.charFormat());
    //
    // Вставить текст ДАЛЬШЕ
    //
    _cursor.insertText(moreTerm());
    _block = _cursor.block();
    updateBlockLayout(_pageWidth, _block);
    const qreal moreBlockHeight = _block.layout()->lineCount() * moreKeywordFormat.lineHeight()
        + moreKeywordFormat.topMargin() + moreKeywordFormat.bottomMargin();
    blockItems[currentBlockInfo.number++] = BlockInfo{ moreBlockHeight, _lastBlockHeight };
    //
    // Перенести текущий блок на следующую страницу, если на текущей влезает
    // ещё хотя бы одна строка текста
    //
    _cursor.movePosition(TextCursor::NextBlock);
    _block = _cursor.block();
    const qreal sizeToPageEnd = _pageHeight - _lastBlockHeight - moreBlockHeight;
    if (sizeToPageEnd >= _blockFormat.topMargin() + _blockFormat.lineHeight()) {
        moveBlockToNextPage(_block, sizeToPageEnd, _pageHeight, _pageWidth, _cursor);
        _block = _cursor.block();
    }
    //
    // Вставить перед текущим блоком декорацию ПЕРСОНАЖ (ПРОД.)
    // и для этого ищём предыдущий блок с именем персонажа
    //
    QTextBlock characterBlock = _block.previous();
    while (characterBlock.isValid()
           && (characterBlock.blockFormat().boolProperty(TextBlockStyle::PropertyIsCorrection)
               || TextBlockStyle::forBlock(characterBlock) != TextParagraphType::Character)) {
        characterBlock = characterBlock.previous();
    }
    //
    // Если нашли блок с именем персонажа
    //
    if (characterBlock.isValid()) {
        //
        // Вставляем блок
        //
        _cursor.setPosition(_block.position());
        insertBlock(_pageWidth, _cursor);
        _cursor.movePosition(TextCursor::PreviousBlock);
        //
        // Оформляем его, как имя персонажа
        //
        QTextBlockFormat characterBlockFormat = characterBlock.blockFormat();
        characterBlockFormat.setProperty(TextBlockStyle::PropertyIsCorrection, true);
        characterBlockFormat.setProperty(TextBlockStyle::PropertyIsCorrectionCharacter, true);
        characterBlockFormat.setProperty(PageTextEdit::PropertyDontShowCursor, true);
        _cursor.setBlockFormat(characterBlockFormat);
        _cursor.setBlockCharFormat(characterBlock.charFormat());
        //
        // И вставляем текст с именем персонажа
        //
        const QString characterName = ComicBookCharacterParser::name(characterBlock.text());
        _cursor.insertText(characterName + continuedTerm());
        _block = _cursor.block();
        //
        updateBlockLayout(_pageWidth, _block);
        const qreal continuedBlockHeight
            = _block.layout()->lineCount() * characterBlockFormat.lineHeight()
            + characterBlockFormat.bottomMargin();
        blockItems[currentBlockInfo.number++] = BlockInfo{ continuedBlockHeight, 0 };
        //
        // Обозначаем последнюю высоту, как высоту предыдущего блока
        //
        _lastBlockHeight = continuedBlockHeight;
        //
        // Текущий блок будет обработан, как очередной блок посередине страницы при следующем
        // проходе
        //
    }
    //
    // А если не нашли то оставляем блок прямо сверху страницы
    //
    else {
        blockItems[currentBlockInfo.number++]
            = BlockInfo{ _blockHeight - _blockFormat.topMargin(), 0 };
        _lastBlockHeight = _blockHeight - _blockFormat.topMargin();
    }
}

QTextBlock ComicBookTextCorrector::Implementation::findPreviousBlock(const QTextBlock& _block)
{
    QTextBlock previousBlock = _block.previous();
    while (previousBlock.isValid() && !previousBlock.isVisible()) {
        --currentBlockInfo.number;
        previousBlock = previousBlock.previous();
    }
    return previousBlock;
}

QTextBlock ComicBookTextCorrector::Implementation::findNextBlock(const QTextBlock& _block)
{
    QTextBlock nextBlock = _block.next();
    while (nextBlock.isValid() && !nextBlock.isVisible()
           && nextBlock.blockFormat().boolProperty(TextBlockStyle::PropertyIsCorrection)) {
        nextBlock = nextBlock.next();
    }
    return nextBlock;
}

void ComicBookTextCorrector::Implementation::moveBlockToNextPage(const QTextBlock& _block,
                                                                 qreal _spaceToPageEnd,
                                                                 qreal _pageHeight,
                                                                 qreal _pageWidth,
                                                                 TextCursor& _cursor)
{
    //
    // Смещаем курсор в начало блока
    //
    _cursor.setPosition(_block.position());
    //
    // Определим сколько пустых блоков нужно вставить
    //
    const QTextBlockFormat format = _block.blockFormat();
    const qreal blockHeight = format.lineHeight() + format.topMargin() + format.bottomMargin();
    const int insertBlockCount = _spaceToPageEnd / blockHeight;
    //
    // Определим формат блоков декорации
    //
    QTextBlockFormat decorationFormat = format;
    const auto paragraphType = TextBlockStyle::forBlock(_block);
    if (paragraphType == TextParagraphType::PanelHeading) {
        decorationFormat.setProperty(TextBlockStyle::PropertyType,
                                     static_cast<int>(TextParagraphType::PanelHeadingShadow));
    }
    if (paragraphType == TextParagraphType::SequenceHeading
        || paragraphType == TextParagraphType::SequenceHeading) {
        decorationFormat.setProperty(TextBlockStyle::PropertyType,
                                     static_cast<int>(TextParagraphType::Description));
        decorationFormat.setBackground(Qt::NoBrush);
    }
    decorationFormat.setProperty(TextBlockStyle::PropertyIsCorrection, true);
    decorationFormat.setProperty(PageTextEdit::PropertyDontShowCursor, true);
    //
    // Вставляем блоки декорации
    //
    for (int blockIndex = 0; blockIndex < insertBlockCount; ++blockIndex) {
        //
        // Декорируем
        //
        insertBlock(_pageWidth, _cursor);
        _cursor.movePosition(TextCursor::PreviousBlock);
        _cursor.setBlockFormat(decorationFormat);
        //
        // Сохраним данные блока, чтобы перенести их к реальному владельцу
        //
        TextBlockData* blockData = nullptr;
        auto block = _cursor.block();
        if (block.userData() != nullptr) {
            blockData = new TextBlockData(static_cast<TextBlockData*>(block.userData()));
            block.setUserData(nullptr);
        }
        //
        // Запоминаем параметры текущего блока
        //
        blockItems[currentBlockInfo.number++]
            = BlockInfo{ blockHeight, _pageHeight - _spaceToPageEnd + blockIndex * blockHeight };
        //
        // Переведём курсор на блок после декорации
        //
        _cursor.movePosition(TextCursor::NextBlock);
        if (blockData != nullptr) {
            _cursor.block().setUserData(blockData);
        }
    }
}


// ****


QString ComicBookTextCorrector::continuedTerm()
{
    return QString(" (%1)").arg(
        QApplication::translate("BusinessLogic::ScriptTextCorrector", kContinuedTerm));
}

ComicBookTextCorrector::ComicBookTextCorrector(QTextDocument* _document)
    : AbstractTextCorrector(_document)
    , d(new Implementation(this))
{
    Q_ASSERT_X(_document, Q_FUNC_INFO, "Document couldn't be a nullptr");
}

ComicBookTextCorrector::~ComicBookTextCorrector() = default;

void ComicBookTextCorrector::setCorrectionOptions(const QStringList& _options)
{
    const auto needToCorrectCharactersNames = _options.contains("correct-characters-names");
    const auto needToShowDialogueNumber = _options.contains("show-dialogue-number");
    const auto needToBoldPanelTitle = _options.contains("bold-panel-title");
    const auto needToCorrectPageBreaks = _options.contains("correct-page-breaks");
    if (d->needToCorrectCharactersNames == needToCorrectCharactersNames
        && d->needToShowDialogueNumber == needToShowDialogueNumber
        && d->needToBoldPanelTitle == needToBoldPanelTitle
        && d->needToCorrectPageBreaks == needToCorrectPageBreaks) {
        return;
    }

    d->needToCorrectCharactersNames = needToCorrectCharactersNames;
    d->needToShowDialogueNumber = needToShowDialogueNumber;
    d->needToBoldPanelTitle = needToBoldPanelTitle;
    d->needToCorrectPageBreaks = needToCorrectPageBreaks;

    clear();
    makeCorrections();
}

void ComicBookTextCorrector::clearImpl()
{
    d->lastDocumentSize = QSizeF();
    d->currentBlockInfo.number = 0;
    d->blockItems.clear();
}

void ComicBookTextCorrector::makeCorrections(int _position, int _charsChanged)
{
    //
    // Избегаем рекурсии, которая может возникать от того, что корректировка происходит
    // при изменении текста документа, но в то же время корректор сам делает изменения в документе
    //
    const auto canRun = RunOnce::tryRun(Q_FUNC_INFO);
    if (!canRun) {
        return;
    }

    //
    // Сначала корректируем видимость блоков
    //
    makeSoftCorrections(_position, _charsChanged);

    //
    // Затем при необходимости корректируем сам текст
    //

    if (d->needToCorrectCharactersNames) {
        d->correctCharactersNames(_position, _charsChanged);
    }

    d->correctBlocksNumbers(_position, _charsChanged);

    if (d->needToCorrectPageBreaks) {
        d->correctPageBreaks(_position);
    }
}

void ComicBookTextCorrector::makeSoftCorrections(int _position, int _charsChanged)
{
    d->updateBlocksVisibility(_position, _charsChanged);
}

} // namespace BusinessLayer
