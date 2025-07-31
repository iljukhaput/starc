#pragma once

#include <business_layer/model/text/text_model_group_item.h>

#include <chrono>


namespace BusinessLayer {

class NovelTextModel;

/**
 * @brief Класс элементов сцен модели сценария
 */
class CORE_LIBRARY_EXPORT NovelTextModelSceneItem : public TextModelGroupItem
{
public:
    /**
     * @brief Роли данных из модели
     */
    enum {
        SceneWordCountRole = TextModelGroupItem::GroupUserRole + 1,
        SceneCharacterCountRole,
        SceneCharacterCountWithSpacesRole,
        SceneDescriptionRole,
    };

public:
    explicit NovelTextModelSceneItem(const NovelTextModel* _model);
    ~NovelTextModelSceneItem() override;

    /**
     * @brief Список битов
     */
    QVector<QString> beats() const;

    /**
     * @brief Описание сцены (объединённые биты)
     */
    QString description(const QString& _separator = " ") const;

    /**
     * @brief Определяем интерфейс получения данных сцены
     */
    QVariant data(int _role) const override;

    /**
     * @brief Скопировать контент с заданного элемента
     */
    void copyFrom(TextModelItem* _item) override;

    /**
     * @brief Проверить равен ли текущий элемент заданному
     */
    bool isEqual(TextModelItem* _item) const override;

    /**
     * @brief Подходит ли элемент под условия заданного фильтра
     */
    bool isFilterAccepted(const QString& _text, bool _isCaseSensitive,
                          int _filterType) const override;

protected:
    /**
     * @brief Считываем дополнительный контент
     */
#if (QT_VERSION > QT_VERSION_CHECK(6, 0, 0))
    QStringView readCustomContent(QXmlStreamReader& _contentReader) override;
#else
    QStringRef readCustomContent(QXmlStreamReader& _contentReader) override;
#endif

    /**
     * @brief Сформировать xml-блок с кастомными данными элемента
     */
    QByteArray customContent() const override;

    /**
     * @brief Обновляем текст сцены при изменении кого-то из детей
     */
    void handleChange() override;

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace BusinessLayer
