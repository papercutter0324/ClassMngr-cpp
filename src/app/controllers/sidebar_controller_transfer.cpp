#include "sidebar_controller_p.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "app/services/feature_services.h"

using namespace SidebarControllerPrivate;

#include "core/utils/file_name_utils.h"
#include "features/classes/services/class_transfer_json_codec.h"
#include "features/classes/ui/class_export_dialog.h"
#include "features/classes/ui/class_import_dialog.h"
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
        classes,
        teachers,
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

void SidebarController::exportClass(
    int classId
    )
{
    auto* classes = openClassService(m_services);

    if (!classes || !m_pages || !m_sidebar || classId <= 0)
    {
        return;
    }

    if (!m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    const Result<Classroom> classroom = classes->classroom(classId);

    if (!classroom)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Export Class"),
            tr("The class could not be loaded."),
            classroom.error()
            );
        return;
    }

    saveClassExport(
        {classId},
        classDisplayName(*classroom),
        tr("Export Class")
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

    const auto package =
        classes->buildTransferPackage(classIds);

    if (!package)
    {
        DialogServices::showWarning(
            m_sidebar, dialogTitle, package.error());
        return;
    }

    const QString filePath = normalizedJsonPath(*selection);
    const Status saved = ClassTransferJsonCodec::saveFile(
        filePath, *package);

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
        classes, teachers, *package, *preview, m_sidebar);

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const QStringList selectedKeys = m_sidebar->selectedKeys();
    const int selectedClassId = m_sidebar->getSelectedClassId();
    const auto summary = classes->importClasses(
        *package, dialog.importPlan());

    if (!summary)
    {
        DialogServices::showWarning(
            m_sidebar, tr("Import Classes"), summary.error());
        return;
    }

    refreshAllSidebars();
    m_pages->refreshAll();

    int firstAffectedClassId = -1;

    if (!summary->createdClassIds.isEmpty())
    {
        firstAffectedClassId = summary->createdClassIds.first();
    }
    else if (!summary->replacedClassIds.isEmpty())
    {
        firstAffectedClassId = summary->replacedClassIds.first();
    }

    if (firstAffectedClassId > 0)
    {
        m_pages->classesPage()->openClass(
            firstAffectedClassId, ClassesSection::Details);
        m_pages->showPage(PageType::Classes);
        m_sidebar->selectClass(firstAffectedClassId);
    }
    else
    {
        m_sidebar->selectByKeys(selectedKeys, selectedClassId);
    }

    DialogServices::showInformation(
        m_sidebar,
        tr("Import Classes"),
        tr("Import complete. Created: %1, replaced: %2, skipped: %3.")
            .arg(summary->createdClassIds.size())
            .arg(summary->replacedClassIds.size())
            .arg(summary->skippedClassCount)
        );
}
