#include "class_transfer_json_codec.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QObject>
#include <QSaveFile>
#include <QSet>

#include <cstring>
#include <limits>

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

// A false result means either invalid JSON or syntax outside the subset this
// validator can prove safe. Callers must use Qt's document parser in both
// cases so its diagnostics and acceptance behavior remain authoritative.
class ConservativeMappedJsonValidator
{
public:
    explicit ConservativeMappedJsonValidator(const QByteArray& bytes)
        : bytes_(bytes)
    {
    }

    bool validatesObjectDocument()
    {
        if (bytes_.startsWith("\xEF\xBB\xBF"))
        {
            position_ = 3;
        }

        skipWhitespace();
        if (atEnd() || peek() != '{' || !parseObject(0))
        {
            return false;
        }

        skipWhitespace();
        return atEnd();
    }

private:
    static constexpr int MaximumContainerDepth = 128;
    static constexpr int MaximumConservativeExponentMagnitude = 10;

    bool atEnd() const { return position_ >= bytes_.size(); }

    unsigned char peek() const
    {
        return static_cast<unsigned char>(bytes_[position_]);
    }

    void advance() { ++position_; }

    void skipWhitespace()
    {
        while (!atEnd())
        {
            const unsigned char token = peek();
            if (token != ' ' && token != '\t' && token != '\r' && token != '\n')
            {
                break;
            }
            advance();
        }
    }

    static bool isDigit(unsigned char token)
    {
        return token >= '0' && token <= '9';
    }

    static bool isContinuation(unsigned char token)
    {
        return token >= 0x80 && token <= 0xBF;
    }

    bool consumeUtf8()
    {
        const unsigned char first = peek();
        if (first >= 0xC2 && first <= 0xDF)
        {
            if (bytes_.size() - position_ < 2
                || !isContinuation(static_cast<unsigned char>(bytes_[position_ + 1])))
            {
                return false;
            }
            position_ += 2;
            return true;
        }

        if (first >= 0xE0 && first <= 0xEF)
        {
            if (bytes_.size() - position_ < 3)
            {
                return false;
            }
            const unsigned char second =
                static_cast<unsigned char>(bytes_[position_ + 1]);
            const unsigned char third =
                static_cast<unsigned char>(bytes_[position_ + 2]);
            const bool validSecond = first == 0xE0
                ? second >= 0xA0 && second <= 0xBF
                : first == 0xED
                    ? second >= 0x80 && second <= 0x9F
                    : isContinuation(second);
            if (!validSecond || !isContinuation(third))
            {
                return false;
            }
            position_ += 3;
            return true;
        }

        if (first >= 0xF0 && first <= 0xF4)
        {
            if (bytes_.size() - position_ < 4)
            {
                return false;
            }
            const unsigned char second =
                static_cast<unsigned char>(bytes_[position_ + 1]);
            const bool validSecond = first == 0xF0
                ? second >= 0x90 && second <= 0xBF
                : first == 0xF4
                    ? second >= 0x80 && second <= 0x8F
                    : isContinuation(second);
            if (!validSecond
                || !isContinuation(static_cast<unsigned char>(bytes_[position_ + 2]))
                || !isContinuation(static_cast<unsigned char>(bytes_[position_ + 3])))
            {
                return false;
            }
            position_ += 4;
            return true;
        }

        return false;
    }

    bool parseString()
    {
        if (atEnd() || peek() != '"')
        {
            return false;
        }
        advance();

        while (!atEnd())
        {
            const unsigned char token = peek();
            if (token == '"')
            {
                advance();
                return true;
            }
            if (token < 0x20)
            {
                return false;
            }
            if (token == '\\')
            {
                advance();
                if (atEnd())
                {
                    return false;
                }
                const unsigned char escaped = peek();
                if (escaped == 'u')
                {
                    // Qt's surrogate handling is deliberately left to the
                    // fallback parser, including otherwise valid \u escapes.
                    return false;
                }
                if (escaped != '"' && escaped != '\\' && escaped != '/'
                    && escaped != 'b' && escaped != 'f' && escaped != 'n'
                    && escaped != 'r' && escaped != 't')
                {
                    return false;
                }
                advance();
                continue;
            }
            if (token >= 0x80)
            {
                if (!consumeUtf8())
                {
                    return false;
                }
                continue;
            }
            advance();
        }

        return false;
    }

    bool parseLiteral(const char* literal, qsizetype length)
    {
        if (bytes_.size() - position_ < length
            || std::memcmp(bytes_.constData() + position_, literal,
                           static_cast<size_t>(length)) != 0)
        {
            return false;
        }
        position_ += length;
        return true;
    }

    bool parseNumber()
    {
        const qsizetype begin = position_;
        if (peek() == '-')
        {
            advance();
            if (atEnd())
            {
                return false;
            }
        }

        if (peek() == '0')
        {
            advance();
            if (!atEnd() && isDigit(peek()))
            {
                return false;
            }
        }
        else if (peek() >= '1' && peek() <= '9')
        {
            do
            {
                advance();
            }
            while (!atEnd() && isDigit(peek()));
        }
        else
        {
            return false;
        }

        if (!atEnd() && peek() == '.')
        {
            advance();
            if (atEnd() || !isDigit(peek()))
            {
                return false;
            }
            do
            {
                advance();
            }
            while (!atEnd() && isDigit(peek()));
        }

        if (!atEnd() && (peek() == 'e' || peek() == 'E'))
        {
            advance();
            if (!atEnd() && (peek() == '+' || peek() == '-'))
            {
                advance();
            }
            if (atEnd() || !isDigit(peek()))
            {
                return false;
            }
            do
            {
                advance();
            }
            while (!atEnd() && isDigit(peek()));
        }

        if (!hasConservativeNumericRange(begin, position_))
        {
            // Long or extreme numbers are delegated to Qt so its conversion
            // limits and exact acceptance behavior remain authoritative.
            return false;
        }
        return true;
    }

    bool hasConservativeNumericRange(qsizetype begin, qsizetype end) const
    {
        // An 18-digit mantissa, a short token, and an exponent no larger than
        // 10 keep values well inside binary64's normal range without relying
        // on platform-specific floating-point conversion routines. Tokens
        // outside these bounds are valid candidates for Qt fallback.
        if (end - begin > 24)
        {
            return false;
        }

        int mantissaDigits = 0;
        int exponentMagnitude = 0;
        int exponentDigits = 0;
        bool inExponent = false;
        for (qsizetype index = begin; index < end; ++index)
        {
            const unsigned char token =
                static_cast<unsigned char>(bytes_[index]);
            if (token == 'e' || token == 'E')
            {
                inExponent = true;
                if (index + 1 < end
                    && (bytes_[index + 1] == '+' || bytes_[index + 1] == '-'))
                {
                    ++index;
                }
                continue;
            }
            if (!isDigit(token))
            {
                continue;
            }
            if (!inExponent)
            {
                if (++mantissaDigits > 18)
                {
                    return false;
                }
                continue;
            }
            if (++exponentDigits > 2)
            {
                return false;
            }
            exponentMagnitude = exponentMagnitude * 10 + (token - '0');
            if (exponentMagnitude > MaximumConservativeExponentMagnitude)
            {
                return false;
            }
        }

        return true;
    }

    bool parseValue(int depth)
    {
        skipWhitespace();
        if (atEnd())
        {
            return false;
        }

        switch (peek())
        {
        case '{': return parseObject(depth);
        case '[': return parseArray(depth);
        case '"': return parseString();
        case 't': return parseLiteral("true", 4);
        case 'f': return parseLiteral("false", 5);
        case 'n': return parseLiteral("null", 4);
        default:
            return peek() == '-' || isDigit(peek()) ? parseNumber() : false;
        }
    }

    bool parseObject(int depth)
    {
        if (depth >= MaximumContainerDepth || atEnd() || peek() != '{')
        {
            return false;
        }
        advance();
        skipWhitespace();
        if (!atEnd() && peek() == '}')
        {
            advance();
            return true;
        }

        while (!atEnd())
        {
            if (peek() != '"' || !parseString())
            {
                return false;
            }
            skipWhitespace();
            if (atEnd() || peek() != ':')
            {
                return false;
            }
            advance();
            if (!parseValue(depth + 1))
            {
                return false;
            }
            skipWhitespace();
            if (!atEnd() && peek() == '}')
            {
                advance();
                return true;
            }
            if (atEnd() || peek() != ',')
            {
                return false;
            }
            advance();
            skipWhitespace();
        }
        return false;
    }

    bool parseArray(int depth)
    {
        if (depth >= MaximumContainerDepth || atEnd() || peek() != '[')
        {
            return false;
        }
        advance();
        skipWhitespace();
        if (!atEnd() && peek() == ']')
        {
            advance();
            return true;
        }

        while (!atEnd())
        {
            if (!parseValue(depth + 1))
            {
                return false;
            }
            skipWhitespace();
            if (!atEnd() && peek() == ']')
            {
                advance();
                return true;
            }
            if (atEnd() || peek() != ',')
            {
                return false;
            }
            advance();
            skipWhitespace();
        }
        return false;
    }

    const QByteArray& bytes_;
    qsizetype position_ = 0;
};

// These cursors only locate values in JSON already proven valid by the
// conservative syntax validator or validated by Qt's full-document parser.
// They do not interpret JSON strings, numbers, duplicate fields, or schema values.
struct JsonRange
{
    qsizetype begin = 0;
    qsizetype end = 0;
};

class ValidatedJsonCursor
{
public:
    explicit ValidatedJsonCursor(const QByteArray& bytes, qsizetype position = 0)
        : bytes_(bytes), position_(position)
    {
    }

    void skipWhitespace()
    {
        while (position_ < bytes_.size()
               && (bytes_[position_] == ' ' || bytes_[position_] == '\t'
                   || bytes_[position_] == '\r' || bytes_[position_] == '\n'))
        {
            ++position_;
        }
    }

    char peek() const { return bytes_[position_]; }
    void advance() { ++position_; }

    JsonRange takeValue()
    {
        skipWhitespace();
        const qsizetype begin = position_;
        if (peek() == '"')
        {
            skipString();
        }
        else if (peek() == '{' || peek() == '[')
        {
            int depth = 0;
            do
            {
                if (peek() == '"')
                {
                    skipString();
                }
                else
                {
                    const char token = peek();
                    advance();
                    if (token == '{' || token == '[') ++depth;
                    else if (token == '}' || token == ']') --depth;
                }
            }
            while (depth > 0);
        }
        else
        {
            while (position_ < bytes_.size() && peek() != ','
                   && peek() != '}' && peek() != ']' && peek() != ' '
                   && peek() != '\t' && peek() != '\r' && peek() != '\n')
            {
                advance();
            }
        }
        return {begin, position_};
    }

private:
    void skipString()
    {
        advance(); // opening quote
        while (peek() != '"')
        {
            if (peek() == '\\') advance();
            advance();
        }
        advance(); // closing quote
    }

    const QByteArray& bytes_;
    qsizetype position_;
};

QByteArray jsonFragment(const QByteArray& bytes, JsonRange range)
{
    return QByteArray::fromRawData(bytes.constData() + range.begin,
                                  range.end - range.begin);
}

QJsonValue scalarFragment(const QByteArray& bytes, JsonRange range)
{
    // An array wrapper lets Qt decode scalar values and escaped property names.
    QByteArray wrapped("[");
    wrapped.append(jsonFragment(bytes, range));
    wrapped.append(']');
    return QJsonDocument::fromJson(wrapped).array().first();
}

Status jsonDocumentStatus(
    const QJsonDocument& document,
    const QJsonParseError& parseError
    )
{
    if (parseError.error != QJsonParseError::NoError)
    {
        return std::unexpected(
            QObject::tr("The class package is not valid JSON:\n%1")
                .arg(parseError.errorString()));
    }
    if (!document.isObject())
    {
        return std::unexpected(
            QObject::tr("The class package root must be an object."));
    }
    return {};
}

Status validateMappedJsonObject(const QByteArray& bytes)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    return jsonDocumentStatus(document, error);
}

bool hasProvenMappedJsonObjectSyntax(const QByteArray& bytes)
{
    return ConservativeMappedJsonValidator(bytes).validatesObjectDocument();
}

Result<ClassTransferPackage> packageFromValidatedMappedJson(
    const QByteArray& bytes
    )
{
    // Only source byte ranges, scalar metadata, and one record's Qt DOM are
    // retained below. The safe mapped path reaches here without constructing a
    // full-file QJsonDocument.
    ValidatedJsonCursor root(bytes, bytes.startsWith("\xEF\xBB\xBF") ? 3 : 0);
    root.skipWhitespace();
    root.advance(); // opening object
    root.skipWhitespace();
    QJsonObject metadata;
    JsonRange teachers;
    JsonRange classes;
    while (root.peek() != '}')
    {
        const QString key = scalarFragment(bytes, root.takeValue()).toString();
        root.skipWhitespace();
        root.advance(); // colon
        const JsonRange value = root.takeValue();
        if (key == QStringLiteral("teachers")) teachers = value;
        else if (key == QStringLiteral("classes")) classes = value;
        else if (key == QStringLiteral("format")
                 || key == QStringLiteral("version")
                 || key == QStringLiteral("exported_at_utc"))
        {
            // Containers cannot satisfy these scalar fields. Avoid building a
            // DOM for even an arbitrarily large invalid metadata container.
            metadata.insert(key, bytes[value.begin] == '{' || bytes[value.begin] == '['
                ? QJsonValue(QJsonValue::Null) : scalarFragment(bytes, value));
        }
        root.skipWhitespace();
        if (root.peek() == ',')
        {
            root.advance();
            root.skipWhitespace();
        }
    }

    // Match fromJson's schema/error precedence independently of root key order.
    const auto format = requiredString(
        metadata, QStringLiteral("format"), QStringLiteral("package"), false);
    if (!format) return std::unexpected(format.error());
    if (*format != FormatName)
        return std::unexpected(QObject::tr("This is not a ClassMngr class package."));
    const QJsonValue versionValue = metadata.value(QStringLiteral("version"));
    if (!versionValue.isDouble() || versionValue.toInt() != versionValue.toDouble())
        return std::unexpected(QObject::tr("package.version must be an integer."));
    const int version = versionValue.toInt();
    if (version != ClassTransferPackage::CurrentVersion)
        return std::unexpected(QObject::tr("Unsupported class package version: %1").arg(version));
    // The timestamp string requirement precedes both array requirements;
    // parsing that string as a timestamp must wait until both arrays exist.
    const auto timestamp = requiredString(metadata, QStringLiteral("exported_at_utc"),
                                         QStringLiteral("package"), false);
    if (!timestamp) return std::unexpected(timestamp.error());
    if (teachers.end == 0 || bytes[teachers.begin] != '[')
        return std::unexpected(QObject::tr("package.teachers must be an array."));
    if (classes.end == 0 || bytes[classes.begin] != '[')
        return std::unexpected(QObject::tr("package.classes must be an array."));
    const QDateTime exportedAt = QDateTime::fromString(*timestamp, Qt::ISODateWithMs);
    if (!exportedAt.isValid())
        return std::unexpected(QObject::tr("package.exported_at_utc is not a valid timestamp."));
    ValidatedJsonCursor classCursor(bytes, classes.begin + 1);
    classCursor.skipWhitespace();
    if (classCursor.peek() == ']')
        return std::unexpected(QObject::tr("The class package does not contain any classes."));

    ClassTransferPackage package;
    package.version = version;
    package.exportedAtUtc = exportedAt.toUTC();
    QSet<QString> teacherKeys;
    QSet<QString> classKeys;
    ValidatedJsonCursor teacherCursor(bytes, teachers.begin + 1);
    teacherCursor.skipWhitespace();
    for (int index = 0; teacherCursor.peek() != ']'; ++index)
    {
        const JsonRange value = teacherCursor.takeValue();
        if (bytes[value.begin] != '{')
            return std::unexpected(QObject::tr("teachers[%1] must be an object.").arg(index));
        auto teacher = teacherFromJson(
            QJsonDocument::fromJson(jsonFragment(bytes, value)).object(), index);
        if (!teacher) return std::unexpected(teacher.error());
        if (teacherKeys.contains(teacher->key))
            return std::unexpected(QObject::tr("Duplicate teacher key: %1").arg(teacher->key));
        teacherKeys.insert(teacher->key);
        package.teachers.append(*teacher);
        teacherCursor.skipWhitespace();
        if (teacherCursor.peek() == ',') teacherCursor.advance();
        teacherCursor.skipWhitespace();
    }
    for (int index = 0; classCursor.peek() != ']'; ++index)
    {
        const JsonRange value = classCursor.takeValue();
        if (bytes[value.begin] != '{')
            return std::unexpected(QObject::tr("classes[%1] must be an object.").arg(index));
        auto transferClass = classFromJson(
            QJsonDocument::fromJson(jsonFragment(bytes, value)).object(), index);
        if (!transferClass) return std::unexpected(transferClass.error());
        if (classKeys.contains(transferClass->key))
            return std::unexpected(QObject::tr("Duplicate class key: %1").arg(transferClass->key));
        if (!transferClass->teacherKey.isEmpty() && !teacherKeys.contains(transferClass->teacherKey))
            return std::unexpected(QObject::tr("Class %1 references an unknown teacher key: %2")
                                       .arg(transferClass->key, transferClass->teacherKey));
        classKeys.insert(transferClass->key);
        package.classes.append(*transferClass);
        classCursor.skipWhitespace();
        if (classCursor.peek() == ',') classCursor.advance();
        classCursor.skipWhitespace();
    }
    return package;
}

using namespace ClassMngr::Next::Application;
QString exportText(const ClassExportText& value) { return QString::fromStdU16String(value); }
QString exportKey(const ClassExportClassKey& value) { return QString::fromStdU16String(value.value()); }
QString exportKey(const ClassExportTeacherKey& value) { return QString::fromStdU16String(value.value()); }
QJsonArray exportRows(const ClassExportRows& rows)
{
    QJsonArray result;
    for (const auto& row : rows)
    {
        QJsonArray cells;
        for (const auto& cell : row)
            cells.append(exportText(cell));
        result.append(cells);
    }
    return result;
}
QJsonArray timesToJson(const std::vector<ClassExportTime>& times)
{
    QJsonArray result;
    for (const auto& time : times)
        result.append(QJsonObject{{QStringLiteral("day"), exportText(time.day)},
            {QStringLiteral("start_time"), exportText(time.startTime)},
            {QStringLiteral("end_time"), exportText(time.endTime)}});
    return result;
}
QJsonObject teacherToJson(const ClassExportTeacher& value)
{
    return {{QStringLiteral("key"), exportKey(value.key)},
        {QStringLiteral("teacher_kr"), exportText(value.teacher.teacherKr)},
        {QStringLiteral("teacher_en"), exportText(value.teacher.teacherEn)},
        {QStringLiteral("preferred_romanization"), exportText(value.teacher.preferredRomanization)},
        {QStringLiteral("preferred_name"), exportText(value.teacher.preferredName)},
        {QStringLiteral("room_number"), exportText(value.teacher.roomNumber)},
        {QStringLiteral("birthday"), exportText(value.teacher.birthday)},
        {QStringLiteral("phone_number"), exportText(value.teacher.phoneNumber)},
        {QStringLiteral("wifi_name"), exportText(value.teacher.wifiName)},
        {QStringLiteral("wifi_password"), exportText(value.teacher.wifiPassword)},
        {QStringLiteral("internet_type"), exportText(value.teacher.internetType)},
        {QStringLiteral("zoom_id"), exportText(value.teacher.zoomId)},
        {QStringLiteral("zoom_password"), exportText(value.teacher.zoomPassword)},
        {QStringLiteral("projection_type"), exportText(value.teacher.projectionType)},
        {QStringLiteral("notes"), exportText(value.teacher.notes)}};
}
QJsonObject classInfoToJson(const ClassExportInfo& value)
{
    return {
        {QStringLiteral("class_grade"), exportText(value.classGrade)},
        {QStringLiteral("class_level"), exportText(value.classLevel)},
        {QStringLiteral("reading_book"), exportText(value.readingBook)},
        {QStringLiteral("essay_book"), exportText(value.essayBook)},
        {QStringLiteral("class_color"), exportText(value.classColor)},
        {QStringLiteral("font_color"), exportText(value.fontColor)},
        {QStringLiteral("notes"), exportText(value.notes)},
        {QStringLiteral("time_filler_activities"), exportText(value.timeFillerActivities)},
        {QStringLiteral("regular_times"), timesToJson(value.classTimes)},
        {QStringLiteral("intensive_times"), timesToJson(value.intensiveTimes)}};
}
QJsonObject rosterToJson(const ClassExportRoster& value)
{
    QJsonArray columns, widths;
    for (const auto& column : value.columns) columns.append(exportText(column));
    for (const auto& width : value.columnWidths) widths.append(width.rawValue());
    return {{QStringLiteral("columns"), columns}, {QStringLiteral("column_widths"), widths},
        {QStringLiteral("rows"), exportRows(value.rows)}};
}
QJsonObject classToJson(const ClassExportClass& value)
{
    QJsonArray evaluations;
    for (const auto& evaluation : value.evaluations)
        evaluations.append(QJsonObject{{QStringLiteral("name"), exportText(evaluation.name)},
            {QStringLiteral("rows"), exportRows(evaluation.rows)}});
    return {{QStringLiteral("key"), exportKey(value.key)}, {QStringLiteral("name"), exportText(value.name)},
        {QStringLiteral("teacher_ref"), exportKey(value.teacherKey)},
        {QStringLiteral("info"), classInfoToJson(value.info)},
        {QStringLiteral("roster"), rosterToJson(value.roster)},
        {QStringLiteral("speaking_evaluations"), evaluations}};
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

QJsonObject ClassTransferJsonCodec::toJson(const ClassTransferPortablePackage& package)
{
    QJsonArray teachers, classes;
    for (const auto& teacher : package.teachers) teachers.append(teacherToJson(teacher));
    for (const auto& classroom : package.classes) classes.append(classToJson(classroom));
    return {{QStringLiteral("format"), FormatName}, {QStringLiteral("version"), package.version.value()},
        {QStringLiteral("exported_at_utc"), QString::fromStdString(package.exportedAtUtc.value())},
        {QStringLiteral("teachers"), teachers}, {QStringLiteral("classes"), classes}};
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

Status ClassTransferJsonCodec::saveFile(
    const QString& filePath,
    const ClassTransferPortablePackage& package
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

    const qint64 fileSize = file.size();
    if (fileSize > 0
        && fileSize <= std::numeric_limits<qsizetype>::max())
    {
        uchar* const mappedData = file.map(0, fileSize);
        if (mappedData)
        {
            const QByteArray mappedBytes = QByteArray::fromRawData(
                reinterpret_cast<const char*>(mappedData),
                static_cast<qsizetype>(fileSize));
            const Status valid = hasProvenMappedJsonObjectSyntax(mappedBytes)
                ? Status{}
                : validateMappedJsonObject(mappedBytes);
            auto result = valid
                ? packageFromValidatedMappedJson(mappedBytes)
                : Result<ClassTransferPackage>(std::unexpected(valid.error()));
            file.unmap(mappedData);
            return result;
        }
    }

    // Preserve readAll for empty, unrepresentable, or unmappable files.
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const Status valid = jsonDocumentStatus(document, parseError);
    if (!valid) return std::unexpected(valid.error());
    return fromJson(document.object());
}
