#include "class_transfer_json_codec.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QObject>
#include <QSaveFile>
#include <QSet>

namespace
{
const QString FormatName = QStringLiteral("ClassMngr Classes");

Result<QString> requiredString(
    const QJsonObject& object,
    const QString& key,
    const QString& context,
    bool allowEmpty = true
    )
{
    const QJsonValue value = object.value(key);

    if (!value.isString())
    {
        return std::unexpected(
            QObject::tr("%1.%2 must be a string.").arg(context, key)
            );
    }

    const QString result = value.toString();

    if (!allowEmpty && result.trimmed().isEmpty())
    {
        return std::unexpected(
            QObject::tr("%1.%2 must not be empty.").arg(context, key)
            );
    }

    return result;
}

Result<QString> optionalString(
    const QJsonObject& object,
    const QString& key,
    const QString& context
    )
{
    if (!object.contains(key))
    {
        return QString();
    }

    return requiredString(object, key, context);
}

Result<QJsonObject> requiredObject(
    const QJsonObject& object,
    const QString& key,
    const QString& context
    )
{
    const QJsonValue value = object.value(key);

    if (!value.isObject())
    {
        return std::unexpected(
            QObject::tr("%1.%2 must be an object.").arg(context, key)
            );
    }

    return value.toObject();
}

Result<QJsonArray> requiredArray(
    const QJsonObject& object,
    const QString& key,
    const QString& context
    )
{
    const QJsonValue value = object.value(key);

    if (!value.isArray())
    {
        return std::unexpected(
            QObject::tr("%1.%2 must be an array.").arg(context, key)
            );
    }

    return value.toArray();
}

QJsonObject teacherToJson(
    const ClassTransferTeacher& transferTeacher
    )
{
    const Teacher& teacher = transferTeacher.teacher;

    return {
        {QStringLiteral("key"), transferTeacher.key},
        {QStringLiteral("teacher_kr"), teacher.teacherKr},
        {QStringLiteral("teacher_en"), teacher.teacherEn},
        {QStringLiteral("preferred_romanization"),
         teacher.preferredRomanization},
        {QStringLiteral("preferred_name"), teacher.preferredName},
        {QStringLiteral("room_number"), teacher.roomNumber},
        {QStringLiteral("birthday"), teacher.birthday},
        {QStringLiteral("phone_number"), teacher.phoneNumber},
        {QStringLiteral("wifi_name"), teacher.wifiName},
        {QStringLiteral("wifi_password"), teacher.wifiPassword},
        {QStringLiteral("internet_type"), teacher.internetType},
        {QStringLiteral("zoom_id"), teacher.zoomId},
        {QStringLiteral("zoom_password"), teacher.zoomPassword},
        {QStringLiteral("projection_type"), teacher.projectionType},
        {QStringLiteral("notes"), teacher.notes}
    };
}

Result<ClassTransferTeacher> teacherFromJson(
    const QJsonObject& object,
    int index
    )
{
    const QString context =
        QStringLiteral("teachers[%1]").arg(index);

    ClassTransferTeacher result;
    Teacher& teacher = result.teacher;

    const QList<QPair<QString, QString*>> fields{
        {QStringLiteral("key"), &result.key},
        {QStringLiteral("teacher_kr"), &teacher.teacherKr},
        {QStringLiteral("teacher_en"), &teacher.teacherEn},
        {QStringLiteral("room_number"), &teacher.roomNumber},
        {QStringLiteral("wifi_name"), &teacher.wifiName},
        {QStringLiteral("wifi_password"), &teacher.wifiPassword},
        {QStringLiteral("internet_type"), &teacher.internetType},
        {QStringLiteral("zoom_id"), &teacher.zoomId},
        {QStringLiteral("zoom_password"), &teacher.zoomPassword},
        {QStringLiteral("projection_type"), &teacher.projectionType},
        {QStringLiteral("notes"), &teacher.notes}
    };

    for (const auto& [key, destination] : fields)
    {
        const auto value = requiredString(
            object,
            key,
            context,
            key != QStringLiteral("key")
            );

        if (!value)
        {
            return std::unexpected(value.error());
        }

        *destination = *value;
    }

    const QList<QPair<QString, QString*>> optionalFields{
        {QStringLiteral("preferred_romanization"),
         &teacher.preferredRomanization},
        {QStringLiteral("preferred_name"), &teacher.preferredName},
        {QStringLiteral("birthday"), &teacher.birthday},
        {QStringLiteral("phone_number"), &teacher.phoneNumber}
    };

    for (const auto& [key, destination] : optionalFields)
    {
        const auto value = optionalString(object, key, context);

        if (!value)
        {
            return std::unexpected(value.error());
        }

        *destination = *value;
    }

    return result;
}

QJsonArray timesToJson(
    const QList<ClassTime>& times
    )
{
    QJsonArray array;

    for (const ClassTime& time : times)
    {
        array.append(QJsonObject{
            {QStringLiteral("day"), time.day},
            {QStringLiteral("start_time"), time.startTime},
            {QStringLiteral("end_time"), time.endTime}
        });
    }

    return array;
}

Result<QList<ClassTime>> timesFromJson(
    const QJsonArray& array,
    const QString& context
    )
{
    QList<ClassTime> times;

    for (int index = 0; index < array.size(); ++index)
    {
        if (!array[index].isObject())
        {
            return std::unexpected(
                QObject::tr("%1[%2] must be an object.").arg(context).arg(index)
                );
        }

        const QJsonObject object = array[index].toObject();
        const QString itemContext =
            QStringLiteral("%1[%2]").arg(context).arg(index);

        const auto day = requiredString(
            object, QStringLiteral("day"), itemContext, false);
        const auto start = requiredString(
            object, QStringLiteral("start_time"), itemContext, false);
        const auto end = requiredString(
            object, QStringLiteral("end_time"), itemContext, false);

        if (!day || !start || !end)
        {
            return std::unexpected(
                !day ? day.error() : !start ? start.error() : end.error()
                );
        }

        times.append({*day, *start, *end});
    }

    return times;
}

QJsonObject classInfoToJson(
    const ClassInfo& info
    )
{
    return {
        {QStringLiteral("class_grade"), info.classGrade},
        {QStringLiteral("class_level"), info.classLevel},
        {QStringLiteral("reading_book"), info.readingBook},
        {QStringLiteral("essay_book"), info.essayBook},
        {QStringLiteral("class_color"), info.classColor},
        {QStringLiteral("font_color"), info.fontColor},
        {QStringLiteral("notes"), info.notes},
        {QStringLiteral("time_filler_activities"), info.timeFillerActivities},
        {QStringLiteral("regular_times"), timesToJson(info.classTimes)},
        {QStringLiteral("intensive_times"), timesToJson(info.intensiveTimes)}
    };
}

Result<ClassInfo> classInfoFromJson(
    const QJsonObject& object,
    const QString& context
    )
{
    ClassInfo info;

    const QList<QPair<QString, QString*>> fields{
        {QStringLiteral("class_grade"), &info.classGrade},
        {QStringLiteral("class_level"), &info.classLevel},
        {QStringLiteral("reading_book"), &info.readingBook},
        {QStringLiteral("essay_book"), &info.essayBook},
        {QStringLiteral("class_color"), &info.classColor},
        {QStringLiteral("font_color"), &info.fontColor},
        {QStringLiteral("notes"), &info.notes},
        {QStringLiteral("time_filler_activities"), &info.timeFillerActivities}
    };

    for (const auto& [key, destination] : fields)
    {
        const auto value = requiredString(object, key, context);

        if (!value)
        {
            return std::unexpected(value.error());
        }

        *destination = *value;
    }

    const auto regularArray = requiredArray(
        object, QStringLiteral("regular_times"), context);
    const auto intensiveArray = requiredArray(
        object, QStringLiteral("intensive_times"), context);

    if (!regularArray || !intensiveArray)
    {
        return std::unexpected(
            !regularArray ? regularArray.error() : intensiveArray.error()
            );
    }

    const auto regular = timesFromJson(
        *regularArray, context + QStringLiteral(".regular_times"));
    const auto intensive = timesFromJson(
        *intensiveArray, context + QStringLiteral(".intensive_times"));

    if (!regular || !intensive)
    {
        return std::unexpected(
            !regular ? regular.error() : intensive.error()
            );
    }

    info.classTimes = *regular;
    info.intensiveTimes = *intensive;

    return info;
}

QJsonObject rosterToJson(
    const Roster& roster
    )
{
    QJsonArray columns;
    QJsonArray widths;
    QJsonArray rows;

    for (const QString& column : roster.columns)
    {
        columns.append(column);
    }

    for (int width : roster.columnWidths)
    {
        widths.append(width);
    }

    for (const QStringList& row : roster.rows)
    {
        QJsonArray rowArray;

        for (const QString& value : row)
        {
            rowArray.append(value);
        }

        rows.append(rowArray);
    }

    return {
        {QStringLiteral("columns"), columns},
        {QStringLiteral("column_widths"), widths},
        {QStringLiteral("rows"), rows}
    };
}

Result<Roster> rosterFromJson(
    const QJsonObject& object,
    const QString& context
    )
{
    const auto columnsArray = requiredArray(
        object, QStringLiteral("columns"), context);
    const auto widthsArray = requiredArray(
        object, QStringLiteral("column_widths"), context);
    const auto rowsArray = requiredArray(
        object, QStringLiteral("rows"), context);

    if (!columnsArray || !widthsArray || !rowsArray)
    {
        return std::unexpected(
            !columnsArray
                ? columnsArray.error()
                : !widthsArray ? widthsArray.error() : rowsArray.error()
            );
    }

    Roster roster;

    for (int index = 0; index < columnsArray->size(); ++index)
    {
        if (!columnsArray->at(index).isString())
        {
            return std::unexpected(
                QObject::tr("%1.columns[%2] must be a string.")
                    .arg(context).arg(index)
                );
        }

        roster.columns.append(columnsArray->at(index).toString());
    }

    if (widthsArray->size() != roster.columns.size())
    {
        return std::unexpected(
            QObject::tr("%1.column_widths must match the column count.")
                .arg(context)
            );
    }

    for (int index = 0; index < widthsArray->size(); ++index)
    {
        const QJsonValue value = widthsArray->at(index);

        if (!value.isDouble())
        {
            return std::unexpected(
                QObject::tr("%1.column_widths[%2] must be a number.")
                    .arg(context).arg(index)
                );
        }

        roster.columnWidths.append(value.toInt());
    }

    for (int rowIndex = 0; rowIndex < rowsArray->size(); ++rowIndex)
    {
        if (!rowsArray->at(rowIndex).isArray())
        {
            return std::unexpected(
                QObject::tr("%1.rows[%2] must be an array.")
                    .arg(context).arg(rowIndex)
                );
        }

        const QJsonArray rowArray = rowsArray->at(rowIndex).toArray();

        if (rowArray.size() != roster.columns.size())
        {
            return std::unexpected(
                QObject::tr("%1.rows[%2] must match the column count.")
                    .arg(context).arg(rowIndex)
                );
        }

        QStringList row;

        for (int column = 0; column < rowArray.size(); ++column)
        {
            if (!rowArray[column].isString())
            {
                return std::unexpected(
                    QObject::tr("%1.rows[%2][%3] must be a string.")
                        .arg(context).arg(rowIndex).arg(column)
                    );
            }

            row.append(rowArray[column].toString());
        }

        roster.rows.append(row);
    }

    return roster;
}

QJsonArray evaluationsToJson(
    const QList<ClassTransferEvaluation>& evaluations
    )
{
    QJsonArray result;

    for (const ClassTransferEvaluation& evaluation : evaluations)
    {
        QJsonArray rows;

        for (const QStringList& row : evaluation.rows)
        {
            QJsonArray values;

            for (const QString& value : row)
            {
                values.append(value);
            }

            rows.append(values);
        }

        result.append(QJsonObject{
            {QStringLiteral("name"), evaluation.name},
            {QStringLiteral("rows"), rows}
        });
    }

    return result;
}

Result<QList<ClassTransferEvaluation>> evaluationsFromJson(
    const QJsonArray& array,
    const QString& context
    )
{
    QList<ClassTransferEvaluation> evaluations;
    QSet<QString> names;

    for (int index = 0; index < array.size(); ++index)
    {
        if (!array[index].isObject())
        {
            return std::unexpected(
                QObject::tr("%1[%2] must be an object.").arg(context).arg(index)
                );
        }

        const QJsonObject object = array[index].toObject();
        const QString itemContext =
            QStringLiteral("%1[%2]").arg(context).arg(index);
        const auto name = requiredString(
            object, QStringLiteral("name"), itemContext, false);
        const auto rows = requiredArray(
            object, QStringLiteral("rows"), itemContext);

        if (!name || !rows)
        {
            return std::unexpected(!name ? name.error() : rows.error());
        }

        if (names.contains(*name))
        {
            return std::unexpected(
                QObject::tr("%1 contains a duplicate evaluation name: %2")
                    .arg(context, *name)
                );
        }

        if (rows->size() > SpeakingEval::RowCount)
        {
            return std::unexpected(
                QObject::tr("%1.rows has too many rows.").arg(itemContext)
                );
        }

        ClassTransferEvaluation evaluation;
        evaluation.name = *name;

        for (int rowIndex = 0; rowIndex < rows->size(); ++rowIndex)
        {
            if (!rows->at(rowIndex).isArray())
            {
                return std::unexpected(
                    QObject::tr("%1.rows[%2] must be an array.")
                        .arg(itemContext).arg(rowIndex)
                    );
            }

            const QJsonArray values = rows->at(rowIndex).toArray();

            if (values.size() != SpeakingEval::ColumnCount)
            {
                return std::unexpected(
                    QObject::tr("%1.rows[%2] must contain %3 columns.")
                        .arg(itemContext)
                        .arg(rowIndex)
                        .arg(SpeakingEval::ColumnCount)
                    );
            }

            QStringList row;

            for (int column = 0; column < values.size(); ++column)
            {
                if (!values[column].isString())
                {
                    return std::unexpected(
                        QObject::tr("%1.rows[%2][%3] must be a string.")
                            .arg(itemContext).arg(rowIndex).arg(column)
                        );
                }

                row.append(values[column].toString());
            }

            evaluation.rows.append(row);
        }

        names.insert(*name);
        evaluations.append(evaluation);
    }

    return evaluations;
}

QJsonObject classToJson(
    const ClassTransferClass& transferClass
    )
{
    return {
        {QStringLiteral("key"), transferClass.key},
        {QStringLiteral("name"), transferClass.name},
        {QStringLiteral("teacher_ref"), transferClass.teacherKey},
        {QStringLiteral("info"), classInfoToJson(transferClass.info)},
        {QStringLiteral("roster"), rosterToJson(transferClass.roster)},
        {QStringLiteral("speaking_evaluations"),
            evaluationsToJson(transferClass.evaluations)}
    };
}

Result<ClassTransferClass> classFromJson(
    const QJsonObject& object,
    int index
    )
{
    const QString context =
        QStringLiteral("classes[%1]").arg(index);
    const auto key = requiredString(
        object, QStringLiteral("key"), context, false);
    const auto name = requiredString(
        object, QStringLiteral("name"), context);
    const auto teacherKey = requiredString(
        object, QStringLiteral("teacher_ref"), context);
    const auto infoObject = requiredObject(
        object, QStringLiteral("info"), context);
    const auto rosterObject = requiredObject(
        object, QStringLiteral("roster"), context);
    const auto evaluationsArray = requiredArray(
        object, QStringLiteral("speaking_evaluations"), context);

    if (!key || !name || !teacherKey || !infoObject
        || !rosterObject || !evaluationsArray)
    {
        if (!key) return std::unexpected(key.error());
        if (!name) return std::unexpected(name.error());
        if (!teacherKey) return std::unexpected(teacherKey.error());
        if (!infoObject) return std::unexpected(infoObject.error());
        if (!rosterObject) return std::unexpected(rosterObject.error());
        return std::unexpected(evaluationsArray.error());
    }

    const auto info = classInfoFromJson(
        *infoObject, context + QStringLiteral(".info"));
    const auto roster = rosterFromJson(
        *rosterObject, context + QStringLiteral(".roster"));
    const auto evaluations = evaluationsFromJson(
        *evaluationsArray,
        context + QStringLiteral(".speaking_evaluations"));

    if (!info || !roster || !evaluations)
    {
        if (!info) return std::unexpected(info.error());
        if (!roster) return std::unexpected(roster.error());
        return std::unexpected(evaluations.error());
    }

    ClassTransferClass result;
    result.key = *key;
    result.name = *name;
    result.teacherKey = *teacherKey;
    result.info = *info;
    result.roster = *roster;
    result.evaluations = *evaluations;

    return result;
}
}

QJsonObject ClassTransferJsonCodec::toJson(
    const ClassTransferPackage& package
    )
{
    QJsonArray teachers;
    QJsonArray classes;

    for (const ClassTransferTeacher& teacher : package.teachers)
    {
        teachers.append(teacherToJson(teacher));
    }

    for (const ClassTransferClass& transferClass : package.classes)
    {
        classes.append(classToJson(transferClass));
    }

    const QDateTime exportedAt = package.exportedAtUtc.isValid()
        ? package.exportedAtUtc.toUTC()
        : QDateTime::currentDateTimeUtc();

    return {
        {QStringLiteral("format"), FormatName},
        {QStringLiteral("version"), package.version},
        {QStringLiteral("exported_at_utc"),
            exportedAt.toString(Qt::ISODateWithMs)},
        {QStringLiteral("teachers"), teachers},
        {QStringLiteral("classes"), classes}
    };
}

Result<ClassTransferPackage> ClassTransferJsonCodec::fromJson(
    const QJsonObject& object
    )
{
    const auto format = requiredString(
        object, QStringLiteral("format"), QStringLiteral("package"), false);

    if (!format)
    {
        return std::unexpected(format.error());
    }

    if (*format != FormatName)
    {
        return std::unexpected(
            QObject::tr("This is not a ClassMngr class package.")
            );
    }

    const QJsonValue versionValue = object.value(QStringLiteral("version"));

    if (!versionValue.isDouble()
        || versionValue.toInt() != versionValue.toDouble())
    {
        return std::unexpected(
            QObject::tr("package.version must be an integer.")
            );
    }

    const int version = versionValue.toInt();

    if (version != ClassTransferPackage::CurrentVersion)
    {
        return std::unexpected(
            QObject::tr("Unsupported class package version: %1").arg(version)
            );
    }

    const auto exportedAtValue = requiredString(
        object,
        QStringLiteral("exported_at_utc"),
        QStringLiteral("package"),
        false
        );
    const auto teachersArray = requiredArray(
        object, QStringLiteral("teachers"), QStringLiteral("package"));
    const auto classesArray = requiredArray(
        object, QStringLiteral("classes"), QStringLiteral("package"));

    if (!exportedAtValue || !teachersArray || !classesArray)
    {
        if (!exportedAtValue) return std::unexpected(exportedAtValue.error());
        if (!teachersArray) return std::unexpected(teachersArray.error());
        return std::unexpected(classesArray.error());
    }

    const QDateTime exportedAt = QDateTime::fromString(
        *exportedAtValue, Qt::ISODateWithMs);

    if (!exportedAt.isValid())
    {
        return std::unexpected(
            QObject::tr("package.exported_at_utc is not a valid timestamp.")
            );
    }

    if (classesArray->isEmpty())
    {
        return std::unexpected(
            QObject::tr("The class package does not contain any classes.")
            );
    }

    ClassTransferPackage package;
    package.version = version;
    package.exportedAtUtc = exportedAt.toUTC();
    QSet<QString> teacherKeys;
    QSet<QString> classKeys;

    for (int index = 0; index < teachersArray->size(); ++index)
    {
        if (!teachersArray->at(index).isObject())
        {
            return std::unexpected(
                QObject::tr("teachers[%1] must be an object.").arg(index)
                );
        }

        auto teacher = teacherFromJson(
            teachersArray->at(index).toObject(), index);

        if (!teacher)
        {
            return std::unexpected(teacher.error());
        }

        if (teacherKeys.contains(teacher->key))
        {
            return std::unexpected(
                QObject::tr("Duplicate teacher key: %1").arg(teacher->key)
                );
        }

        teacherKeys.insert(teacher->key);
        package.teachers.append(*teacher);
    }

    for (int index = 0; index < classesArray->size(); ++index)
    {
        if (!classesArray->at(index).isObject())
        {
            return std::unexpected(
                QObject::tr("classes[%1] must be an object.").arg(index)
                );
        }

        auto transferClass = classFromJson(
            classesArray->at(index).toObject(), index);

        if (!transferClass)
        {
            return std::unexpected(transferClass.error());
        }

        if (classKeys.contains(transferClass->key))
        {
            return std::unexpected(
                QObject::tr("Duplicate class key: %1").arg(transferClass->key)
                );
        }

        if (!transferClass->teacherKey.isEmpty()
            && !teacherKeys.contains(transferClass->teacherKey))
        {
            return std::unexpected(
                QObject::tr("Class %1 references an unknown teacher key: %2")
                    .arg(transferClass->key, transferClass->teacherKey)
                );
        }

        classKeys.insert(transferClass->key);
        package.classes.append(*transferClass);
    }

    return package;
}

Status ClassTransferJsonCodec::saveFile(
    const QString& filePath,
    const ClassTransferPackage& package
    )
{
    if (filePath.trimmed().isEmpty())
    {
        return std::unexpected(QObject::tr("No export path was provided."));
    }

    QSaveFile file(filePath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return std::unexpected(
            QObject::tr("Unable to open the class package for writing:\n%1")
                .arg(file.errorString())
            );
    }

    const QByteArray bytes =
        QJsonDocument(toJson(package)).toJson(QJsonDocument::Indented);

    if (file.write(bytes) != bytes.size())
    {
        file.cancelWriting();
        return std::unexpected(
            QObject::tr("Unable to write the class package:\n%1")
                .arg(file.errorString())
            );
    }

    if (!file.commit())
    {
        return std::unexpected(
            QObject::tr("Unable to finish writing the class package:\n%1")
                .arg(file.errorString())
            );
    }

    return {};
}

Result<ClassTransferPackage> ClassTransferJsonCodec::loadFile(
    const QString& filePath
    )
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return std::unexpected(
            QObject::tr("Unable to open the class package:\n%1")
                .arg(file.errorString())
            );
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        file.readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        return std::unexpected(
            QObject::tr("The class package is not valid JSON:\n%1")
                .arg(parseError.errorString())
            );
    }

    if (!document.isObject())
    {
        return std::unexpected(
            QObject::tr("The class package root must be an object.")
            );
    }

    return fromJson(document.object());
}
