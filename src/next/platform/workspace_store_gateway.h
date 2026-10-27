#pragma once

#include "next/application/personal_details_save.h"
#include "next/application/initial_setup_teacher_choices_read_snapshot.h"
#include "next/application/custom_color_palette_preferences.h"
#include "next/application/class_create.h"
#include "next/application/initial_setup_class_times_requirement.h"
#include "next/application/teacher_create.h"
#include "next/application/teacher_import_use_case.h"
#include "next/application/teacher_import_latest_source_date_read_query.h"
#include "next/application/teacher_profile_edit_use_case.h"
#include "next/application/teacher_profile_read_port.h"
#include "next/application/teacher_create.h"
#include "next/application/teacher_delete.h"
#include "next/application/teacher_directory_read_port.h"
#include "next/application/korean_teacher_birthday_directory_read_port.h"
#include "next/application/native_english_teacher_birthday_directory_read_port.h"
#include "next/application/gs_team_birthday_directory_read_port.h"
#include "next/application/native_english_teacher_directory_read_query.h"
#include "next/application/native_english_teacher_directory_save_use_case.h"
#include "next/application/gs_team_directory_read_query.h"
#include "next/application/gs_team_directory_save_use_case.h"
#include "next/application/workspace_contracts.h"

#include <memory>
#include <optional>

class WorkspaceStore;

namespace ClassMngr::Next::Platform
{

// Store-backed persistence boundary for the Next startup workspace. The
// adapter borrows the single WorkspaceStore owned by the composition root.
class WorkspaceStoreGateway final
    : public Application::WorkspaceGateway,
      public Application::KoreanTeacherBirthdayDirectoryReadPort,
      public Application::NativeEnglishTeacherBirthdayDirectoryReadPort,
      public Application::GsTeamBirthdayDirectoryReadPort
{
public:
    WorkspaceStoreGateway();
    ~WorkspaceStoreGateway() override;

    WorkspaceStoreGateway(const WorkspaceStoreGateway&) = delete;
    WorkspaceStoreGateway& operator=(const WorkspaceStoreGateway&) = delete;

    [[nodiscard]] Domain::Result<Application::WorkspaceSession>
    createWorkspace(
        const Application::CreateWorkspaceRequest& request
        ) override;

    [[nodiscard]] Domain::Result<Application::WorkspaceSession>
    openWorkspace(
        const Application::OpenWorkspaceRequest& request
        ) override;

    [[nodiscard]] Domain::Result<void> closeWorkspace(
        const Application::CloseWorkspaceRequest& request
        ) override;

    [[nodiscard]] Domain::Result<void> saveWorkspace(
        const Application::SaveWorkspaceRequest& request
        ) override;

    [[nodiscard]] Domain::Result<void> discardWorkspaceChanges(
        const Application::DiscardWorkspaceChangesRequest& request
        ) override;

    [[nodiscard]] Domain::Result<Application::WorkspaceLocation>
    saveWorkspaceAs(
        const Application::SaveWorkspaceAsRequest& request
        ) override;

    [[nodiscard]] Domain::Result<Application::WorkspaceLocation>
    exportWorkspace(
        const Application::ExportWorkspaceRequest& request
        ) override;

    // Setup feature operations share the active store owned by this gateway.
    [[nodiscard]] Application::PersonalDetailsReadResult
    readPersonalDetails() const;
    [[nodiscard]] Application::PersonalDetailsSaveResult
    savePersonalDetails(
        const Application::PersonalDetailsSaveRequest& request
        ) const;
    [[nodiscard]] Application::InitialSetupTeacherChoicesResult
    loadInitialSetupTeacherChoices() const;
    [[nodiscard]] Domain::Result<Application::CustomColorPalette>
    readInitialSetupClassColorPalette() const;
    [[nodiscard]] Domain::Result<void> saveInitialSetupClassColorPalette(
        const Application::CustomColorPalette& palette
        ) const;
    [[nodiscard]] Application::TeacherCreateResult createInitialSetupTeacher(
        const Application::TeacherCreateRequest& request
        ) const;
    [[nodiscard]] Application::TeacherDirectoryReadResult
    readTeacherDirectory() const;
    [[nodiscard]] Application::TeacherCreateResult createTeacher(
        const Application::TeacherCreateRequest& request
        ) const;
    [[nodiscard]] Application::TeacherDeleteResult deleteTeacher(
        const Application::TeacherDeleteRequest& request
        ) const;
    [[nodiscard]] Application::TeacherProfileReadResult readTeacherProfile(
        const Domain::TeacherId& id
        ) const;
    [[nodiscard]] Application::TeacherProfileValidationResult
    validateTeacherProfile(
        const Domain::TeacherProfile& profile
        ) const;
    [[nodiscard]] Application::TeacherProfileEditResult editTeacherProfile(
        const Application::TeacherProfileEditRequest& request
        ) const;
    [[nodiscard]] Application::ClassCreateResult createInitialSetupClass() const;
    [[nodiscard]] Application::InitialSetupClassTimesSaveResult
    saveInitialSetupClassTimes(
        const Application::InitialSetupClassTimesSaveRequest& request
        ) const;
    [[nodiscard]] std::expected<
        Application::TeacherImportUseCaseResult,
        Application::TeacherImportUseCaseError
        > applyTeacherImportUseCase(
            const Application::TeacherImportUseCaseRequest& request
            ) const;
    [[nodiscard]] Application::TeacherImportLatestSourceDateReadResult
    readLatestTeacherImportSourceDate() const;
    [[nodiscard]] Application::KoreanTeacherBirthdayDirectoryReadResult
    readKoreanTeacherBirthdayDirectory() const override;
    [[nodiscard]] Application::NativeEnglishTeacherBirthdayDirectoryReadResult
    readNativeEnglishTeacherBirthdayDirectory() const override;
    [[nodiscard]] Application::GsTeamBirthdayDirectoryReadResult
    readGsTeamBirthdayDirectory() const override;
    [[nodiscard]] Application::NativeEnglishTeacherDirectoryReadResult
    readNativeEnglishTeacherDirectory() const;
    [[nodiscard]] Application::NativeEnglishTeacherDirectorySaveOutcome
    saveNativeEnglishTeacherDirectory(
        const Application::NativeEnglishTeacherDirectorySaveRequest& request
        ) const;
    [[nodiscard]] Application::GsTeamDirectoryReadResult
    readGsTeamDirectory() const;
    [[nodiscard]] Application::GsTeamDirectorySaveOutcome saveGsTeamDirectory(
        const Application::GsTeamDirectorySaveRequest& request
        ) const;

private:
    [[nodiscard]] Domain::Result<Application::WorkspaceSession> sessionForOpenStore() const;
    [[nodiscard]] Domain::Result<void> validateSession(
        const Application::WorkspaceSession& session
        ) const;

    std::unique_ptr<WorkspaceStore> m_store;
    std::optional<Domain::WorkspaceId> m_activeWorkspaceId;
};

} // namespace ClassMngr::Next::Platform
