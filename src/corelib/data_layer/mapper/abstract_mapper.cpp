#include "abstract_mapper.h"

#include <data_layer/database.h>
#include <domain/domain_object.h>

#include <QDebug>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

using DatabaseLayer::Database;
using Domain::DomainObject;
using Domain::Identifier;


namespace DataMappingLayer {

void AbstractMapper::clear()
{
    m_isLastIdentifierLoaded = false;
    for (auto& [key, value] : m_loadedObjectsMap) {
        delete value;
        value = nullptr;
    }
    m_loadedObjectsMap.clear();
}

DomainObject* AbstractMapper::abstractFind(const Identifier& _id)
{
    const auto objectIter = m_loadedObjectsMap.find(_id);
    DomainObject* result = nullptr;
    if (objectIter != m_loadedObjectsMap.cend()) {
        result = objectIter->second;
    } else {
        result = loadObjectFromDatabase(_id);
    }
    return result;
}

QVector<Domain::DomainObject*> AbstractMapper::abstractFind(const QString& _filter)
{
    QSqlQuery query = Database::query();
    query.prepare(findAllStatement() + _filter);
    query.exec();
    QVector<Domain::DomainObject*> result;
    while (query.next()) {
        QSqlRecord record = query.record();
        DomainObject* domainObject = load(record);
        result.append(domainObject);
    }
    return result;
}

bool AbstractMapper::abstractInsert(DomainObject* _object)
{
    //
    // Установим идентификатор для нового объекта
    //
    _object->setId(findNextIdentifier());

    //
    // Добавим вновь созданный объект в список загруженных объектов
    //
    m_loadedObjectsMap.emplace(_object->id(), _object);

    //
    // Получим данные для формирования запроса на их добавление
    //
    QVariantList insertValues;
    QString insertQueryString = insertStatement(_object, insertValues);

    //
    // Сформируем запрос на добавление данных в базу
    //
    QSqlQuery insertQuery = Database::query();
    insertQuery.prepare(insertQueryString);
    for (const QVariant& value : std::as_const(insertValues)) {
        insertQuery.addBindValue(value);
    }

    //
    // Добавим данные в базу
    //
    const auto isInsertSuccesful = executeSql(insertQuery);
    if (isInsertSuccesful) {
        _object->markChangesStored();
    }
    return isInsertSuccesful;
}

bool AbstractMapper::abstractUpdate(DomainObject* _object)
{
    //
    // Обновления не было
    //
    if (_object->isChangesStored()) {
        return false;
    }

    //
    // т.к. в m_loadedObjectsMap хранится список указателей, то после обновления элементов
    // обновлять элемент непосредственно в списке не нужно
    //

    //
    // Получим данные для формирования запроса на их обновление
    //
    QVariantList updateValues;
    const QString updateQueryString = updateStatement(_object, updateValues);

    //
    // Сформируем запрос на обновление данных в базе
    //
    QSqlQuery updateQuery = Database::query();
    updateQuery.prepare(updateQueryString);
    for (const QVariant& value : std::as_const(updateValues)) {
        updateQuery.addBindValue(value);
    }

    //
    // Обновим данные в базе
    //
    const bool isUpdateSuccesful = executeSql(updateQuery);
    if (isUpdateSuccesful) {
        _object->markChangesStored();
    }
    return isUpdateSuccesful;
}

bool AbstractMapper::abstractDelete(DomainObject* _object)
{
    //
    // Получим данные для формирования запроса на их удаление
    //
    QVariantList deleteValues;
    QString deleteQueryString = deleteStatement(_object, deleteValues);

    //
    // Сформируем запрос на удаление данных из базы
    //
    QSqlQuery deleteQuery = Database::query();
    deleteQuery.prepare(deleteQueryString);
    for (const QVariant& value : std::as_const(deleteValues)) {
        deleteQuery.addBindValue(value);
    }

    //
    // Удалим данные из базы
    //
    const bool isDeleteSuccesful = executeSql(deleteQuery);
    if (isDeleteSuccesful) {
        //
        // Удалим объекст из списка загруженных
        //
        m_loadedObjectsMap.erase(_object->id());
        delete _object;
        _object = nullptr;
    }
    return isDeleteSuccesful;
}

bool AbstractMapper::executeSql(QSqlQuery& _sqlQuery)
{
    const bool isExecutionSuccesful = _sqlQuery.exec();
    if (isExecutionSuccesful) {
        return true;
    }

    //
    // Если запрос завершился с ошибкой, выводим отладочную информацию
    //
    Database::setLastError(_sqlQuery.lastError().text());

    qDebug() << _sqlQuery.lastError();
    qDebug() << _sqlQuery.lastQuery();
    qDebug() << _sqlQuery.boundValues();

    return false;
}

DomainObject* AbstractMapper::loadObjectFromDatabase(const Identifier& _id)
{
    QSqlQuery query = Database::query();
    query.prepare(findStatement(_id));
    query.exec();
    query.next();
    const QSqlRecord record = query.record();
    DomainObject* result = load(record);
    return result;
}

Identifier AbstractMapper::findNextIdentifier()
{
    if (!m_isLastIdentifierLoaded) {
        //
        // Если нет ещё последнего индекса по таблице, загрузим его
        //
        QSqlQuery query = Database::query();
        query.prepare(findLastOneStatement());
        query.exec();
        query.next();
        const QSqlRecord record = query.record();
        load(record);

        m_isLastIdentifierLoaded = true;
    }

    Identifier maxId(0);
    if (!m_loadedObjectsMap.empty()) {
        auto iter = m_loadedObjectsMap.cend();
        --iter;
        maxId = iter->first;
    }
    return maxId.next();
}

DomainObject* AbstractMapper::load(const QSqlRecord& _record)
{
    const int idValue = _record.value("id").toInt();
    if (idValue == 0) {
        return nullptr;
    }

    Identifier id(idValue);
    DomainObject* result = nullptr;
    //
    // Если объект загружен, используем указатель на него
    //
    const auto iter = m_loadedObjectsMap.find(id);
    if (iter != m_loadedObjectsMap.cend()) {
        result = iter->second;
        //
        // ... если данные не были изменены, обновляем объект
        //
        if (result->isChangesStored()) {
            doLoad(result, _record);
        }
    }
    //
    // В противном случае создаём новый объект и сохраняем указатель на него
    //
    else {
        result = doLoad(id, _record);
        m_loadedObjectsMap.emplace(id, result);
    }
    return result;
}

} // namespace DataMappingLayer
