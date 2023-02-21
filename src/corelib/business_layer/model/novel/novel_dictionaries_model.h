#pragma once

#include "../abstract_model.h"


namespace BusinessLayer {

class CORE_LIBRARY_EXPORT NovelDictionariesModel : public AbstractModel
{
    Q_OBJECT

public:
    explicit NovelDictionariesModel(QObject* _parent = nullptr);
    ~NovelDictionariesModel() override;

    const QVector<QString>& sceneIntros() const;
    void addSceneIntro(const QString& _intro);
    void setSceneIntro(int _index, const QString& _intro);
    void removeSceneIntro(int _index);
    Q_SIGNAL void sceneIntrosChanged();

    const QVector<QString>& sceneTimes() const;
    void addSceneTime(const QString& _time);
    void setSceneTime(int _index, const QString& _time);
    void removeSceneTime(int _index);
    Q_SIGNAL void sceneTimesChanged();

    const QVector<QString>& characterExtensions() const;
    void addCharacterExtension(const QString& _extension);
    void setCharacterExtension(int _index, const QString& _extension);
    void removeCharacterExtension(int _index);
    Q_SIGNAL void charactersExtensionsChanged();

    const QVector<QString>& transitions() const;
    void addTransition(const QString& _transition);
    void setTransition(int _index, const QString& _transition);
    void removeTransition(int _index);
    Q_SIGNAL void transitionsChanged();

    QVector<QString> storyDays() const;
    void addStoryDay(const QString& _day);
    void removeStoryDay(const QString& _day);
    Q_SIGNAL void storyDaysChanged();

    QVector<QPair<QString, QColor>> tags() const;
    void addTags(const QVector<QPair<QString, QColor>>& _tags);
    void removeTags(const QVector<QPair<QString, QColor>>& _tags);
    Q_SIGNAL void tagsChanged();

protected:
    /**
     * @brief Реализация модели для работы с документами
     */
    /** @{ */
    void initDocument() override;
    void clearDocument() override;
    QByteArray toXml() const override;
    /** @} */

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace BusinessLayer