#include "key_press_handler_facade.h"

#include "inline_note_handler.h"
#include "pre_handler.h"
#include "prepare_handler.h"
#include "scene_heading_handler.h"
#include "text_handler.h"

#include "../text_edit.h"

#include <business_layer/templates/text_template.h>

#include <QTextBlock>
#include <QKeyEvent>

using BusinessLayer::TextParagraphType;
using Ui::TextEdit;

namespace KeyProcessingLayer
{

class KeyPressHandlerFacade::Implementation
{
public:
    explicit Implementation(TextEdit* _editor);

    Ui::TextEdit* editor = nullptr;

    QScopedPointer<PrepareHandler> prepareHandler;
    QScopedPointer<PreHandler> preHandler;
    QScopedPointer<SceneHeadingHandler> headingHandler;
    QScopedPointer<TextHandler> textHandler;
    QScopedPointer<InlineNoteHandler> inlineNoteHandler;
};

KeyPressHandlerFacade::Implementation::Implementation(Ui::TextEdit* _editor)
    : editor(_editor),
      prepareHandler(new PrepareHandler(_editor)),
      preHandler(new PreHandler(_editor)),
      headingHandler(new SceneHeadingHandler(_editor)),
      textHandler(new TextHandler(_editor)),
      inlineNoteHandler(new InlineNoteHandler(_editor))
{
}


// ****


KeyPressHandlerFacade* KeyPressHandlerFacade::instance(Ui::TextEdit* _editor)
{
    static QHash<TextEdit*, KeyPressHandlerFacade*> instances;
    if (!instances.contains(_editor)) {
        instances.insert(_editor, new KeyPressHandlerFacade(_editor));
    }

    return instances.value(_editor);
}

KeyPressHandlerFacade::~KeyPressHandlerFacade() = default;

void KeyPressHandlerFacade::prepare(QKeyEvent* _event)
{
    d->prepareHandler->handle(_event);
}

void KeyPressHandlerFacade::prepareForHandle(QKeyEvent* _event)
{
    d->preHandler->handle(_event);
}

void KeyPressHandlerFacade::prehandle()
{
    const bool prepare = true;
    handle(nullptr, prepare);
}

void KeyPressHandlerFacade::handle(QEvent* _event, bool _pre)
{
    const auto currentType = d->editor->currentParagraphType();
    auto currentHandler = handlerFor(currentType);

    if (currentHandler == nullptr) {
        return;
    }

    if (_pre) {
        currentHandler->prehandle();
    } else {
        currentHandler->handle(_event);
    }
}

bool KeyPressHandlerFacade::needSendEventToBaseClass() const
{
    return d->prepareHandler->needSendEventToBaseClass();
}

bool KeyPressHandlerFacade::needEnsureCursorVisible() const
{
    return d->prepareHandler->needEnsureCursorVisible();
}

bool KeyPressHandlerFacade::needPrehandle() const
{
    return d->prepareHandler->needPrehandle();
}

KeyPressHandlerFacade::KeyPressHandlerFacade(TextEdit* _editor)
    : d(new Implementation(_editor))
{
}

AbstractKeyHandler* KeyPressHandlerFacade::handlerFor(TextParagraphType _type)
{
    switch (_type) {
        case TextParagraphType::Heading1:
        case TextParagraphType::Heading2:
        case TextParagraphType::Heading3:
        case TextParagraphType::Heading4:
        case TextParagraphType::Heading5:
        case TextParagraphType::Heading6: {
            return d->headingHandler.data();
        }

        case TextParagraphType::Text: {
            return d->textHandler.data();
        }

        case TextParagraphType::InlineNote: {
            return d->inlineNoteHandler.data();
        }

        default: {
            return nullptr;
        }
    }
}

} // namespace KeyProcessingLayer
