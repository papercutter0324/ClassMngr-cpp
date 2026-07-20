#include "teacher_import_repository.h"

#include "data/database/database_transaction.h"
#include "features/teacher/import/teacher_import_name_utils.h"

#include <QObject>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>

namespace
{
QString normalizedName(const QString& value)
{
    return value.simplified().toCaseFolded();
}

QString koreanTeacherNameKey(const QString& value)
{
    return TeacherImportNameUtils::hangulOnly(value);
}

QString queryFailure(const QSqlQuery& query, const QString& action)
{
    return QObject::tr("%1 failed: %2").arg(action, query.lastError().text());
}

Status validatePlan(const TeacherImportPlan& plan)
{
    if (!plan.sourceDate.isValid())
    {
        return std::unexpected(QObject::tr("The teacher import date is invalid."));
    }

    QSet<QString> korean;
    for (const Teacher& teacher : plan.koreanTeachers)
    {
        const QString key = koreanTeacherNameKey(teacher.teacherKr);
        if (key.isEmpty())
        {
            return std::unexpected(QObject::tr("Every imported Korean teacher must have a name."));
        }
        if (korean.contains(key))
        {
            return std::unexpected(QObject::tr("The import contains a duplicate Korean teacher name."));
        }
        korean.insert(key);
    }

    QSet<QString> native;
    for (const NativeEnglishTeacher& teacher : plan.nativeEnglishTeachers)
    {
        const QString key = normalizedName(teacher.name);
        if (key.isEmpty())
        {
            return std::unexpected(QObject::tr("Every imported Native English Teacher must have a name."));
        }
        if (native.contains(key))
        {
            return std::unexpected(QObject::tr("The import contains a duplicate Native English Teacher name."));
        }
        native.insert(key);
    }

    QSet<QString> gsEnglish;
    QSet<QString> gsKorean;
    for (const GsTeamMember& member : plan.gsTeamMembers)
    {
        const QString english = normalizedName(member.name);
        const QString koreanName = normalizedName(member.koreanName);
        if (english.isEmpty() && koreanName.isEmpty())
        {
            return std::unexpected(QObject::tr("Every imported GS Team member must have a name."));
        }
        if ((!english.isEmpty() && gsEnglish.contains(english))
            || (!koreanName.isEmpty() && gsKorean.contains(koreanName)))
        {
            return std::unexpected(QObject::tr("The import contains a duplicate GS Team name."));
        }
        if (!english.isEmpty()) gsEnglish.insert(english);
        if (!koreanName.isEmpty()) gsKorean.insert(koreanName);
    }

    return {};
}

Result<QList<Teacher>> loadKoreanTeachers(QSqlDatabase& database)
{
    QList<Teacher> result;
    QSqlQuery query(database);
    if (!query.exec(R"(
        SELECT id, teacher_kr, teacher_en, preferred_romanization,
               room_number, birthday, phone_number, wifi_name, wifi_password,
               internet_type, zoom_id, zoom_password, projection_type, notes
        FROM teachers
    )"))
    {
        return std::unexpected(queryFailure(query, QObject::tr("Loading Korean teachers")));
    }

    while (query.next())
    {
        Teacher teacher;
        teacher.id = query.value(0).toInt();
        teacher.teacherKr = query.value(1).toString();
        teacher.teacherEn = query.value(2).toString();
        teacher.preferredRomanization = query.value(3).toString();
        teacher.roomNumber = query.value(4).toString();
        teacher.birthday = query.value(5).toString();
        teacher.phoneNumber = query.value(6).toString();
        teacher.wifiName = query.value(7).toString();
        teacher.wifiPassword = query.value(8).toString();
        teacher.internetType = query.value(9).toString();
        teacher.zoomId = query.value(10).toString();
        teacher.zoomPassword = query.value(11).toString();
        teacher.projectionType = query.value(12).toString();
        teacher.notes = query.value(13).toString();
        result.append(teacher);
    }
    return result;
}

Result<QList<NativeEnglishTeacher>> loadNativeTeachers(QSqlDatabase& database)
{
    QList<NativeEnglishTeacher> result;
    QSqlQuery query(database);
    if (!query.exec(R"(
        SELECT id, name, position, phone_number, birthday, nationality, email
        FROM native_english_teachers
    )"))
    {
        return std::unexpected(queryFailure(query, QObject::tr("Loading Native English Teachers")));
    }
    while (query.next())
    {
        result.append({
            query.value(0).toInt(), query.value(1).toString(),
            query.value(2).toString(), query.value(3).toString(),
            query.value(4).toString(), query.value(5).toString(),
            query.value(6).toString()
        });
    }
    return result;
}

Result<QList<GsTeamMember>> loadGsTeam(QSqlDatabase& database)
{
    QList<GsTeamMember> result;
    QSqlQuery query(database);
    if (!query.exec(R"(
        SELECT id, name, korean_name, position, phone_number, birthday
        FROM gs_team
    )"))
    {
        return std::unexpected(queryFailure(query, QObject::tr("Loading GS Team members")));
    }
    while (query.next())
    {
        result.append({
            query.value(0).toInt(), query.value(1).toString(),
            query.value(2).toString(), query.value(3).toString(),
            query.value(4).toString(), query.value(5).toString()
        });
    }
    return result;
}

template<typename T, typename Name>
QList<int> matchingIndexes(const QList<T>& values, const QString& key, Name name)
{
    QList<int> result;
    for (int index = 0; index < values.size(); ++index)
    {
        if (normalizedName(name(values.at(index))) == key)
        {
            result.append(index);
        }
    }
    return result;
}

QList<int> matchingKoreanTeacherIndexes(
    const QList<Teacher>& teachers,
    const QString& key
    )
{
    QList<int> result;
    for (int index = 0; index < teachers.size(); ++index)
    {
        if (koreanTeacherNameKey(teachers.at(index).teacherKr) == key)
        {
            result.append(index);
        }
    }
    return result;
}

Status updateLatestDate(QSqlDatabase& database, const QDate& sourceDate)
{
    QDate current;
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT value FROM app_settings WHERE key=?"));
    query.addBindValue(QString::fromLatin1(TeacherImportRepository::LatestSourceDateSetting));
    if (!query.exec())
    {
        return std::unexpected(queryFailure(query, QObject::tr("Loading the previous teacher import date")));
    }
    if (query.next())
    {
        current = QDate::fromString(query.value(0).toString(), Qt::ISODate);
    }

    if (current.isValid() && sourceDate <= current)
    {
        return {};
    }

    query.prepare(R"(
        INSERT INTO app_settings (key, value) VALUES (?, ?)
        ON CONFLICT(key) DO UPDATE SET value=excluded.value
    )");
    query.addBindValue(QString::fromLatin1(TeacherImportRepository::LatestSourceDateSetting));
    query.addBindValue(sourceDate.toString(Qt::ISODate));
    if (!query.exec())
    {
        return std::unexpected(queryFailure(query, QObject::tr("Saving the teacher import date")));
    }
    return {};
}
}

TeacherImportRepository::TeacherImportRepository(QSqlDatabase& database)
    : m_database(database)
{
}

Result<TeacherImportSummary> TeacherImportRepository::importTeachers(
    const TeacherImportPlan& plan
    )
{
    const Status valid = validatePlan(plan);
    if (!valid)
    {
        return std::unexpected(valid.error());
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(QObject::tr("Unable to start the teacher import transaction."));
    }

    auto koreanResult = loadKoreanTeachers(m_database);
    auto nativeResult = loadNativeTeachers(m_database);
    auto gsResult = loadGsTeam(m_database);
    if (!koreanResult) return std::unexpected(koreanResult.error());
    if (!nativeResult) return std::unexpected(nativeResult.error());
    if (!gsResult) return std::unexpected(gsResult.error());

    QList<Teacher> korean = *koreanResult;
    QList<NativeEnglishTeacher> native = *nativeResult;
    QList<GsTeamMember> gs = *gsResult;
    TeacherImportSummary summary;

    for (const Teacher& source : plan.koreanTeachers)
    {
        const QString key = koreanTeacherNameKey(source.teacherKr);
        const QList<int> matches = matchingKoreanTeacherIndexes(korean, key);
        if (matches.size() > 1)
        {
            return std::unexpected(QObject::tr("More than one stored Korean teacher matches %1.").arg(source.teacherKr));
        }

        if (matches.isEmpty())
        {
            QSqlQuery query(m_database);
            query.prepare(R"(
                INSERT INTO teachers
                    (teacher_kr, teacher_en, preferred_romanization,
                     room_number, birthday, phone_number,
                     wifi_name, wifi_password, internet_type,
                     zoom_id, zoom_password, projection_type, notes)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            )");
            query.addBindValue(key);
            query.addBindValue(source.teacherEn.trimmed());
            query.addBindValue(source.preferredRomanization.trimmed());
            query.addBindValue(source.roomNumber.trimmed());
            query.addBindValue(source.birthday.trimmed());
            query.addBindValue(source.phoneNumber.trimmed());
            query.addBindValue(source.wifiName.trimmed());
            query.addBindValue(source.wifiPassword.trimmed());
            query.addBindValue(source.internetType.trimmed());
            query.addBindValue(source.zoomId.trimmed());
            query.addBindValue(source.zoomPassword.trimmed());
            query.addBindValue(source.projectionType.trimmed());
            query.addBindValue(source.notes.trimmed());
            if (!query.exec())
            {
                return std::unexpected(queryFailure(query, QObject::tr("Creating a Korean teacher")));
            }
            ++summary.koreanTeachers.created;
            continue;
        }

        const Teacher& existing = korean.at(matches.first());
        const QString name = key;
        const QString room = source.roomNumber.trimmed().isEmpty()
            ? existing.roomNumber : source.roomNumber.trimmed();
        const QString birthday = source.birthday.trimmed().isEmpty()
            ? existing.birthday : source.birthday.trimmed();
        const QString phone = source.phoneNumber.trimmed().isEmpty()
            ? existing.phoneNumber : source.phoneNumber.trimmed();
        if (name == existing.teacherKr && room == existing.roomNumber
            && birthday == existing.birthday && phone == existing.phoneNumber)
        {
            ++summary.koreanTeachers.unchanged;
            continue;
        }

        QSqlQuery query(m_database);
        query.prepare(R"(
            UPDATE teachers
            SET teacher_kr=?, room_number=?, birthday=?, phone_number=?
            WHERE id=?
        )");
        query.addBindValue(name);
        query.addBindValue(room);
        query.addBindValue(birthday);
        query.addBindValue(phone);
        query.addBindValue(existing.id);
        if (!query.exec())
        {
            return std::unexpected(queryFailure(query, QObject::tr("Updating a Korean teacher")));
        }
        ++summary.koreanTeachers.updated;
    }

    for (const NativeEnglishTeacher& source : plan.nativeEnglishTeachers)
    {
        const QString key = normalizedName(source.name);
        const QList<int> matches = matchingIndexes(
            native, key, [](const NativeEnglishTeacher& value) { return value.name; });
        if (matches.size() > 1)
        {
            return std::unexpected(QObject::tr("More than one stored Native English Teacher matches %1.").arg(source.name));
        }

        if (matches.isEmpty())
        {
            QSqlQuery query(m_database);
            query.prepare(R"(
                INSERT INTO native_english_teachers
                    (name, position, phone_number, birthday, nationality, email)
                VALUES (?, ?, ?, ?, ?, ?)
            )");
            query.addBindValue(source.name.simplified());
            query.addBindValue(source.position.trimmed());
            query.addBindValue(source.phoneNumber.trimmed());
            query.addBindValue(source.birthday.trimmed());
            query.addBindValue(source.nationality.trimmed());
            query.addBindValue(source.email.trimmed());
            if (!query.exec())
            {
                return std::unexpected(queryFailure(query, QObject::tr("Creating a Native English Teacher")));
            }
            ++summary.nativeEnglishTeachers.created;
            continue;
        }

        const NativeEnglishTeacher& existing = native.at(matches.first());
        NativeEnglishTeacher updated = existing;
        updated.name = source.name.simplified();
        if (!source.position.trimmed().isEmpty()) updated.position = source.position.trimmed();
        if (!source.phoneNumber.trimmed().isEmpty()) updated.phoneNumber = source.phoneNumber.trimmed();
        if (!source.birthday.trimmed().isEmpty()) updated.birthday = source.birthday.trimmed();
        if (!source.nationality.trimmed().isEmpty()) updated.nationality = source.nationality.trimmed();
        if (!source.email.trimmed().isEmpty()) updated.email = source.email.trimmed();
        if (updated.name == existing.name && updated.position == existing.position
            && updated.phoneNumber == existing.phoneNumber && updated.birthday == existing.birthday
            && updated.nationality == existing.nationality && updated.email == existing.email)
        {
            ++summary.nativeEnglishTeachers.unchanged;
            continue;
        }

        QSqlQuery query(m_database);
        query.prepare(R"(
            UPDATE native_english_teachers
            SET name=?, position=?, phone_number=?, birthday=?, nationality=?, email=?
            WHERE id=?
        )");
        query.addBindValue(updated.name);
        query.addBindValue(updated.position);
        query.addBindValue(updated.phoneNumber);
        query.addBindValue(updated.birthday);
        query.addBindValue(updated.nationality);
        query.addBindValue(updated.email);
        query.addBindValue(existing.id);
        if (!query.exec())
        {
            return std::unexpected(queryFailure(query, QObject::tr("Updating a Native English Teacher")));
        }
        ++summary.nativeEnglishTeachers.updated;
    }

    for (const GsTeamMember& source : plan.gsTeamMembers)
    {
        const bool useKorean = !source.koreanName.trimmed().isEmpty();
        const QString key = normalizedName(useKorean ? source.koreanName : source.name);
        const QList<int> matches = useKorean
            ? matchingIndexes(gs, key, [](const GsTeamMember& value) { return value.koreanName; })
            : matchingIndexes(gs, key, [](const GsTeamMember& value) { return value.name; });
        if (matches.size() > 1)
        {
            return std::unexpected(QObject::tr("More than one stored GS Team member matches %1.")
                .arg(useKorean ? source.koreanName : source.name));
        }

        if (matches.isEmpty())
        {
            QSqlQuery query(m_database);
            query.prepare(R"(
                INSERT INTO gs_team
                    (name, korean_name, position, phone_number, birthday)
                VALUES (?, ?, ?, ?, ?)
            )");
            query.addBindValue(source.name.simplified());
            query.addBindValue(source.koreanName.simplified());
            query.addBindValue(source.position.trimmed());
            query.addBindValue(source.phoneNumber.trimmed());
            query.addBindValue(source.birthday.trimmed());
            if (!query.exec())
            {
                return std::unexpected(queryFailure(query, QObject::tr("Creating a GS Team member")));
            }
            ++summary.gsTeamMembers.created;
            continue;
        }

        const GsTeamMember& existing = gs.at(matches.first());
        GsTeamMember updated = existing;
        if (!source.name.trimmed().isEmpty()) updated.name = source.name.simplified();
        if (!source.koreanName.trimmed().isEmpty()) updated.koreanName = source.koreanName.simplified();
        if (!source.position.trimmed().isEmpty()) updated.position = source.position.trimmed();
        if (!source.phoneNumber.trimmed().isEmpty()) updated.phoneNumber = source.phoneNumber.trimmed();
        if (!source.birthday.trimmed().isEmpty()) updated.birthday = source.birthday.trimmed();
        if (updated.name == existing.name && updated.koreanName == existing.koreanName
            && updated.position == existing.position && updated.phoneNumber == existing.phoneNumber
            && updated.birthday == existing.birthday)
        {
            ++summary.gsTeamMembers.unchanged;
            continue;
        }

        QSqlQuery query(m_database);
        query.prepare(R"(
            UPDATE gs_team
            SET name=?, korean_name=?, position=?, phone_number=?, birthday=?
            WHERE id=?
        )");
        query.addBindValue(updated.name);
        query.addBindValue(updated.koreanName);
        query.addBindValue(updated.position);
        query.addBindValue(updated.phoneNumber);
        query.addBindValue(updated.birthday);
        query.addBindValue(existing.id);
        if (!query.exec())
        {
            return std::unexpected(queryFailure(query, QObject::tr("Updating a GS Team member")));
        }
        ++summary.gsTeamMembers.updated;
    }

    const Status dateStatus = updateLatestDate(m_database, plan.sourceDate);
    if (!dateStatus)
    {
        return std::unexpected(dateStatus.error());
    }

    if (!transaction.commit())
    {
        return std::unexpected(QObject::tr("Unable to commit the teacher import transaction."));
    }

    return summary;
}
