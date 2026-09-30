#pragma once

#include "next/application/native_english_teacher_directory_save_policy.h"
#include "next/application/native_english_teacher_directory_save_port.h"

namespace ClassMngr::Next::Application
{

class NativeEnglishTeacherDirectorySaveUseCase final
{
public:
    [[nodiscard]] static NativeEnglishTeacherDirectorySaveOutcome execute(
        const NativeEnglishTeacherDirectorySaveRequest& request,
        const NativeEnglishTeacherDirectorySavePort& port
        )
    {
        const NativeEnglishTeacherDirectorySaveValidation validation =
            validateNativeEnglishTeacherDirectorySave(request);
        if (!validation.isValid())
        {
            return NativeEnglishTeacherDirectorySaveOutcome::invalid(validation);
        }

        const Domain::Result<void> saved =
            port.saveNativeEnglishTeacherDirectory(request);
        if (!saved)
        {
            return NativeEnglishTeacherDirectorySaveOutcome::failure(
                saved.error());
        }

        return NativeEnglishTeacherDirectorySaveOutcome::success();
    }
};

} // namespace ClassMngr::Next::Application
