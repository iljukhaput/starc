#include "text_model_group_item.h"

#include "text_model.h"
#include "text_model_splitter_item.h"
#include "text_model_text_item.h"
#include "text_model_xml.h"
#include "text_model_xml_writer.h"

#include <business_layer/templates/screenplay_template.h>
#include <utils/helpers/text_helper.h>

#include <QDateTime>
#include <QLocale>
#include <QUuid>
#include <QVariant>
#include <QXmlStreamReader>


namespace BusinessLayer {

class TextModelGroupItem::Implementation
{
public:
    /**
     * @brief Тип группы
     */
    TextGroupType groupType = TextGroupType::Undefined;

    /**
     * @brief Идентификатор группы
     */
    QUuid uuid = QUuid::createUuid();

    /**
     * @brief Пропущена ли группа
     */
    bool isOmited = false;

    /**
     * @brief Номер группы
     */
    std::optional<Number> number;

    /**
     * @brief Цвет группы
     */
    QColor color;

    //    /**
    //     * @brief Тэги группы
    //     */
    //    QVector<Tag> tags;

    //    /**
    //     * @brief Сюжетные линии группы
    //     */
    //    QVector<StoryLine> storyLines;

    /**
     * @brief Название группы
     */
    QString title;

    /**
     * @brief День сценария
     */
    QString storyDay;

    /**
     * @brief Дата и время начала события
     */
    QDateTime startDateTime;

    /**
     * @brief Штамп на группе
     */
    QString stamp;

    /**
     * @brief Тэги группы
     */
    QVector<QPair<QString, QColor>> tags;

    //
    // Ридонли свойства, которые формируются по ходу работы
    //

    /**
     * @brief Заголовок группы
     */
    QString heading;

    /**
     * @brief Текст группы
     */
    QString text;

    /**
     * @brief Количество заметок по тексту
     */
    int inlineNotesSize = 0;

    /**
     * @brief Количество редакторских заметок
     */
    int reviewMarksSize = 0;
};


// ****


bool TextModelGroupItem::Number::operator==(const TextModelGroupItem::Number& _other) const
{
    return value == _other.value && isCustom == _other.isCustom && isEatNumber == _other.isEatNumber
        && isLocked == _other.isLocked && followNumber == _other.followNumber;
}

TextModelGroupItem::TextModelGroupItem(const TextModel* _model)
    : TextModelItem(TextModelItemType::Group, _model)
    , d(new Implementation)
{
}

TextModelGroupItem::~TextModelGroupItem() = default;

int TextModelGroupItem::subtype() const
{
    return static_cast<int>(groupType());
}

const TextGroupType& TextModelGroupItem::groupType() const
{
    return d->groupType;
}

void TextModelGroupItem::setGroupType(TextGroupType _type)
{
    if (d->groupType == _type) {
        return;
    }

    d->groupType = _type;
    setChanged(true);
}

QUuid TextModelGroupItem::uuid() const
{
    return d->uuid;
}

int TextModelGroupItem::level() const
{
    return textGroupTypeLevel(d->groupType);
}

std::optional<TextModelGroupItem::Number> TextModelGroupItem::number() const
{
    return d->number;
}

void TextModelGroupItem::resetNumber()
{
    if (d->number.has_value()) {
        d->number.reset();
    }
}

bool TextModelGroupItem::setNumber(int _number, const QString& _followNumber)
{
    if (childCount() == 0) {
        return false;
    }

    bool hasContent = false;
    for (int childIndex = 0; childIndex < childCount(); ++childIndex) {
        const auto child = childAt(childIndex);
        if (child->type() != TextModelItemType::Text) {
            continue;
        }

        const auto textItemChild = static_cast<const TextModelTextItem*>(child);
        if (!textItemChild->isCorrection()) {
            hasContent = true;
            break;
        }
    }
    if (!hasContent) {
        return false;
    }

    //
    // В простом случае, текст номера это его текстовое представление
    //
    QString numberText = QString::number(_number);
    //
    // ... а в случае подмножества, он представляется буквой,
    //     если же буквы заканчиваются, то к букве добавляется номер
    //
    if (!_followNumber.isEmpty()) {
        const unsigned alphabetLen = 'Z' - 'A' + 1;
        unsigned sceneLetter = _number % alphabetLen;
        unsigned sceneLetterNumber = _number / alphabetLen;
        numberText = QChar('A' + sceneLetter);
        if (sceneLetterNumber != 0) {
            numberText += QString::number(sceneLetterNumber);
        }
    }

    auto newNumber = d->number.value_or(Number());
    newNumber.value = numberText;
    newNumber.followNumber = _followNumber;
    if (d->number == newNumber) {
        return false;
    }

    d->number = newNumber;
    setChanged(true);

    return true;
}

void TextModelGroupItem::setCustomNumber(const QString& _customNumber, bool _isEatNumber)
{
    if (childCount() == 0) {
        return;
    }

    bool hasContent = false;
    for (int childIndex = 0; childIndex < childCount(); ++childIndex) {
        const auto child = childAt(childIndex);
        if (child->type() != TextModelItemType::Text) {
            continue;
        }

        const auto textItemChild = static_cast<const TextModelTextItem*>(child);
        if (!textItemChild->isCorrection()) {
            hasContent = true;
            break;
        }
    }
    if (!hasContent) {
        return;
    }

    if (d->number.has_value() && d->number->isCustom && d->number->text == _customNumber
        && d->number->isEatNumber == _isEatNumber) {
        return;
    }

    auto newNumber = d->number.value_or(Number());
    newNumber.isCustom = true;
    newNumber.value = _customNumber;
    newNumber.followNumber.clear();
    newNumber.isEatNumber = _isEatNumber;
    d->number = newNumber;
    setChanged(true);
}

void TextModelGroupItem::lockNumber()
{
    if (childCount() == 0 || !d->number.has_value() || d->number->isLocked) {
        return;
    }

    d->number->value.prepend(d->number->followNumber);
    d->number->followNumber.clear();
    d->number->isLocked = true;
    setChanged(true);
}

void TextModelGroupItem::prepareNumberText(const QString& _template)
{
    if (!d->number.has_value()) {
        return;
    }

    auto newNumberText = _template;
    newNumberText.replace('#', d->number->followNumber + d->number->value);
    if (d->number->text == newNumberText) {
        return;
    }

    d->number->text = newNumberText;
    setChanged(true);
}

QColor TextModelGroupItem::color() const
{
    return d->color;
}

void TextModelGroupItem::setColor(const QColor& _color)
{
    if (d->color == _color) {
        return;
    }

    d->color = _color;
    setChanged(true);
}

QString TextModelGroupItem::title() const
{
    return d->title;
}

void TextModelGroupItem::setTitle(const QString& _title)
{
    if (d->title == _title) {
        return;
    }

    d->title = _title;
    setChanged(true);
}

QString TextModelGroupItem::storyDay() const
{
    return d->storyDay;
}

void TextModelGroupItem::setStoryDay(const QString& _storyDay)
{
    if (d->storyDay == _storyDay) {
        return;
    }

    d->storyDay = _storyDay;
    setChanged(true);
}

QDateTime TextModelGroupItem::startDateTime() const
{
    return d->startDateTime;
}

void TextModelGroupItem::setStartDateTime(const QDateTime& _startDateTime)
{
    if (d->startDateTime == _startDateTime) {
        return;
    }

    d->startDateTime = _startDateTime;
    setChanged(true);
}

QString TextModelGroupItem::stamp() const
{
    return d->stamp;
}

void TextModelGroupItem::setStamp(const QString& _stamp)
{
    if (d->stamp == _stamp) {
        return;
    }

    d->stamp = _stamp;
    setChanged(true);
}

QVector<QPair<QString, QColor>> TextModelGroupItem::tags() const
{
    return d->tags;
}

void TextModelGroupItem::setTags(const QVector<QPair<QString, QColor>>& _tags)
{
    if (d->tags == _tags) {
        return;
    }

    d->tags = _tags;
    setChanged(true);
}

QString TextModelGroupItem::heading() const
{
    return d->heading;
}

QString TextModelGroupItem::text() const
{
    return d->text;
}

int TextModelGroupItem::length() const
{
    return heading().length() + text().length() + (childCount() > 1 ? 1 : 0);
}

int TextModelGroupItem::inlineNotesSize() const
{
    return d->inlineNotesSize;
}

int TextModelGroupItem::reviewMarksSize() const
{
    return d->reviewMarksSize;
}

bool TextModelGroupItem::isEmpty() const
{
    return d->title.isEmpty() && d->heading.isEmpty() && d->text.isEmpty();
}

QVariant TextModelGroupItem::data(int _role) const
{
    switch (_role) {
    case Qt::DecorationRole: {
        return !customIcon().isEmpty() ? customIcon() : u8"\U000f021a";
    }

    case GroupTypeRole: {
        return static_cast<int>(d->groupType);
    }

    case GroupNumberRole: {
        if (d->number.has_value()) {
            return d->number->text;
        }
        return {};
    }

    case GroupColorRole: {
        return d->color;
    }

    case Qt::DisplayRole:
    case GroupHeadingRole: {
        return d->title.isEmpty() ? d->heading : d->title;
    }

    case GroupTextRole: {
        return d->text;
    }

    case GroupInlineNotesSizeRole: {
        return d->inlineNotesSize;
    }

    case GroupReviewMarksSizeRole: {
        return d->reviewMarksSize;
    }

    default: {
        return TextModelItem::data(_role);
    }
    }
}

void TextModelGroupItem::readContent(QXmlStreamReader& _contentReader)
{
    d->groupType = textGroupTypeFromString(_contentReader.name().toString());
    Q_ASSERT(d->groupType != TextGroupType::Undefined);

    const auto groupAttributes = _contentReader.attributes();
    if (groupAttributes.hasAttribute(xml::kUuidAttribute)) {
        d->uuid = QUuid::fromString(groupAttributes.value(xml::kUuidAttribute).toString());
    }

    //
    // TODO: plots
    //
    d->isOmited = groupAttributes.hasAttribute(xml::kOmitedAttribute);
    xml::readNextElement(_contentReader);

    auto currentTag = _contentReader.name();
    if (currentTag == xml::kNumberTag) {
        const auto attributes = _contentReader.attributes();
        d->number = { attributes.value(xml::kNumberValueAttribute).toString(),
                      {},
                      attributes.hasAttribute(xml::kNumberIsCustomAttribute),
                      attributes.hasAttribute(xml::kNumberIsEatNumberAttribute),
                      attributes.hasAttribute(xml::kNumberIsLockedAttribute) };
        xml::readNextElement(_contentReader); // end
        currentTag = xml::readNextElement(_contentReader); // next
    }

    if (currentTag == xml::kColorTag) {
        d->color = xml::readContent(_contentReader).toString();
        xml::readNextElement(_contentReader); // end
        currentTag = xml::readNextElement(_contentReader); // next
    }

    if (currentTag == xml::kTitleTag) {
        d->title = TextHelper::fromHtmlEscaped(xml::readContent(_contentReader).toString());
        xml::readNextElement(_contentReader); // end
        currentTag = xml::readNextElement(_contentReader); // next
    }

    if (currentTag == xml::kStoryDayTag) {
        d->storyDay = TextHelper::fromHtmlEscaped(xml::readContent(_contentReader).toString());
        xml::readNextElement(_contentReader); // end
        currentTag = xml::readNextElement(_contentReader); // next
    }

    if (currentTag == xml::kStartDateTimeTag) {
        d->startDateTime
            = QDateTime::fromString(xml::readContent(_contentReader).toString(), Qt::ISODateWithMs);
        xml::readNextElement(_contentReader); // end
        currentTag = xml::readNextElement(_contentReader); // next
    }

    if (currentTag == xml::kStampTag) {
        d->stamp = TextHelper::fromHtmlEscaped(xml::readContent(_contentReader).toString());
        xml::readNextElement(_contentReader); // end
        currentTag = xml::readNextElement(_contentReader); // next
    }

    if (currentTag == xml::kTagsTag) {
        d->tags.clear();
        currentTag = xml::readNextElement(_contentReader); // next
        while (currentTag == xml::kTagTag) {
            const auto tagColor
                = _contentReader.attributes().value(xml::kColorAttribute).toString();
            const auto tagName
                = TextHelper::fromHtmlEscaped(xml::readContent(_contentReader).toString());
            d->tags.append({ tagName, tagColor });

            xml::readNextElement(_contentReader); // end
            currentTag = xml::readNextElement(_contentReader); // next tag or tags end
        }
        currentTag = xml::readNextElement(_contentReader); // next
    }

    currentTag = readCustomContent(_contentReader);

    if (currentTag == xml::kContentTag) {
        xml::readNextElement(_contentReader); // next item
        do {
            currentTag = _contentReader.name();

            //
            // Проглатываем закрывающий контентный тэг
            //
            if (currentTag == xml::kContentTag && _contentReader.isEndElement()) {
                xml::readNextElement(_contentReader);
                continue;
            }
            //
            // Если дошли до конца группы, выходим из обработки
            //
            else if (textGroupTypeFromString(currentTag.toString()) != TextGroupType::Undefined
                     && _contentReader.isEndElement()) {
                xml::readNextElement(_contentReader);
                break;
            }
            //
            // Считываем вложенный контент
            //
            else if (textGroupTypeFromString(currentTag.toString()) != TextGroupType::Undefined) {
                auto item = model()->createGroupItem(_contentReader);
                appendItem(item);
            } else if (currentTag == xml::kSplitterTag) {
                auto item = model()->createSplitterItem(_contentReader);
                appendItem(item);
            } else {
                auto item = model()->createTextItem(_contentReader);
                appendItem(item);
            }
        } while (!_contentReader.atEnd());
    }

    //
    // Соберём заголовок, текст группы и прочие параметры
    //
    handleChange();
}

QByteArray TextModelGroupItem::toXml() const
{
    return toXml(nullptr, 0, nullptr, 0, false);
}

QByteArray TextModelGroupItem::toXml(TextModelItem* _from, int _fromPosition, TextModelItem* _to,
                                     int _toPosition, bool _clearUuid) const
{
    xml::TextModelXmlWriter xml;
    xml += xmlHeader(_clearUuid);
    for (int childIndex = 0; childIndex < childCount(); ++childIndex) {
        auto child = childAt(childIndex);

        //
        // Нетекстовые блоки, просто добавляем к общему xml
        //
        if (child->type() != TextModelItemType::Text) {
            xml += child;
            if (child == _to) {
                break;
            }
            continue;
        }

        //
        // Текстовые блоки, в зависимости от необходимости вставить блок целиком, или его часть
        //
        auto textItem = static_cast<TextModelTextItem*>(child);
        if (textItem == _to) {
            if (textItem == _from) {
                xml += { textItem, _fromPosition, _toPosition - _fromPosition };
            } else {
                xml += { textItem, 0, _toPosition };
            }
            break;
        }
        //
        else if (textItem == _from) {
            xml += { textItem, _fromPosition,
                     static_cast<int>(textItem->text().length()) - _fromPosition };
        } else {
            xml += textItem;
        }
    }
    xml += QString("</%1>\n").arg(xml::kContentTag).toUtf8();
    xml += QString("</%1>\n").arg(toString(d->groupType)).toUtf8();

    return xml.data();
}

QByteArray TextModelGroupItem::xmlHeader(bool _clearUuid) const
{
    QByteArray xml;
    //
    // TODO: plots
    //
    xml += QString("<%1 %2=\"%3\" %4=\"%5\" %6>\n")
               .arg(toString(d->groupType), xml::kUuidAttribute,
                    _clearUuid ? QUuid::createUuid().toString() : d->uuid.toString(),
                    xml::kPlotsAttribute, QString(),
                    (d->isOmited ? QString("%1=\"true\"").arg(xml::kOmitedAttribute) : ""))
               .toUtf8();
    if (d->number.has_value() && (d->number->isCustom || d->number->isLocked)) {
        xml += QString("<%1 %2=\"%3\" %4%5/>\n")
                   .arg(xml::kNumberTag, xml::kNumberValueAttribute, d->number->value,
                        (d->number->isCustom
                             ? QString("%1=\"true\" %2 ")
                                   .arg(xml::kNumberIsCustomAttribute,
                                        (d->number->isEatNumber
                                             ? QString("%1=\"true\"")
                                                   .arg(xml::kNumberIsEatNumberAttribute)
                                             : ""))
                             : ""),
                        (d->number->isLocked
                             ? QString("%1=\"true\" ").arg(xml::kNumberIsLockedAttribute)
                             : ""))
                   .toUtf8();
    }
    if (d->color.isValid()) {
        xml += QString("<%1><![CDATA[%2]]></%1>\n").arg(xml::kColorTag, d->color.name()).toUtf8();
    }
    if (!d->title.isEmpty()) {
        xml += QString("<%1><![CDATA[%2]]></%1>\n")
                   .arg(xml::kTitleTag, TextHelper::toHtmlEscaped(d->title))
                   .toUtf8();
    }
    if (!d->storyDay.isEmpty()) {
        xml += QString("<%1><![CDATA[%2]]></%1>\n")
                   .arg(xml::kStoryDayTag, TextHelper::toHtmlEscaped(d->storyDay))
                   .toUtf8();
    }
    if (d->startDateTime.isValid()) {
        xml += QString("<%1><![CDATA[%2]]></%1>\n")
                   .arg(xml::kStartDateTimeTag, d->startDateTime.toString(Qt::ISODateWithMs))
                   .toUtf8();
    }
    if (!d->stamp.isEmpty()) {
        xml += QString("<%1><![CDATA[%2]]></%1>\n")
                   .arg(xml::kStampTag, TextHelper::toHtmlEscaped(d->stamp))
                   .toUtf8();
    }
    if (!d->tags.isEmpty()) {
        xml += QString("<%1>\n").arg(xml::kTagsTag).toUtf8();
        for (const auto& tag : std::as_const(d->tags)) {
            xml += QString("<%1 %2=\"%3\"><![CDATA[%4]]></%1>\n")
                       .arg(xml::kTagTag, xml::kColorAttribute, tag.second.name(),
                            TextHelper::toHtmlEscaped(tag.first))
                       .toUtf8();
        }
        xml += QString("</%1>\n").arg(xml::kTagsTag).toUtf8();
    }
    xml += customContent();
    xml += QString("<%1>\n").arg(xml::kContentTag).toUtf8();

    return xml;
}

void TextModelGroupItem::copyFrom(TextModelItem* _item)
{
    if (_item == nullptr || type() != _item->type() || subtype() != _item->subtype()) {
        Q_ASSERT(false);
        return;
    }

    auto groupItem = static_cast<TextModelGroupItem*>(_item);
    d->uuid = groupItem->d->uuid;
    d->isOmited = groupItem->d->isOmited;
    d->number = groupItem->d->number;
    d->color = groupItem->d->color;
    d->title = groupItem->title();
    d->storyDay = groupItem->d->storyDay;
    d->startDateTime = groupItem->d->startDateTime;
    d->stamp = groupItem->d->stamp;
    d->tags = groupItem->d->tags;
}

bool TextModelGroupItem::isEqual(TextModelItem* _item) const
{
    if (_item == nullptr || type() != _item->type() || subtype() != _item->subtype()) {
        return false;
    }

    const auto groupItem = static_cast<TextModelGroupItem*>(_item);
    return d->uuid == groupItem->d->uuid
        && d->isOmited == groupItem->d->isOmited
        //
        // TODO: тут нужно сравнивать, только когда номера зафиксированы
        //
        //            && d->number == sceneItem->d->number
        && d->color == groupItem->d->color && d->title == groupItem->d->title
        && d->storyDay == groupItem->d->storyDay && d->startDateTime == groupItem->d->startDateTime
        && d->stamp == groupItem->d->stamp && d->tags == groupItem->d->tags;
}

void TextModelGroupItem::setHeading(const QString& _heading)
{
    d->heading = _heading;
}

void TextModelGroupItem::setText(const QString& _text)
{
    d->text = _text;
}

void TextModelGroupItem::setInlineNotesSize(int _size)
{
    d->inlineNotesSize = _size;
}

void TextModelGroupItem::setReviewMarksSize(int _size)
{
    d->reviewMarksSize = _size;
}

} // namespace BusinessLayer
