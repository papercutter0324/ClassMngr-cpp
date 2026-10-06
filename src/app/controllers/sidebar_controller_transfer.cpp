#include "sidebar_controller_p.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "app/services/feature_services.h"

using namespace SidebarControllerPrivate;

#include "core/utils/file_name_utils.h"
#include "features/classes/services/class_transfer_json_codec.h"
#include "next/platform/application_services_class_transfer_export_source_read_port.h"
#include "features/classes/ui/class_export_dialog.h"
#include "features/classes/ui/class_import_dialog.h"
#include "next/application/class_transfer_apply_use_case.h"
#include "next/platform/application_services_class_transfer_apply_port.h"
#include "ui/shared/dialogs/file_dialog_service.h"

#include <QDir>
#include <QFileInfo>

namespace
{
const QString JsonSuffix = QStringLiteral(".json");

QString packageDirectory(
    const QString& databasePath
    )
{
    const QFileInfo databaseInfo(databasePath);

    return databaseInfo.absolutePath();
}

QString normalizedJsonPath(
    QString filePath
    )
{
    if (!filePath.endsWith(JsonSuffix, Qt::CaseInsensitive))
    {
        filePath += JsonSuffix;
    }

    return QFileInfo(filePath).absoluteFilePath();
}

}

void SidebarController::exportClasses()
{
    auto* classes = openClassService(m_services);
    auto* teachers = openTeacherService(m_services);

    if (!classes || !teachers || !m_pages || !m_sidebar)
    {
        return;
    }

    if (!m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    ClassExportDialog dialog(
        m_services,
        m_sidebar
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    saveClassExport(
        dialog.selectedClassIds(),
        tr("Classes"),
        tr("Export Classes")
        );
}

void SidebarController::saveClassExport(
    const QList<int>& classIds,
    const QString& suggestedBaseName,
    const QString& dialogTitle
    )
{
    auto* classes = openClassService(m_services);

    if (!classes || !m_sidebar || classIds.isEmpty())
    {
        return;
    }

    const std::optional<QString> selection =
        DialogServices::fileDialogs().saveFile(
            SaveFileRequest{
                .parent = m_sidebar,
                .title = dialogTitle,
                .purpose = FileDialogPurpose::ClassTransfer,
                .initialDirectory = packageDirectory(
                    m_services->currentDatabasePath()),
                .suggestedFileName =
                    FileNameUtils::filesystemSafeJsonFileName(
                        suggestedBaseName,
                        tr("Classes")
                        ),
                .nameFilters = {tr("JSON Files (*.json)")},
                .defaultSuffix = QStringLiteral("json")
            }
            );

    if (!selection)
    {
        return;
    }

    ClassMngr::Next::Application::ClassTransferExportRequest request;
    for (const int classId : classIds)
        request.classIds.push_back(*ClassMngr::Next::Domain::ClassId::fromString(std::to_string(classId)));
    ClassMngr::Next::Platform::ApplicationServicesClassTransferExportSourceReadPort source(*m_services);
    const auto package = ClassMngr::Next::Application::ClassTransferExportQuery(source).execute(request);

    if (!package)
    {
        DialogServices::showWarning(
            m_sidebar, dialogTitle, QString::fromUtf8(package.error().message.data(),
                static_cast<qsizetype>(package.error().message.size())));
        return;
    }

    const QString filePath = normalizedJsonPath(*selection);
    const Status saved = ClassTransferJsonCodec::saveFile(
        filePath, package.value());

    if (!saved)
    {
        DialogServices::showWarning(
            m_sidebar, dialogTitle, saved.error());
        return;
    }

    DialogServices::showInformation(
        m_sidebar,
        dialogTitle,
        tr("Exported %1 class(es) to:\n%2")
            .arg(classIds.size())
            .arg(QDir::toNativeSeparators(filePath))
        );
}

void SidebarController::importClasses()
{
    auto* classes = openClassService(m_services);
    auto* teachers = openTeacherService(m_services);

    if (!classes || !teachers || !m_pages || !m_sidebar)
    {
        return;
    }

    if (!m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    const std::optional<QString> selection =
        DialogServices::fileDialogs().openFile(
            OpenFileRequest{
                .parent = m_sidebar,
                .title = tr("Import Classes"),
                .purpose = FileDialogPurpose::ClassTransfer,
                .initialDirectory = packageDirectory(
                    m_services->currentDatabasePath()),
                .nameFilters = {tr("JSON Files (*.json)")}
            }
            );

    if (!selection)
    {
        return;
    }

    const auto package = ClassTransferJsonCodec::loadFile(*selection);

    if (!package)
    {
        DialogServices::showWarning(
            m_sidebar, tr("Import Classes"), package.error());
        return;
    }

    const auto preview = classes->previewImport(*package);

    if (!preview)
    {
        DialogServices::showWarning(
            m_sidebar, tr("Import Classes"), preview.error());
        return;
    }

    ClassImportDialog dialog(
        m_services,
        *package,
        *preview,
        m_sidebar
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const QStringList selectedKeys = m_sidebar->selectedKeys();
    ClassMngr::Next::Application::ClassTransferApplyCommand command{
        .package = *package,
        .choices = dialog.applyRequest()
    };
    ClassMngr::Next::Platform::ApplicationServicesClassTransferApplyPort port(
        *m_services
        );
    const auto summary =
        ClassMngr::Next::Application::ClassTransferApplyUseCase::execute(
            command,
            port
            );

    if (!summary)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Import Classes"),
            QString::fromUtf8(summary.error().message.data(),
                static_cast<qsizetype>(summary.error().message.size()))
            );
        return;
    }
    const auto& importedSummary = summary.value();

    refreshAllSidebars();
    m_pages->refreshAll();

    int firstAffectedClassId = -1;

    if (!importedSummary.createdClassIds.empty())
    {
        const auto id = ClassMngr::Next::Application::classTransferApplyDestinationId(
            importedSummary.createdClassIds.front()
            );
        firstAffectedClassId = id.value_or(-1);
    }
    else if (!importedSummary.replacedClassIds.empty())
    {
        const auto id = ClassMngr::Next::Application::classTransferApplyDestinationId(
            importedSummary.replacedClassIds.front()
            );
        firstAffectedClassId = id.value_or(-1);
    }

    if (firstAffectedClassId > 0)
    {
        if (auto* page = m_pages->ensureClassesPage())
        {
            page->openClass(
                firstAffectedClassId,
                ClassesSection::Details
                );
        }
        m_pages->showPage(PageType::Classes);
        m_sidebar->selectByKeys(
            {QStringLiteral("classes")}
            );
    }
    else
    {
        m_sidebar->selectByKeys(selectedKeys);
    }

    DialogServices::showInformation(
        m_sidebar,
        tr("Import Classes"),
        tr("Import complete. Created: %1, replaced: %2, skipped: %3.")
            .arg(importedSummary.createdClassIds.size())
            .arg(importedSummary.replacedClassIds.size())
            .arg(importedSummary.skippedClassCount)
        );
}
