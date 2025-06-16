#pragma once

#include "../abstract_model.h"

#include <QImage>

namespace BusinessLayer {

/**
 * @brief Модель презентации
 */
class CORE_LIBRARY_EXPORT PresentationModel : public AbstractModel
{
    Q_OBJECT

public:
    explicit PresentationModel(QObject* _parent = nullptr);
    ~PresentationModel() override;

    QString documentName() const override;
    void setDocumentName(const QString& _name) override;
    Q_SIGNAL void nameChanged(const QString& _name);

    QString description() const;
    void setDescription(const QString& _description);
    Q_SIGNAL void descriptionChanged(const QString& _description);

    /**
     * @brief Размеры изображений
     */
    QVector<QSize> imageSizes() const;

    /**
     * @brief Uuid документа zip-архива
     */
    QUuid zipArchiveUuid() const;

    /**
     * @brief Установить контент презентации (zip-врхив с изображениями и размеры этих изображений)
     * @return Uuid zip-архива
     */
    QUuid setContent(const QByteArray& _zipArchive, const QVector<QSize>& _sizes);

    /**
     * @brief Загрузить изображения из zip-архива
     */
    QUuid loadImagesAsync(int _begin, int _length);
    Q_SIGNAL void imagesLoaded(const QUuid& _queryUuid, const QVector<QImage>& _images, int _begin);
    Q_SIGNAL void imagesLoadingFailed(const QUuid& _queryUuid);

    /**
     * @brief Запрос на загрузку презентации
     */
    Q_SIGNAL void downloadPresentationRequested(const QString& _filePath);

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
