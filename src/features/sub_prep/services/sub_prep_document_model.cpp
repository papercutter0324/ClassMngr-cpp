#include "sub_prep_document_model.h"

#include "classmngr/engine/sub_prep_document.h"
#include "sub_prep_print_service.h"

#include <QByteArray>

#include <string>
#include <string_view>
#include <utility>

namespace
{
using PortableClassInfo = classmngr::engine::ClassInfo;
using PortableClassTime = classmngr::engine::ClassTime;
using PortableDocument = classmngr::engine::SubPrepDocument;
using PortableDocumentRequest = classmngr::engine::SubPrepDocumentRequest;
using PortableScheduleCell = classmngr::engine::ScheduleReportCell;
using PortableScheduleEntry = classmngr::engine::ScheduleReportEntry;
using PortableScheduleEntryKind = classmngr::engine::ScheduleReportEntryKind;
using PortableScheduleModel = classmngr::engine::ScheduleReportModel;
using PortableScheduleRow = classmngr::engine::ScheduleReportRowView;
using PortableTeacher = classmngr::engine::Teacher;
using PortableTeacherGroup = classmngr::engine::SubPrepTeacherGroup;

std::string toUtf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return {
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
    };
}

QString fromUtf8(std::string_view value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

PortableClassTime toPortable(const ClassTime& source)
{
    return {
        toUtf8(source.day),
        toUtf8(source.startTime),
        toUtf8(source.endTime)
    };
}

ClassTime fromPortable(const PortableClassTime& source)
{
    return {
        fromUtf8(source.day),
        fromUtf8(source.startTime),
        fromUtf8(source.endTime)
    };
}

PortableTeacher toPortable(const Teacher& source)
{
    PortableTeacher result;
    result.id = source.id;
    result.teacherKr = toUtf8(source.teacherKr);
    result.teacherEn = toUtf8(source.teacherEn);
    result.preferredRomanization = toUtf8(source.preferredRomanization);
    result.preferredName = toUtf8(source.preferredName);
    result.roomNumber = toUtf8(source.roomNumber);
    result.birthday = toUtf8(source.birthday);
    result.phoneNumber = toUtf8(source.phoneNumber);
    result.wifiName = toUtf8(source.wifiName);
    result.wifiPassword = toUtf8(source.wifiPassword);
    result.internetType = toUtf8(source.internetType);
    result.zoomId = toUtf8(source.zoomId);
    result.zoomPassword = toUtf8(source.zoomPassword);
    result.projectionType = toUtf8(source.projectionType);
    result.notes = toUtf8(source.notes);
    return result;
}

Teacher fromPortable(const PortableTeacher& source)
{
    Teacher result;
    result.id = source.id;
    result.teacherKr = fromUtf8(source.teacherKr);
    result.teacherEn = fromUtf8(source.teacherEn);
    result.preferredRomanization = fromUtf8(source.preferredRomanization);
    result.preferredName = fromUtf8(source.preferredName);
    result.roomNumber = fromUtf8(source.roomNumber);
    result.birthday = fromUtf8(source.birthday);
    result.phoneNumber = fromUtf8(source.phoneNumber);
    result.wifiName = fromUtf8(source.wifiName);
    result.wifiPassword = fromUtf8(source.wifiPassword);
    result.internetType = fromUtf8(source.internetType);
    result.zoomId = fromUtf8(source.zoomId);
    result.zoomPassword = fromUtf8(source.zoomPassword);
    result.projectionType = fromUtf8(source.projectionType);
    result.notes = fromUtf8(source.notes);
    return result;
}

PortableClassInfo toPortable(const ClassInfo& source)
{
    PortableClassInfo result;
    result.classId = source.classId;
    result.teacherId = source.teacherId;
    result.teacherKr = toUtf8(source.teacherKr);
    result.teacherEn = toUtf8(source.teacherEn);
    result.teacherPreferredName = toUtf8(source.teacherPreferredName);
    result.roomNumber = toUtf8(source.roomNumber);
    result.wifiName = toUtf8(source.wifiName);
    result.wifiPassword = toUtf8(source.wifiPassword);
    result.internetType = toUtf8(source.internetType);
    result.zoomId = toUtf8(source.zoomId);
    result.zoomPassword = toUtf8(source.zoomPassword);
    result.projectionType = toUtf8(source.projectionType);
    result.classGrade = toUtf8(source.classGrade);
    result.classLevel = toUtf8(source.classLevel);
    result.readingBook = toUtf8(source.readingBook);
    result.essayBook = toUtf8(source.essayBook);
    result.classColor = toUtf8(source.classColor);
    result.fontColor = toUtf8(source.fontColor);
    result.notes = toUtf8(source.notes);
    result.timeFillerActivities = toUtf8(source.timeFillerActivities);

    result.classTimes.reserve(static_cast<std::size_t>(source.classTimes.size()));
    for (const ClassTime& time : source.classTimes)
    {
        result.classTimes.push_back(toPortable(time));
    }
    result.intensiveTimes.reserve(
        static_cast<std::size_t>(source.intensiveTimes.size())
        );
    for (const ClassTime& time : source.intensiveTimes)
    {
        result.intensiveTimes.push_back(toPortable(time));
    }
    return result;
}

ClassInfo fromPortable(const PortableClassInfo& source)
{
    ClassInfo result;
    result.classId = source.classId;
    result.teacherId = source.teacherId;
    result.teacherKr = fromUtf8(source.teacherKr);
    result.teacherEn = fromUtf8(source.teacherEn);
    result.teacherPreferredName = fromUtf8(source.teacherPreferredName);
    result.roomNumber = fromUtf8(source.roomNumber);
    result.wifiName = fromUtf8(source.wifiName);
    result.wifiPassword = fromUtf8(source.wifiPassword);
    result.internetType = fromUtf8(source.internetType);
    result.zoomId = fromUtf8(source.zoomId);
    result.zoomPassword = fromUtf8(source.zoomPassword);
    result.projectionType = fromUtf8(source.projectionType);
    result.classGrade = fromUtf8(source.classGrade);
    result.classLevel = fromUtf8(source.classLevel);
    result.readingBook = fromUtf8(source.readingBook);
    result.essayBook = fromUtf8(source.essayBook);
    result.classColor = fromUtf8(source.classColor);
    result.fontColor = fromUtf8(source.fontColor);
    result.notes = fromUtf8(source.notes);
    result.timeFillerActivities = fromUtf8(source.timeFillerActivities);

    result.classTimes.reserve(static_cast<qsizetype>(source.classTimes.size()));
    for (const PortableClassTime& time : source.classTimes)
    {
        result.classTimes.append(fromPortable(time));
    }
    result.intensiveTimes.reserve(
        static_cast<qsizetype>(source.intensiveTimes.size())
        );
    for (const PortableClassTime& time : source.intensiveTimes)
    {
        result.intensiveTimes.append(fromPortable(time));
    }
    return result;
}

PortableScheduleEntry toPortable(const ScheduleEntry& source)
{
    PortableScheduleEntry result;
    result.classId = source.classId;
    result.kind = source.kind == ScheduleEntryKind::TestingClass
        ? PortableScheduleEntryKind::TestingClass
        : PortableScheduleEntryKind::RegularClass;
    result.className = toUtf8(source.className);
    result.teacherKr = toUtf8(source.teacherKr);
    result.teacherEn = toUtf8(source.teacherEn);
    result.teacherPreferredName = toUtf8(source.teacherPreferredName);
    result.roomNumber = toUtf8(source.roomNumber);
    result.classGrade = toUtf8(source.classGrade);
    result.classLevel = toUtf8(source.classLevel);
    result.classColor = toUtf8(source.classColor);
    result.fontColor = toUtf8(source.fontColor);
    return result;
}

ScheduleEntry fromPortable(const PortableScheduleEntry& source)
{
    ScheduleEntry result;
    result.classId = source.classId;
    result.kind = source.kind == PortableScheduleEntryKind::TestingClass
        ? ScheduleEntryKind::TestingClass
        : ScheduleEntryKind::RegularClass;
    result.className = fromUtf8(source.className);
    result.teacherKr = fromUtf8(source.teacherKr);
    result.teacherEn = fromUtf8(source.teacherEn);
    result.teacherPreferredName = fromUtf8(source.teacherPreferredName);
    result.roomNumber = fromUtf8(source.roomNumber);
    result.classGrade = fromUtf8(source.classGrade);
    result.classLevel = fromUtf8(source.classLevel);
    result.classColor = fromUtf8(source.classColor);
    result.fontColor = fromUtf8(source.fontColor);
    return result;
}

PortableScheduleCell toPortable(const ScheduleCellView& source)
{
    PortableScheduleCell result;
    result.day = toUtf8(source.day);
    result.timeLabel = toUtf8(source.timeLabel);
    result.defaultSlotState = toUtf8(source.defaultSlotState);
    result.slotState = toUtf8(source.slotState);
    result.testingRoom = toUtf8(source.testingRoom);
    result.testingClassAssignment = source.testingClassAssignment;
    result.testingClassId = source.testingClassId;
    result.slotTogglingEnabled = source.slotTogglingEnabled;
    result.testingBlockCreationEnabled = source.testingBlockCreationEnabled;
    result.entries.reserve(static_cast<std::size_t>(source.entries.size()));
    for (const ScheduleEntry& entry : source.entries)
    {
        result.entries.push_back(toPortable(entry));
    }
    return result;
}

ScheduleCellView fromPortable(const PortableScheduleCell& source)
{
    ScheduleCellView result;
    result.day = fromUtf8(source.day);
    result.timeLabel = fromUtf8(source.timeLabel);
    result.defaultSlotState = fromUtf8(source.defaultSlotState);
    result.slotState = fromUtf8(source.slotState);
    result.testingRoom = fromUtf8(source.testingRoom);
    result.testingClassAssignment = source.testingClassAssignment;
    result.testingClassId = source.testingClassId;
    result.slotTogglingEnabled = source.slotTogglingEnabled;
    result.testingBlockCreationEnabled = source.testingBlockCreationEnabled;
    result.entries.reserve(static_cast<qsizetype>(source.entries.size()));
    for (const PortableScheduleEntry& entry : source.entries)
    {
        result.entries.append(fromPortable(entry));
    }
    return result;
}

PortableScheduleModel toPortable(const ScheduleViewModel& source)
{
    PortableScheduleModel result;
    result.days.reserve(static_cast<std::size_t>(source.days.size()));
    for (const QString& day : source.days)
    {
        result.days.push_back(toUtf8(day));
    }

    result.rows.reserve(static_cast<std::size_t>(source.rows.size()));
    for (const ScheduleRowView& sourceRow : source.rows)
    {
        PortableScheduleRow row;
        row.timeLabel = toUtf8(sourceRow.timeLabel);
        row.timeRangeLabel = toUtf8(sourceRow.timeRangeLabel);
        row.maxEntryCount = sourceRow.maxEntryCount;
        row.cells.reserve(static_cast<std::size_t>(sourceRow.cells.size()));
        for (const ScheduleCellView& cell : sourceRow.cells)
        {
            row.cells.push_back(toPortable(cell));
        }
        result.rows.push_back(std::move(row));
    }

    result.uses55Endings = source.uses55Endings;
    result.summary.essayBlocks = source.summary.essayBlocks;
    result.summary.testingBlocks = source.summary.testingBlocks;
    result.summary.testingClassBlocks = source.summary.testingClassBlocks;
    result.summary.scheduledBlocks = source.summary.scheduledBlocks;
    return result;
}

ScheduleViewModel fromPortable(const PortableScheduleModel& source)
{
    ScheduleViewModel result;
    result.days.reserve(static_cast<qsizetype>(source.days.size()));
    for (const std::string& day : source.days)
    {
        result.days.append(fromUtf8(day));
    }

    result.rows.reserve(static_cast<qsizetype>(source.rows.size()));
    for (const PortableScheduleRow& sourceRow : source.rows)
    {
        ScheduleRowView row;
        row.timeLabel = fromUtf8(sourceRow.timeLabel);
        row.timeRangeLabel = fromUtf8(sourceRow.timeRangeLabel);
        row.maxEntryCount = sourceRow.maxEntryCount;
        row.cells.reserve(static_cast<qsizetype>(sourceRow.cells.size()));
        for (const PortableScheduleCell& cell : sourceRow.cells)
        {
            row.cells.append(fromPortable(cell));
        }
        result.rows.append(std::move(row));
    }

    result.uses55Endings = source.uses55Endings;
    result.summary.essayBlocks = source.summary.essayBlocks;
    result.summary.testingBlocks = source.summary.testingBlocks;
    result.summary.testingClassBlocks = source.summary.testingClassBlocks;
    result.summary.scheduledBlocks = source.summary.scheduledBlocks;
    return result;
}

PortableTeacherGroup toPortable(
    const SubPrepClassInformation::TeacherGroup& source
    )
{
    PortableTeacherGroup result;
    result.teacher = toPortable(source.teacher);
    result.displayName = toUtf8(source.displayName);
    result.classListText = toUtf8(source.classListText);
    result.classes.reserve(static_cast<std::size_t>(source.classes.size()));
    for (const SubPrepClassInformation::ClassDetails& sourceDetails :
         source.classes)
    {
        classmngr::engine::SubPrepClassDetails details;
        details.classId = sourceDetails.classId;
        details.info = toPortable(sourceDetails.info);
        details.studentCount = sourceDetails.studentCount;
        details.classLabel = toUtf8(sourceDetails.classLabel);
        details.timeText = toUtf8(sourceDetails.timeText);
        result.classes.push_back(std::move(details));
    }
    return result;
}

SubPrepClassInformation::TeacherGroup fromPortable(
    const PortableTeacherGroup& source
    )
{
    SubPrepClassInformation::TeacherGroup result;
    result.teacher = fromPortable(source.teacher);
    result.displayName = fromUtf8(source.displayName);
    result.classListText = fromUtf8(source.classListText);
    result.classes.reserve(static_cast<qsizetype>(source.classes.size()));
    for (const classmngr::engine::SubPrepClassDetails& sourceDetails :
         source.classes)
    {
        SubPrepClassInformation::ClassDetails details;
        details.classId = sourceDetails.classId;
        details.info = fromPortable(sourceDetails.info);
        details.studentCount = sourceDetails.studentCount;
        details.classLabel = fromUtf8(sourceDetails.classLabel);
        details.timeText = fromUtf8(sourceDetails.timeText);
        result.classes.append(std::move(details));
    }
    return result;
}

PortableDocumentRequest toPortable(const SubPrepPrintService::Request& source)
{
    PortableDocumentRequest result;
    result.campus.officeNumber = toUtf8(source.campus.officeNumber);
    result.campus.officeWifi = toUtf8(source.campus.officeWifi);
    result.campus.officeWifiPassword = toUtf8(source.campus.officeWifiPassword);
    result.campus.photocopierCode = toUtf8(source.campus.photocopierCode);
    result.zoom.loginId = toUtf8(source.zoom.loginId);
    result.zoom.password = toUtf8(source.zoom.password);
    result.classMaterials = toUtf8(source.classMaterials);
    result.gradingInstructions = toUtf8(source.gradingInstructions);
    result.specialInstructions = toUtf8(source.specialInstructions);
    result.schedule = toPortable(source.schedule);
    result.classInformation.reserve(
        static_cast<std::size_t>(source.classInformation.size())
        );
    for (const SubPrepClassInformation::TeacherGroup& group :
         source.classInformation)
    {
        result.classInformation.push_back(toPortable(group));
    }
    result.subNotes = toUtf8(source.subNotes);
    return result;
}

SubPrepDocumentModel::Document fromPortable(const PortableDocument& source)
{
    SubPrepDocumentModel::Document result;
    result.campus.officeNumber = fromUtf8(source.campus.officeNumber);
    result.campus.officeWifi = fromUtf8(source.campus.officeWifi);
    result.campus.officeWifiPassword = fromUtf8(
        source.campus.officeWifiPassword
        );
    result.campus.photocopierCode = fromUtf8(source.campus.photocopierCode);
    result.zoom.loginId = fromUtf8(source.zoom.loginId);
    result.zoom.password = fromUtf8(source.zoom.password);
    result.classMaterials = fromUtf8(source.classMaterials);
    result.gradingInstructions = fromUtf8(source.gradingInstructions);
    result.specialInstructions = fromUtf8(source.specialInstructions);
    result.schedule = fromPortable(source.schedule);
    result.classInformation.reserve(
        static_cast<qsizetype>(source.classInformation.size())
        );
    for (const PortableTeacherGroup& group : source.classInformation)
    {
        result.classInformation.append(fromPortable(group));
    }
    result.subNotes = fromUtf8(source.subNotes);
    return result;
}
} // namespace

namespace SubPrepDocumentModel
{
Document build(
    const SubPrepPrintService::Request& request
    )
{
    const PortableDocumentRequest portableRequest = toPortable(request);
    return fromPortable(
        classmngr::engine::SubPrepDocumentService::build(portableRequest)
        );
}
}
