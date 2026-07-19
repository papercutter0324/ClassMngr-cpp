#include "sidebar_controller_p.h"

#include "core/utils/file_name_utils.h"
#include "features/classes/services/class_transfer_json_codec.h"
#include "features/classes/ui/class_export_dialog.h"
#include "features/classes/ui/class_import_dialog.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

namespace
{
const QString JsonSuffix = QStringLiteral(".json");

QString packageDirectory(
    DataService* dataService
    )
{
    const QFileInfo databaseInfo(
        dataService ? dataService->currentDatabasePath() : QString());

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
    auto* dataService = openDataService(m_services);

    if (!dataService || !m_pages || !m_sidebar)
    {
        return;
    }

    if (!m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    ClassExportDialog dialog(
        dataService,
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
    auto* dataService = openDataService(m_services);

    if (!dataService || !m_pages || !m_sidebar || classId <= 0)
    {
        return;
    }

    if (!m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    const Classroom classroom = dataService->getClassById(classId);

    if (classroom.id <= 0)
    {
        return;
    }

    saveClassExport(
        {classId},
        classDisplayName(classroom),
        tr("Export Class")
        );
}

void SidebarController::saveClassExport(
    const QList<int>& classIds,
    const QString& suggestedBaseName,
    const QString& dialogTitle
    )
{
    auto* dataService = openDataService(m_services);

    if (!dataService || !m_sidebar || classIds.isEmpty())
    {
        return;
    }

    const QString selectedPath = QFileDialog::getSaveFileName(
        m_sidebar,
        dialogTitle,
        QDir(packageDirectory(dataService)).filePath(
            FileNameUtils::filesystemSafeJsonFileName(
                suggestedBaseName, tr("Classes"))),
        tr("JSON Files (*.json)")
        );

    if (selectedPath.isEmpty())
    {
        return;
    }

    const auto package =
        dataService->buildClassTransferPackage(classIds);

    if (!package)
    {
        QMessageBox::warning(
            m_sidebar, dialogTitle, package.error());
        return;
    }

    const QString filePath = normalizedJsonPath(selectedPath);
    const Status saved = ClassTransferJsonCodec::saveFile(
        filePath, *package);

    if (!saved)
    {
        QMessageBox::warning(
            m_sidebar, dialogTitle, saved.error());
        return;
    }

    QMessageBox::information(
        m_sidebar,
        dialogTitle,
        tr("Exported %1 class(es) to:\n%2")
            .arg(classIds.size())
            .arg(QDir::toNativeSeparators(filePath))
        );
}

void SidebarController::importClasses()
{
    auto* dataService = openDataService(m_services);

    if (!dataService || !m_pages || !m_sidebar)
    {
        return;
    }

    if (!m_pages->confirmCurrentPageCanLeave())
    {
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(
        m_sidebar,
        tr("Import Classes"),
        packageDirectory(dataService),
        tr("JSON Files (*.json)")
        );

    if (filePath.isEmpty())
    {
        return;
    }

    const auto package = ClassTransferJsonCodec::loadFile(filePath);

    if (!package)
    {
        QMessageBox::warning(
            m_sidebar, tr("Import Classes"), package.error());
        return;
    }

    const auto preview = dataService->previewClassImport(*package);

    if (!preview)
    {
        QMessageBox::warning(
            m_sidebar, tr("Import Classes"), preview.error());
        return;
    }

    ClassImportDialog dialog(
        dataService, *package, *preview, m_sidebar);

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const QStringList selectedKeys = m_sidebar->selectedKeys();
    const int selectedClassId = m_sidebar->getSelectedClassId();
    const auto summary = dataService->importClasses(
        *package, dialog.importPlan());

    if (!summary)
    {
        QMessageBox::warning(
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

    QMessageBox::information(
        m_sidebar,
        tr("Import Classes"),
        tr("Import complete. Created: %1, replaced: %2, skipped: %3.")
            .arg(summary->createdClassIds.size())
            .arg(summary->replacedClassIds.size())
            .arg(summary->skippedClassCount)
        );
}
