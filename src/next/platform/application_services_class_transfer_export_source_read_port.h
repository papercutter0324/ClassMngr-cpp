#pragma once
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_transfer_repository.h"
#include "next/application/class_transfer_export_query.h"
#include <charconv>
#include <string>

namespace ClassMngr::Next::Platform
{
class ApplicationServicesClassTransferExportSourceReadPort final : public Application::ClassTransferExportSourceReadPort
{
public:
    explicit ApplicationServicesClassTransferExportSourceReadPort(ApplicationServices& services) : m_services(services) {}
    Application::ClassTransferExportSourceResult readSource(const Application::ClassTransferExportRequest& request) override
    {
        using namespace Application;
        DatabaseSession* session = m_services.databaseSession();
        if (!session || !session->classTransferRepository())
            return ClassTransferExportSourceResult::failure(error(QStringLiteral("No Teacher Profile service is available.")));
        QList<int> ids;
        for (const auto& id : request.classIds)
        {
            int value = 0;
            const auto& text = id.value();
            const auto [end, code] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (code != std::errc{} || end != text.data() + text.size() || std::to_string(value) != text)
                value = 0; // Repository defers this error behind earlier staged failures.
            ids.append(value);
        }
        auto read = session->classTransferRepository()->readExportSource(ids);
        if (!read)
            return ClassTransferExportSourceResult::failure(error(read.error()));
        ClassTransferExportSource output;
        output.exportedAtUtcMilliseconds = read->exportedAtUtc.toMSecsSinceEpoch();
        if (!read->selectionOrReadError.isEmpty())
            output.deferredSelectionOrLookupError = error(read->selectionOrReadError);
        if (!read->teacherBatchError.isEmpty())
            output.teacherBatchError = error(read->teacherBatchError);
        for (const int id : read->teacherIds)
        {
            ClassExportSourceTeacher teacher{*Domain::TeacherId::fromString(std::to_string(id)), {}};
            if (read->teacherErrorsById.contains(id))
                teacher.profile = ClassExportStage<ClassExportTeacherProfile>::error(error(read->teacherErrorsById.value(id)));
            else if (read->teachersById.contains(id))
            {
                const Teacher& source = read->teachersById[id];
                ClassExportTeacherProfile value;
                value.teacherKr = source.teacherKr.toStdU16String();
                value.teacherEn = source.teacherEn.toStdU16String();
                value.preferredRomanization = source.preferredRomanization.toStdU16String();
                value.preferredName = source.preferredName.toStdU16String();
                value.roomNumber = source.roomNumber.toStdU16String();
                value.birthday = source.birthday.toStdU16String();
                value.phoneNumber = source.phoneNumber.toStdU16String();
                value.wifiName = source.wifiName.toStdU16String();
                value.wifiPassword = source.wifiPassword.toStdU16String();
                value.internetType = source.internetType.toStdU16String();
                value.zoomId = source.zoomId.toStdU16String();
                value.zoomPassword = source.zoomPassword.toStdU16String();
                value.projectionType = source.projectionType.toStdU16String();
                value.notes = source.notes.toStdU16String();
                teacher.profile = ClassExportStage<ClassExportTeacherProfile>::value(std::move(value));
            }
            output.teachers.push_back(std::move(teacher));
        }
        for (const auto& source : read->classes)
        {
            ClassExportSourceClass item{*Domain::ClassId::fromString(std::to_string(source.classId)),
                static_cast<std::size_t>(source.selectedIndex), source.name.toStdU16String(), {}, {}, {}, {}};
            if (!source.infoError.isEmpty())
                item.info = ClassExportStage<ClassExportSourceInfo>::error(error(source.infoError));
            else
            {
                ClassExportSourceInfo value;
                value.sourceClassId = source.info.classId;
                value.sourceTeacherId = source.info.teacherId;
                value.classGrade = source.info.classGrade.toStdU16String();
                value.classLevel = source.info.classLevel.toStdU16String();
                value.readingBook = source.info.readingBook.toStdU16String();
                value.essayBook = source.info.essayBook.toStdU16String();
                value.classColor = source.info.classColor.toStdU16String();
                value.fontColor = source.info.fontColor.toStdU16String();
                value.notes = source.info.notes.toStdU16String();
                value.timeFillerActivities = source.info.timeFillerActivities.toStdU16String();
                value.teacherKr = source.info.teacherKr.toStdU16String();
                value.teacherEn = source.info.teacherEn.toStdU16String();
                value.teacherPreferredName = source.info.teacherPreferredName.toStdU16String();
                value.roomNumber = source.info.roomNumber.toStdU16String();
                value.wifiName = source.info.wifiName.toStdU16String();
                value.wifiPassword = source.info.wifiPassword.toStdU16String();
                value.internetType = source.info.internetType.toStdU16String();
                value.zoomId = source.info.zoomId.toStdU16String();
                value.zoomPassword = source.info.zoomPassword.toStdU16String();
                value.projectionType = source.info.projectionType.toStdU16String();
                for (const auto& time : source.info.classTimes)
                    value.classTimes.push_back({time.day.toStdU16String(), time.startTime.toStdU16String(), time.endTime.toStdU16String()});
                for (const auto& time : source.info.intensiveTimes)
                    value.intensiveTimes.push_back({time.day.toStdU16String(), time.startTime.toStdU16String(), time.endTime.toStdU16String()});
                item.info = ClassExportStage<ClassExportSourceInfo>::value(std::move(value));
            }
            if (!source.rosterError.isEmpty())
                item.roster = ClassExportStage<ClassExportRoster>::error(error(source.rosterError));
            else
            {
                ClassExportRoster value;
                for (const auto& column : source.roster.columns)
                    value.columns.push_back(column.toStdU16String());
                for (int width : source.roster.columnWidths)
                    value.columnWidths.push_back(width);
                value.rows = rows(source.roster.rows);
                item.roster = ClassExportStage<ClassExportRoster>::value(std::move(value));
            }
            if (source.evaluationAttempted)
            {
                item.teacher = ClassExportStage<std::optional<Domain::TeacherId>>::value(source.teacherId > 0
                    ? Domain::TeacherId::fromString(std::to_string(source.teacherId)) : std::nullopt);
                if (!source.evaluationError.isEmpty())
                    item.evaluations = ClassExportStage<std::vector<ClassExportEvaluation>>::error(error(source.evaluationError));
                else
                {
                    std::vector<ClassExportEvaluation> values;
                    for (const auto& evaluation : source.evaluations)
                        values.push_back({evaluation.name.toStdU16String(), rows(evaluation.rows)});
                    item.evaluations = ClassExportStage<std::vector<ClassExportEvaluation>>::value(std::move(values));
                }
            }
            output.classes.push_back(std::move(item));
        }
        return ClassTransferExportSourceResult::success(std::move(output));
    }
private:
    static Domain::OperationError error(const QString& message)
    {
        return {Domain::ErrorCode::Technical, message.toUtf8().toStdString(), true};
    }
    static Application::ClassExportRows rows(const QList<QStringList>& source)
    {
        Application::ClassExportRows result;
        for (const auto& row : source)
        {
            std::vector<Application::ClassExportText> values;
            for (const auto& cell : row)
                values.push_back(cell.toStdU16String());
            result.push_back(std::move(values));
        }
        return result;
    }
    ApplicationServices& m_services;
};
}
