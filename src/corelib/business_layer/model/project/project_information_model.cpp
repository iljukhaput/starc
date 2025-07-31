#include "project_information_model.h"

#include <business_layer/model/abstract_image_wrapper.h>
#include <business_layer/model/abstract_model_xml.h>
#include <domain/document_object.h>
#include <utils/diff_match_patch/diff_match_patch_controller.h>
#include <utils/helpers/image_helper.h>
#include <utils/helpers/text_helper.h>
#include <utils/logging.h>

#include <QDomDocument>


namespace BusinessLayer {

namespace {
const QString kDocumentKey = QStringLiteral("document");
const QString kNameKey = QStringLiteral("name");
const QString kLoglineKey = QStringLiteral("logline");
const QString kCoverKey = QStringLiteral("cover");
} // namespace

class ProjectInformationModel::Implementation
{
public:
    QString name;
    QString logline;
    Domain::DocumentImage cover;

    StructureModel* structureModel = nullptr;

    QVector<Domain::ProjectCollaboratorInfo> collaborators;
    QVector<Domain::TeamMemberInfo> teammates;
};


// ****


ProjectInformationModel::ProjectInformationModel(QObject* _parent)
    : AbstractModel({ kDocumentKey, kNameKey, kLoglineKey, kCoverKey }, _parent)
    , d(new Implementation)
{
    connect(this, &ProjectInformationModel::nameChanged, this,
            &ProjectInformationModel::updateDocumentContent);
    connect(this, &ProjectInformationModel::loglineChanged, this,
            &ProjectInformationModel::updateDocumentContent);
    connect(this, &ProjectInformationModel::coverChanged, this,
            &ProjectInformationModel::updateDocumentContent);
}

ProjectInformationModel::~ProjectInformationModel() = default;

const QString& ProjectInformationModel::name() const
{
    return d->name;
}

void ProjectInformationModel::setName(const QString& _name)
{
    if (d->name == _name) {
        return;
    }

    d->name = _name;
    emit nameChanged(d->name);
    emit documentNameChanged(d->name);
}

QString ProjectInformationModel::documentName() const
{
    return name();
}

void ProjectInformationModel::setDocumentName(const QString& _name)
{
    setName(_name);
}

const QString& ProjectInformationModel::logline() const
{
    return d->logline;
}

void ProjectInformationModel::setLogline(const QString& _logline)
{
    if (d->logline == _logline) {
        return;
    }

    d->logline = _logline;
    emit loglineChanged(d->logline);
}

const QPixmap& ProjectInformationModel::cover() const
{
    return d->cover.image;
}

void ProjectInformationModel::setCover(const QPixmap& _cover)
{
    //
    // Если изображения одинаковые, или если оба пусты, то ничего не делаем
    //
    if (_cover.cacheKey() == d->cover.image.cacheKey()
        || (_cover.isNull() && d->cover.image.isNull())) {
        return;
    }

    //
    // Если ранее обложка была задана, то удалим её
    //
    if (!d->cover.uuid.isNull()) {
        imageWrapper()->remove(d->cover.uuid);
        d->cover = {};
    }

    //
    // Если задана новая обложка, то добавляем её
    //
    if (!_cover.isNull()) {
        d->cover.uuid = imageWrapper()->save(_cover);
        d->cover.image = _cover;
    }

    emit coverChanged(d->cover.image);
}

void ProjectInformationModel::setCover(const QUuid& _uuid, const QPixmap& _cover)
{
    if (d->cover.uuid == _uuid && _cover.cacheKey() == d->cover.image.cacheKey()) {
        return;
    }

    d->cover.image = _cover;
    if (d->cover.uuid != _uuid) {
        d->cover.uuid = _uuid;
    }
    emit coverChanged(d->cover.image);
}

StructureModel* ProjectInformationModel::structureModel() const
{
    return d->structureModel;
}

void ProjectInformationModel::setStructureModel(StructureModel* _model)
{
    d->structureModel = _model;
}

QVector<Domain::ProjectCollaboratorInfo> ProjectInformationModel::collaborators() const
{
    return d->collaborators;
}

void ProjectInformationModel::setCollaborators(
    const QVector<Domain::ProjectCollaboratorInfo>& _collaborators)
{
    if (d->collaborators == _collaborators) {
        return;
    }

    d->collaborators = _collaborators;
    emit collaboratorsChanged(d->collaborators);
}

QVector<Domain::TeamMemberInfo> ProjectInformationModel::teammates() const
{
    return d->teammates;
}

void ProjectInformationModel::setTeammates(const QVector<Domain::TeamMemberInfo>& _teammates)
{
    if (d->teammates == _teammates) {
        return;
    }

    d->teammates = _teammates;
    emit teammatesChanged(d->teammates);
}

void ProjectInformationModel::initImageWrapper()
{
    connect(imageWrapper(), &AbstractImageWrapper::imageUpdated, this,
            [this](const QUuid& _uuid, const QPixmap& _image) {
                if (_uuid != d->cover.uuid) {
                    return;
                }

                setCover(_uuid, _image);
            });
}

void ProjectInformationModel::initDocument()
{
    if (document() == nullptr) {
        return;
    }

    QDomDocument domDocument;
    domDocument.setContent(document()->content());
    const auto documentNode = domDocument.firstChildElement(kDocumentKey);
    d->name = documentNode.firstChildElement(kNameKey).text();
    d->logline = documentNode.firstChildElement(kLoglineKey).text();
    d->cover.uuid = QUuid::fromString(documentNode.firstChildElement(kCoverKey).text());
    d->cover.image = imageWrapper()->load(d->cover.uuid);
}

void ProjectInformationModel::clearDocument()
{
    //
    // Сохраняем указатель на модель структуры, чтобы после очистки восстановить его
    //
    auto structureModel = d->structureModel;

    d.reset(new Implementation);

    d->structureModel = structureModel;
}

QByteArray ProjectInformationModel::toXml() const
{
    if (document() == nullptr) {
        return {};
    }

    QByteArray xml = "<?xml version=\"1.0\"?>\n";
    xml += QString("<%1 mime-type=\"%2\" version=\"1.0\">\n")
               .arg(kDocumentKey, Domain::mimeTypeFor(document()->type()))
               .toUtf8();
    xml += QString("<%1><![CDATA[%2]]></%1>\n").arg(kNameKey, d->name).toUtf8();
    xml += QString("<%1><![CDATA[%2]]></%1>\n").arg(kLoglineKey, d->logline).toUtf8();
    xml += QString("<%1><![CDATA[%2]]></%1>\n").arg(kCoverKey, d->cover.uuid.toString()).toUtf8();
    xml += QString("</%1>").arg(kDocumentKey).toUtf8();
    return xml;
}

ChangeCursor ProjectInformationModel::applyPatch(const QByteArray& _patch)
{
    //
    // Определить область изменения в xml
    //
    auto changes = dmpController().changedXml(toXml(), _patch);
    if (changes.first.xml.isEmpty() && changes.second.xml.isEmpty()) {
        Log::warning("Project information model patch don't lead to any changes");
        return {};
    }

    changes.second.xml = xml::prepareXml(changes.second.xml);

    QDomDocument domDocument;
    domDocument.setContent(changes.second.xml);
    const auto documentNode = domDocument.firstChildElement(kDocumentKey);
    if (auto nameNode = documentNode.firstChildElement(kNameKey); !nameNode.isNull()) {
        setName(nameNode.text());
    }
    if (auto loglineNode = documentNode.firstChildElement(kLoglineKey); !loglineNode.isNull()) {
        setLogline(loglineNode.text());
    }
    if (auto coverNode = documentNode.firstChildElement(kCoverKey); !coverNode.isNull()) {
        const auto coverUuid = QUuid(coverNode.text());
        setCover(coverUuid, imageWrapper()->load(coverUuid));
    }

    return {};
}

} // namespace BusinessLayer
