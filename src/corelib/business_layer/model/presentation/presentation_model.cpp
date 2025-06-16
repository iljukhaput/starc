#include "presentation_model.h"

#include <business_layer/model/abstract_raw_data_wrapper.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_object.h>
#include <private/qzipreader_p.h>
#include <utils/helpers/image_helper.h>
#include <utils/helpers/text_helper.h>

#include <QBuffer>
#include <QDomDocument>
#include <QtConcurrent>

namespace BusinessLayer {

namespace {
const QLatin1String kDocumentKey("document");
const QLatin1String kNameKey("name");
const QLatin1String kDescriptionKey("description");
const QLatin1String kZipArchiveKey("zip_archive");
const QLatin1String kImageSizesKey("image_sizes");
const QLatin1String kImageSizeKey("size");
const QLatin1String kImageWidthKey("width");
const QLatin1String kImageHeightKey("height");

} // namespace


class PresentationModel::Implementation
{
public:
    explicit Implementation(PresentationModel* _q);

    /**
     * @brief Загрузить следующие в очереди изображения
     */
    void loadNextImagesAsync();


    PresentationModel* q = nullptr;

    QString name;
    QString description;
    QVector<QSize> imageSizes;

    /**
     * @brief Архив с изображениями
     */
    QByteArray zipArchive;
    QUuid zipArchiveUuid;

    /**
     * @brief Следующий запрос на загрузку
     */
    struct {
        QUuid uuid;
        int begin = -1;
        int length = -1;
    } nextQuery;

    /**
     * @brief Загружаются ли сейчас новые изображения
     */
    bool isLoadingNewImages = false;
};

PresentationModel::Implementation::Implementation(PresentationModel* _q)
    : q(_q)
{
}

void PresentationModel::Implementation::loadNextImagesAsync()
{
    if (nextQuery.uuid.isNull()) {
        isLoadingNewImages = false;
        return;
    }

    QtConcurrent::run(
        [this, queryUuid = nextQuery.uuid, begin = nextQuery.begin, length = nextQuery.length]() {
            //
            // Обработать ошибку в основном потоке
            //
            auto processFail = [this, queryUuid]() {
                QMetaObject::invokeMethod(
                    q,
                    [this, queryUuid]() {
                        emit q->imagesLoadingFailed(queryUuid);
                        loadNextImagesAsync();
                    },
                    Qt::QueuedConnection);
            };

            if (zipArchive.isEmpty()) {
                processFail();
                return;
            }

            QBuffer buffer(&zipArchive);
            if (!buffer.open(QIODevice::ReadOnly)) {
                processFail();
                return;
            }

            QZipReader zip(&buffer);
            if (!zip.isReadable()) {
                processFail();
                return;
            }

            auto fileInfoList = zip.fileInfoList();
            const bool outOfRange = fileInfoList.size() < begin + length;
            Q_ASSERT(!outOfRange);

            //
            // Сортируем по имени
            //
            std::sort(fileInfoList.begin(), fileInfoList.end(),
                      [](const QZipReader::FileInfo& a, const QZipReader::FileInfo& b) {
                          return a.filePath < b.filePath;
                      });

            QVector<QImage> images;
            const auto maxSize = ImageHelper::maxSize();
            for (int i = begin; i < begin + length; ++i) {
                QByteArray data = zip.fileData(fileInfoList[i].filePath);
                if (data.isEmpty()) {
                    continue;
                }
                QImage image;
                image.loadFromData(data);
                //
                // Сжимаем изображение, если его размер больше максимального
                //
                if (image.width() > maxSize.width() || image.height() > maxSize.height()) {
                    image = image.scaled(maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                images.append(image);
            }

            //
            // В основном потоке отправляем сигнал об окончании загрузки и запрос на загрузку
            // следующих
            //
            QMetaObject::invokeMethod(
                q,
                [this, queryUuid, images, begin]() {
                    emit q->imagesLoaded(queryUuid, images, begin);
                    loadNextImagesAsync();
                },
                Qt::QueuedConnection);
        });

    nextQuery = { {}, -1, -1 };
}


// ****


PresentationModel::PresentationModel(QObject* _parent)
    : AbstractModel({}, _parent)
    , d(new Implementation(this))
{
    connect(this, &PresentationModel::nameChanged, this, &PresentationModel::updateDocumentContent);
    connect(this, &PresentationModel::descriptionChanged, this,
            &PresentationModel::updateDocumentContent);
}

PresentationModel::~PresentationModel() = default;

QString PresentationModel::documentName() const
{
    return d->name;
}

void PresentationModel::setDocumentName(const QString& _name)
{
    if (d->name == _name) {
        return;
    }

    d->name = _name;
    emit nameChanged(d->name);
    emit documentNameChanged(d->name);
}

QString PresentationModel::description() const
{
    return d->description;
}

void PresentationModel::setDescription(const QString& _description)
{
    if (d->description == _description) {
        return;
    }

    d->description = _description;
    emit descriptionChanged(d->description);
}

QVector<QSize> PresentationModel::imageSizes() const
{
    return d->imageSizes;
}

QUuid PresentationModel::zipArchiveUuid() const
{
    return d->zipArchiveUuid;
}

QUuid PresentationModel::setContent(const QByteArray& _zipArchive, const QVector<QSize>& _sizes)
{
    //
    // Если в модели уже был установлен архив, то удалим его
    //
    if (!d->zipArchiveUuid.isNull()) {
        rawDataWrapper()->remove(d->zipArchiveUuid);
        d->zipArchiveUuid = QUuid();
        d->zipArchive = {};
    }
    //
    // Положим архив в хранилище
    //
    const auto uuid = rawDataWrapper()->save(_zipArchive, Domain::DocumentObjectType::ZipArchive);

    d->zipArchive = _zipArchive;
    d->zipArchiveUuid = uuid;
    d->imageSizes = _sizes;

    updateDocumentContent();
    return uuid;
}

QUuid PresentationModel::loadImagesAsync(int _begin, int _length)
{
    d->nextQuery.uuid = QUuid::createUuid();
    d->nextQuery.begin = _begin;
    d->nextQuery.length = _length;

    if (d->isLoadingNewImages) {
        return d->nextQuery.uuid;
    }

    d->isLoadingNewImages = true;
    const bool success = QMetaObject::invokeMethod(
        this, [this]() { d->loadNextImagesAsync(); }, Qt::QueuedConnection);
    Q_ASSERT(success);

    return d->nextQuery.uuid;
}

void PresentationModel::initDocument()
{
    if (document() == nullptr) {
        return;
    }

    QDomDocument domDocument;
    domDocument.setContent(document()->content());
    const auto documentNode = domDocument.firstChildElement(kDocumentKey);
    auto load = [&documentNode](const QString& _key) {
        return TextHelper::fromHtmlEscaped(documentNode.firstChildElement(_key).text());
    };
    d->name = load(kNameKey);
    d->description = load(kDescriptionKey);
    d->zipArchiveUuid = load(kZipArchiveKey);
    d->zipArchive = rawDataWrapper()->load(d->zipArchiveUuid);

    const auto imageSizesNode = documentNode.firstChildElement(kImageSizesKey);
    if (imageSizesNode.isNull()) {
        return;
    }

    auto imageSizeNode = imageSizesNode.firstChildElement(kImageSizeKey);
    while (!imageSizeNode.isNull()) {
        QSize size;
        size.setWidth(imageSizeNode.attribute(kImageWidthKey, "0").toInt());
        size.setHeight(imageSizeNode.attribute(kImageHeightKey, "0").toInt());
        d->imageSizes.append(size);
        imageSizeNode = imageSizeNode.nextSiblingElement();
    }
}

void PresentationModel::clearDocument()
{
    d.reset(new Implementation(this));
}

QByteArray PresentationModel::toXml() const
{
    if (document() == nullptr) {
        return {};
    }

    QByteArray xml = "<?xml version=\"1.0\"?>\n";
    xml += QString("<%1 mime-type=\"%2\" version=\"1.0\">\n")
               .arg(kDocumentKey, Domain::mimeTypeFor(document()->type()))
               .toUtf8();
    auto save = [&xml](const QString& _key, const QString& _value) {
        xml += QString("<%1><![CDATA[%2]]></%1>\n")
                   .arg(_key, TextHelper::toHtmlEscaped(_value))
                   .toUtf8();
    };
    save(kNameKey, d->name);
    save(kDescriptionKey, d->description);
    save(kZipArchiveKey, d->zipArchiveUuid.toString());
    if (!d->imageSizes.isEmpty()) {
        xml += QString("<%1>\n").arg(kImageSizesKey).toUtf8();
        for (int i = 0; i < d->imageSizes.size(); ++i) {
            xml += QString("<%1 %2=\"%3\" %4=\"%5\"/>\n")
                       .arg(kImageSizeKey, kImageWidthKey,
                            QString::number(d->imageSizes[i].width()), kImageHeightKey,
                            QString::number(d->imageSizes[i].height()))
                       .toUtf8();
        }
        xml += QString("</%1>\n").arg(kImageSizesKey).toUtf8();
    }
    xml += QString("</%1>\n").arg(kDocumentKey).toUtf8();

    return xml;
}

} // namespace BusinessLayer
