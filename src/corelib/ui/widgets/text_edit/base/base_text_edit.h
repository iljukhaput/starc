#pragma once

#include <ui/widgets/text_edit/completer/completer_text_edit.h>


class CORE_LIBRARY_EXPORT BaseTextEdit : public CompleterTextEdit
{
    Q_OBJECT

public:
    explicit BaseTextEdit(QWidget* _parent = nullptr);
    ~BaseTextEdit() override;

    /**
     * @brief Необходимо ли поднимать регистр первой буквы в предложении
     */
    void setCapitalizeWords(bool _capitalize);

    /**
     * @brief Необходимо ли корректировать проблему ДВойных ЗАглавных
     */
    void setCorrectDoubleCapitals(bool _correct);

    /**
     * @brief Необходимо ли i на I
     */
    void setCapitalizeSingleILetter(bool _capitalize);

    /**
     * @brief Необходимо ли заменять три точки на многоточие
     */
    void setReplaceThreeDots(bool _replace);

    /**
     * @brief Использовать умные кавычки для текущего языка интерфейса
     */
    void setUseSmartQuotes(bool _use);

    /**
     * @brief Заменять два тира на длинное тире
     */
    void setReplaceTwoDashes(bool _replace);

    /**
     * @brief Запретить вводить несколько пробелов подряд
     */
    void setAvoidMultipleSpaces(bool _avoid);

    /**
     * @brief Установить доступность возможности вставки из буфера обмена, как простого текста
     */
    void setPasteAsPlainTextAvailable(bool _available);

    /**
     * @brief Установить доступность пунктов меню форматирования текста
     */
    void setFormattigAvailable(bool _available);

    /**
     * @brief Настроить форматирование выделенного текста
     */
    /** @{ */
    void setTextBold(bool _bold);
    void setTextItalic(bool _italic);
    void setTextUnderline(bool _underline);
    void setTextStrikethrough(bool _strikethrough);
    //
    void invertTextBold();
    void invertTextItalic();
    void invertTextUnderline();
    void invertTextStrikethrough();
    /** @} */

    /**
     * @brief Задать шрифт выделенного текста
     */
    void setTextFont(const QFont& _font);

    /**
     * @brief Задать выравнивание выделенного текста
     */
    void setTextAlignment(Qt::Alignment _alignment);

    /**
     * @brief Поменять регистр букв
     */
    void changeTextCase(bool _upper);

    /**
     * @brief Добавляем опции форматирования в контекстное меню
     */
    ContextMenu* createContextMenu(const QPoint& _position, QWidget* _parent = nullptr) override;

protected:
    /**
     * @brief Перенастраиваем виджет при обновлении дизайн системы
     */
    bool event(QEvent* _event) override;

    /**
     * @brief Переопределяется для обработки тройного клика
     */
    /** @{ */
    void mousePressEvent(QMouseEvent* _event) override;
    void mouseDoubleClickEvent(QMouseEvent* _event) override;
    /** @} */

    /**
     * @brief Дополнительная функция для обработки нажатий самим редактором
     * @return Обработано ли событие
     * @note В наследниках необходимо вызывать вручную при необходимости
     */
    virtual bool keyPressEventReimpl(QKeyEvent* _event);

    /**
     * @brief Скорректировать введённый текст
     *
     * - изменить регистр текста в начале предложения, если это необходимо
     * - исправить проблему ДВойных ЗАглавных
     *
     * @return Был ли изменён текст
     */
    virtual bool updateEnteredText(const QKeyEvent* _event);

    /**
     * @brief Переопределяем, чтобы обработать кейс с установкой курсора, начало, или конец
     * выделения в котором находится в невидимом блоке
     */
    void doSetTextCursor(const QTextCursor& _cursor) override;

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};
