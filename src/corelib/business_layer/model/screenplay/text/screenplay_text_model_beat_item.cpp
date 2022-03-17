#include "screenplay_text_model_beat_item.h"

#include "screenplay_text_model.h"
#include "screenplay_text_model_text_item.h"

#include <business_layer/model/text/text_model_xml.h>
#include <business_layer/templates/screenplay_template.h>
#include <utils/helpers/text_helper.h>


namespace BusinessLayer {

class ScreenplayTextModelBeatItem::Implementation
{
public:
    //
    // Ридонли свойства, которые формируются по ходу работы со сценарием
    //

    /**
     * @brief Длительность бита
     */
    std::chrono::milliseconds duration = std::chrono::milliseconds{ 0 };
};


// ****


ScreenplayTextModelBeatItem::ScreenplayTextModelBeatItem(const ScreenplayTextModel* _model)
    : TextModelGroupItem(_model)
    , d(new Implementation)
{
    setGroupType(TextGroupType::Beat);
    setLevel(1);
}

ScreenplayTextModelBeatItem::~ScreenplayTextModelBeatItem() = default;

std::chrono::milliseconds ScreenplayTextModelBeatItem::duration() const
{
    return d->duration;
}

QVariant ScreenplayTextModelBeatItem::data(int _role) const
{
    switch (_role) {
    case Qt::DecorationRole: {
        return u8"\U000F0B77";
    }

    case BeatDurationRole: {
        const int duration = std::chrono::duration_cast<std::chrono::seconds>(d->duration).count();
        return duration;
    }

    default: {
        return TextModelGroupItem::data(_role);
    }
    }
}

QStringRef ScreenplayTextModelBeatItem::readCustomContent(QXmlStreamReader& _contentReader)
{
    return _contentReader.name();
}

QByteArray ScreenplayTextModelBeatItem::customContent() const
{
    return {};
}

void ScreenplayTextModelBeatItem::handleChange()
{
    QString heading;
    QString text;
    int inlineNotesSize = 0;
    int reviewMarksSize = 0;
    d->duration = std::chrono::seconds{ 0 };

    for (int childIndex = 0; childIndex < childCount(); ++childIndex) {
        auto child = childAt(childIndex);
        if (child->type() != TextModelItemType::Text) {
            continue;
        }

        auto childTextItem = static_cast<ScreenplayTextModelTextItem*>(child);

        //
        // Собираем текст
        //
        switch (childTextItem->paragraphType()) {
        case TextParagraphType::BeatHeading: {
            heading = TextHelper::smartToUpper(childTextItem->text());
            break;
        }

        case TextParagraphType::InlineNote: {
            ++inlineNotesSize;
            break;
        }

        default: {
            text.append(childTextItem->text() + " ");
            reviewMarksSize += std::count_if(
                childTextItem->reviewMarks().begin(), childTextItem->reviewMarks().end(),
                [](const ScreenplayTextModelTextItem::ReviewMark& _reviewMark) {
                    return !_reviewMark.isDone;
                });
            break;
        }
        }

        //
        // Собираем хронометраж
        //
        d->duration += childTextItem->duration();
    }

    setHeading(heading);
    setText(text);
    setInlineNotesSize(inlineNotesSize);
    setReviewMarksSize(reviewMarksSize);
}

} // namespace BusinessLayer
