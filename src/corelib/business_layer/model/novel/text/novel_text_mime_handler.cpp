#include "novel_text_mime_handler.h"

#include <business_layer/templates/novel_template.h>

#include <QRegularExpression>
#include <QUuid>


namespace BusinessLayer {

QString NovelTextMimeHandler::removeInvisibleBlocksForTreatment(const QString& _mime)
{
    QString correctedMime;

    //
    // Вырезаем все лишние блоки, оставляя только папки, сцены и биты с пустым действием внутри
    //

    struct {
        int from = 0;
        int to = 0;
        bool inProcessing = false;
    } startTag, endTag;
    auto startTagContent = [&startTag, _mime] {
        return _mime.mid(startTag.from + 1, startTag.to - startTag.from - 1);
    };
    auto endTaContent
        = [&endTag, _mime] { return _mime.mid(endTag.from + 2, endTag.to - endTag.from - 2); };
    bool waitForEndTag = false;
    for (int pos = 0; pos < _mime.length(); ++pos) {
        //
        // Начало тэга
        //
        if (_mime[pos] == '<') {
            if (_mime[pos + 1] == '/') {
                endTag.from = pos;
                endTag.inProcessing = true;
            } else if (!waitForEndTag) {
                startTag.from = pos;
                startTag.inProcessing = true;
            }
        }
        //
        // Конец тэга
        //
        else if (_mime[pos] == '>') {
            if (startTag.inProcessing) {
                startTag.to = pos;
                startTag.inProcessing = false;

                //
                // Некоторые тэги копируем целиком, дойдя до завершающего
                //
                const auto tagContent = startTagContent();
                if (tagContent.startsWith("part_heading") || tagContent.startsWith("part_footer")
                    || tagContent.startsWith("chapter_heading")
                    || tagContent.startsWith("chapter_footer")
                    || tagContent.startsWith("scene_heading")
                    || tagContent.startsWith("beat_heading")
                    || tagContent.startsWith("scene_characters")) {
                    waitForEndTag = true;
                }
                //
                // Некоторые тэги просто копируем без поиска завершающего
                //
                else if (tagContent.startsWith("?xml") || tagContent.startsWith("document ")
                         || tagContent.startsWith("part ") || tagContent.startsWith("chapter ")
                         || tagContent.startsWith("scene ") || tagContent.startsWith("beat ")
                         || tagContent == "content") {
                    correctedMime += _mime.midRef(startTag.from, startTag.to - startTag.from + 1);
                }
            } else if (endTag.inProcessing) {
                endTag.to = pos;
                endTag.inProcessing = false;

                //
                // Некоторые тэги копируем целиком
                //
                const auto tagContent = endTaContent();
                if (tagContent.startsWith("part_heading") || tagContent.startsWith("part_footer")
                    || tagContent.startsWith("chapter_heading")
                    || tagContent.startsWith("chapter_footer")
                    || tagContent.startsWith("scene_heading")
                    || tagContent.startsWith("beat_heading")
                    || tagContent.startsWith("scene_characters")) {
                    waitForEndTag = false;
                    correctedMime += _mime.midRef(startTag.from, endTag.to - startTag.from + 1);

                    //
                    // Для бита, дополнительно вставляем пустой блок действия, чтобы его можно было
                    // потом прописать в редакторе текста сценария
                    //
                    if (tagContent.startsWith("beat_heading")) {
                        correctedMime += "<action><v><![CDATA[]]></v></action>";
                    }
                }
                //
                // Некоторые тэги просто копируем целиком, без учёта начала
                //
                else if (tagContent.startsWith("?xml") || tagContent.startsWith("document ")
                         || tagContent.startsWith("part ") || tagContent.startsWith("chapter ")
                         || tagContent.startsWith("scene ") || tagContent.startsWith("beat ")
                         || tagContent == "content") {
                    correctedMime += _mime.midRef(endTag.from, endTag.to - endTag.from + 1);
                }
            }
        }
    }

    return correctedMime;
}

QString NovelTextMimeHandler::convertTextBlocksToBeats(const QString& _mime)
{
    QString correctedMime;

    //
    // Преобразуем все текстовые блоки в биты, кроме папок, сцен, участников сцены и самих битов
    //

    struct {
        int from = 0;
        int to = 0;
        bool inProcessing = false;
    } startTag, endTag;
    auto startTagContent = [&startTag, _mime] {
        return _mime.mid(startTag.from + 1, startTag.to - startTag.from - 1);
    };
    auto endTaContent
        = [&endTag, _mime] { return _mime.mid(endTag.from + 2, endTag.to - endTag.from - 2); };
    bool copyAsIs = false;
    bool copyWithEndTagWithoutModification = false;
    bool copyWithEndTagAsBeat = false;
    for (int pos = 0; pos < _mime.length(); ++pos) {
        //
        // Начало тэга
        //
        if (_mime[pos] == '<') {
            if (_mime[pos + 1] == '/') {
                endTag.from = pos;
                endTag.inProcessing = true;
            } else if (!copyWithEndTagWithoutModification && !copyWithEndTagAsBeat) {
                startTag.from = pos;
                startTag.inProcessing = true;
            }
        }
        //
        // Конец тэга
        //
        else if (_mime[pos] == '>') {
            if (startTag.inProcessing) {
                startTag.to = pos;
                startTag.inProcessing = false;

                //
                // Некоторые тэги копируем целиком, дойдя до завершающего
                //
                const auto tagContent = startTagContent();
                if (tagContent.startsWith("part_heading") || tagContent.startsWith("part_footer")
                    || tagContent.startsWith("chapter_heading")
                    || tagContent.startsWith("chapter_footer")
                    || tagContent.startsWith("scene_heading")
                    || tagContent.startsWith("beat_heading")) {
                    copyWithEndTagWithoutModification = true;
                }
                //
                // Некоторые тэги просто копируем без поиска завершающего
                //
                else if (tagContent.startsWith("?xml") || tagContent.startsWith("document ")
                         || tagContent.startsWith("part ") || tagContent == QLatin1String("part")
                         || tagContent.startsWith("chapter") || tagContent.startsWith("scene")
                         || tagContent == "content") {
                    correctedMime += _mime.midRef(startTag.from, startTag.to - startTag.from + 1);
                }
                //
                // Содержимое битов не будем преобразовывать
                //
                else if (tagContent.startsWith("beat")) {
                    correctedMime += _mime.midRef(startTag.from, startTag.to - startTag.from + 1);
                }
                //
                // Остальные копируем целиком, дойдя до завершающего, при необходимости преобразуя
                //
                else if (tagContent.startsWith("unformatted_text")
                         || tagContent.startsWith("inline_note") || tagContent.startsWith("text")) {
                    if (copyAsIs) {
                        copyWithEndTagWithoutModification = true;
                    } else {
                        copyWithEndTagAsBeat = true;
                    }
                }
            } else if (endTag.inProcessing) {
                endTag.to = pos;
                endTag.inProcessing = false;

                //
                // Некоторые тэги копируем целиком
                //
                const auto tagContent = endTaContent();
                if (tagContent.startsWith("part_heading") || tagContent.startsWith("part_footer")
                    || tagContent.startsWith("chapter_heading")
                    || tagContent.startsWith("chapter_footer")
                    || tagContent.startsWith("scene_heading")
                    || tagContent.startsWith("beat_heading")) {
                    copyWithEndTagWithoutModification = false;
                    correctedMime += _mime.midRef(startTag.from, endTag.to - startTag.from + 1);
                }
                //
                // Некоторые тэги просто копируем целиком, без учёта начала
                //
                else if (tagContent.startsWith("?xml") || tagContent.startsWith("document ")
                         || tagContent.startsWith("part ") || tagContent == QLatin1String("part")
                         || tagContent.startsWith("chapter") || tagContent.startsWith("scene")
                         || tagContent == "content") {
                    correctedMime += _mime.midRef(endTag.from, endTag.to - endTag.from + 1);
                }
                //
                // Завершаем необходимость копировать текстовые блоки без преобразования
                //
                else if (tagContent.startsWith("beat")) {
                    copyAsIs = false;
                    correctedMime += _mime.midRef(endTag.from, endTag.to - endTag.from + 1);
                }
                //
                // Копируем содержимое текстового блока, при необходимости преобразуя в бит
                //
                else if (tagContent.startsWith("unformatted_text")
                         || tagContent.startsWith("inline_note") || tagContent.startsWith("text")) {
                    if (copyAsIs) {
                        copyWithEndTagWithoutModification = false;
                        correctedMime += _mime.midRef(startTag.from, endTag.to - startTag.from + 1);
                    } else {
                        copyWithEndTagAsBeat = false;
                        auto blockXml = _mime.mid(startTag.from, endTag.to - startTag.from + 1);
                        blockXml.replace(tagContent, "beat_heading");
                        correctedMime += QString("<beat uuid=\"%1\"><content>%2</content></beat>")
                                             .arg(QUuid::createUuid().toString(), blockXml);
                    }
                }
            }
        }
    }

    return correctedMime;
}

QString NovelTextMimeHandler::convertBeatsToTextBlocks(const QString& _mime)
{
    QString correctedMime = _mime;
    correctedMime.remove(QRegularExpression("<beat uuid=(*.)><content>"));
    correctedMime.remove("</content></beat>");
    correctedMime.replace("beat_heading", "text");
    return correctedMime;
}

} // namespace BusinessLayer
