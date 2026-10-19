#pragma once

#include "next/application/class_transfer_package.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace ClassMngr::Next::Application
{
struct ClassTransferExportRequest { std::vector<Domain::ClassId> classIds; };
struct ClassExportSourceInfo : ClassExportInfo
{
    int sourceClassId = -1;
    std::int64_t sourceTeacherId = -1;
    ClassExportText teacherKr, teacherEn, teacherPreferredName, roomNumber;
    ClassExportText wifiName, wifiPassword, internetType, zoomId, zoomPassword, projectionType;
};

enum class ClassExportStageState { NotAttempted, Value, Error };
template<class T> class ClassExportStage
{
public:
    static ClassExportStage value(T value) { return ClassExportStage(std::move(value)); }
    static ClassExportStage error(Domain::OperationError error) { return ClassExportStage(std::move(error)); }
    ClassExportStage() = default;
    ClassExportStageState state() const
    {
        return static_cast<ClassExportStageState>(m_data.index());
    }
    const T& value() const { return std::get<T>(m_data); }
    const Domain::OperationError& error() const { return std::get<Domain::OperationError>(m_data); }
private:
    explicit ClassExportStage(T value) : m_data(std::move(value)) {}
    explicit ClassExportStage(Domain::OperationError error) : m_data(std::move(error)) {}
    std::variant<std::monostate, T, Domain::OperationError> m_data;
};
struct ClassExportSourceClass
{
    Domain::ClassId id;
    std::size_t selectedIndex = 0;
    ClassExportText name;
    ClassExportStage<ClassExportSourceInfo> info;
    ClassExportStage<ClassExportRoster> roster;
    ClassExportStage<std::optional<Domain::TeacherId>> teacher;
    ClassExportStage<std::vector<ClassExportEvaluation>> evaluations;
};
struct ClassExportSourceTeacher
{
    Domain::TeacherId id;
    ClassExportStage<ClassExportTeacherProfile> profile;
};
struct ClassTransferExportSource
{
    std::int64_t exportedAtUtcMilliseconds = 0;
    std::vector<ClassExportSourceClass> classes;
    std::vector<ClassExportSourceTeacher> teachers;
    std::optional<Domain::OperationError> teacherBatchError;
    std::optional<Domain::OperationError> deferredSelectionOrLookupError;
};
using ClassTransferExportSourceResult = Domain::Result<ClassTransferExportSource>;
using ClassTransferExportResult = Domain::Result<ClassTransferPortablePackage>;
class ClassTransferExportSourceReadPort
{
public:
    virtual ~ClassTransferExportSourceReadPort() = default;
    virtual ClassTransferExportSourceResult readSource(const ClassTransferExportRequest&) = 0;
};

namespace ClassTransferExportDetail
{
inline std::u16string key(const char16_t* prefix, const std::size_t index)
{
    const std::string digits = std::to_string(index);
    return std::u16string(prefix) + std::u16string(digits.begin(), digits.end());
}
inline ClassExportClassKey classKey(const std::size_t index)
{
    return ClassExportClassKey{key(u"class-", index)};
}
inline ClassExportTeacherKey teacherKey(const std::size_t index)
{
    return ClassExportTeacherKey{key(u"teacher-", index)};
}
inline ClassTransferExportedAtUtc utcTimestamp(
    const std::int64_t milliseconds
    )
{
    using namespace std::chrono;
    const sys_time<std::chrono::milliseconds> point{std::chrono::milliseconds{milliseconds}};
    const auto day = floor<days>(point);
    const year_month_day date{day};
    const hh_mm_ss time{point - day};
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setfill('0') << std::setw(4) << int(date.year()) << '-'
        << std::setw(2) << unsigned(date.month()) << '-' << std::setw(2) << unsigned(date.day())
        << 'T' << std::setw(2) << time.hours().count() << ':' << std::setw(2) << time.minutes().count()
        << ':' << std::setw(2) << time.seconds().count() << '.' << std::setw(3) << time.subseconds().count() << 'Z';
    return ClassTransferExportedAtUtc(stream.str());
}
}

class ClassTransferExportQuery
{
public:
    explicit ClassTransferExportQuery(ClassTransferExportSourceReadPort& port) : m_port(port) {}
    ClassTransferExportResult execute(const ClassTransferExportRequest& request) const
    {
        // The source owns selection validation at its original read position.
        // Eager request rejection would mask an earlier per-class read failure.
        auto loaded = m_port.readSource(request);
        if (!loaded)
            return ClassTransferExportResult::failure(loaded.error());
        const auto& source = loaded.value();
        for (const auto& item : source.classes)
        {
            if (item.info.state() == ClassExportStageState::Error)
                return ClassTransferExportResult::failure(item.info.error());
            if (item.roster.state() == ClassExportStageState::Error)
                return ClassTransferExportResult::failure(item.roster.error());
            if (item.teacher.state() == ClassExportStageState::Error)
                return ClassTransferExportResult::failure(item.teacher.error());
            if (item.teacher.state() == ClassExportStageState::Value && item.teacher.value())
            {
                if (source.teacherBatchError)
                    return ClassTransferExportResult::failure(*source.teacherBatchError);
                for (const auto& teacher : source.teachers)
                    if (teacher.id == *item.teacher.value() && teacher.profile.state() == ClassExportStageState::Error)
                        return ClassTransferExportResult::failure(teacher.profile.error());
            }
            if (item.evaluations.state() == ClassExportStageState::Error)
                return ClassTransferExportResult::failure(item.evaluations.error());
        }
        if (source.deferredSelectionOrLookupError)
            return ClassTransferExportResult::failure(*source.deferredSelectionOrLookupError);

        // Successful source snapshots contain every attempted value. Projection
        // performs no additional validation or reads after the transaction.
        ClassTransferPortablePackage result;
        result.exportedAtUtc = ClassTransferExportDetail::utcTimestamp(source.exportedAtUtcMilliseconds);
        std::map<Domain::TeacherId, ClassExportTeacherKey> teacherKeys;
        for (const auto& item : source.classes)
        {
            ClassExportClass output;
            output.key = ClassTransferExportDetail::classKey(item.selectedIndex + 1);
            output.name = item.name;
            output.info = static_cast<const ClassExportInfo&>(item.info.value());
            output.roster = item.roster.value();
            output.evaluations = item.evaluations.value();
            if (item.teacher.value())
            {
                const auto& id = *item.teacher.value();
                if (!teacherKeys.contains(id))
                    for (const auto& teacher : source.teachers)
                        if (teacher.id == id)
                        {
                            const auto key = ClassTransferExportDetail::teacherKey(result.teachers.size() + 1);
                            teacherKeys.emplace(id, key);
                            result.teachers.push_back({key, teacher.profile.value()});
                            break;
                        }
                output.teacherKey = teacherKeys.at(id);
            }
            result.classes.push_back(std::move(output));
        }
        return ClassTransferExportResult::success(std::move(result));
    }
private:
    ClassTransferExportSourceReadPort& m_port;
};
}
