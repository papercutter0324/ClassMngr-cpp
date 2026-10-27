#include "core/process_memory_snapshot.h"
#include "core/utils/sidebar_node_naming.h"
#include "data/database/workspace_store.h"
#include "domain/models/teacher.h"
#include "next/application/preferences_model.h"
#include "next/application/teacher_directory_read_port.h"
#include "next/domain/teacher_display_name.h"
#include "next/platform/workspace_store_gateway.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/application_shell.h"
#include "next/ui/font_size_presenter.h"
#include "next/ui/navigation_font_policy.h"
#include "next/ui/page_host.h"
#include "next/ui/placeholder_page.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QScrollBar>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTranslator>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QUuid>
#include <QWidget>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>

using namespace ClassMngr;

namespace
{

constexpr QSize Viewport{1024, 768};
constexpr std::size_t ExpectedFixtureTeacherCount = 24;

class EmptyPreferencesReadPort final
    : public Next::Application::PreferencesStateReadPort
{
public:
    [[nodiscard]] Next::Application::PreferencesStateReadResult read() const override
    {
        return Next::Application::PreferencesStateReadResult::success({});
    }
};

class EmptyPreferencesPersistPort final
    : public Next::Application::PreferencesSnapshotPersistPort
{
public:
    [[nodiscard]] Next::Application::PreferencesPersistResult persist(
        const Next::Application::UserPreferencesSnapshot&)
        const override
    {
        return Next::Application::PreferencesPersistResult::success();
    }
};

[[nodiscard]] QString readTextFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

[[nodiscard]] bool executeFixture(
    const QString& profilePath,
    QString* errorMessage
    )
{
    const QString fixturePath = QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR))
        .filePath(QStringLiteral("tests/fixtures/workspaces/large_startup.sql"));
    QFile fixture(fixturePath);
    if (!fixture.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (errorMessage)
        {
            *errorMessage = fixture.errorString();
        }
        return false;
    }

    const QStringList statements = QString::fromUtf8(fixture.readAll())
        .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    const QString connectionName = QStringLiteral("teacher-directory-capture-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool executed = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(profilePath);
        if (!database.open())
        {
            if (errorMessage)
            {
                *errorMessage = database.lastError().text();
            }
        }
        else
        {
            executed = true;
            QSqlQuery query(database);
            for (const QString& rawStatement : statements)
            {
                const QString statement = rawStatement.trimmed();
                if (statement.isEmpty())
                {
                    continue;
                }
                if (!query.exec(statement))
                {
                    if (errorMessage)
                    {
                        *errorMessage = QStringLiteral("%1: %2")
                            .arg(statement.simplified().left(140),
                                 query.lastError().text());
                    }
                    executed = false;
                    break;
                }
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return executed;
}

[[nodiscard]] bool loadLargeFixtureDirectory(
    Next::Platform::WorkspaceStoreGateway& gateway,
    QTemporaryDir& temporaryDirectory,
    Next::Application::TeacherDirectoryReadSnapshot* snapshot,
    QString* errorMessage
    )
{
    const QString profilePath = temporaryDirectory.filePath(
        QStringLiteral("large-teacher-directory.tps")
        );
    const auto opened = gateway.createWorkspace({
        .location = Next::Application::WorkspaceLocation(
            profilePath.toUtf8().toStdString()
            )
    });
    if (!opened)
    {
        if (errorMessage)
        {
            *errorMessage = QString::fromUtf8(
                opened.error().message.data(),
                static_cast<qsizetype>(opened.error().message.size())
                );
        }
        return false;
    }
    if (!executeFixture(profilePath, errorMessage))
    {
        return false;
    }

    const Next::Application::TeacherDirectoryReadResult loaded =
        gateway.readTeacherDirectory();
    if (!loaded)
    {
        if (errorMessage)
        {
            *errorMessage = QString::fromUtf8(
                loaded.error().message.data(),
                static_cast<qsizetype>(loaded.error().message.size())
                );
        }
        return false;
    }
    *snapshot = loaded.value();
    return snapshot->teachers.size() == ExpectedFixtureTeacherCount;
}

[[nodiscard]] int countTreeItems(const QTreeWidgetItem* item)
{
    if (!item)
    {
        return 0;
    }
    int count = 1;
    for (int index = 0; index < item->childCount(); ++index)
    {
        count += countTreeItems(item->child(index));
    }
    return count;
}

[[nodiscard]] int countTeacherTreeItems(const QTreeWidgetItem* item)
{
    if (!item)
    {
        return 0;
    }
    int count = item->data(0, Qt::UserRole + 3).isValid() ? 1 : 0;
    for (int index = 0; index < item->childCount(); ++index)
    {
        count += countTeacherTreeItems(item->child(index));
    }
    return count;
}

[[nodiscard]] QTreeWidgetItem* itemByNodeId(
    QTreeWidget* tree,
    const QString& id
    )
{
    QTreeWidgetItemIterator iterator(tree);
    while (*iterator)
    {
        if ((*iterator)->data(0, Qt::UserRole + 2).toString() == id)
        {
            return *iterator;
        }
        ++iterator;
    }
    return nullptr;
}

[[nodiscard]] QString teacherDisplayName(
    const Next::Application::TeacherDirectoryEntry& teacher
    )
{
    const Next::Domain::TeacherDisplayName selected =
        Next::Domain::TeacherDisplayName::select(
            teacher.preferredName,
            teacher.teacherEn,
            teacher.preferredRomanization,
            teacher.teacherKr
            );
    const QString value = QString::fromStdU16String(selected.value());
    return value.isEmpty()
        ? QCoreApplication::translate("KoreanTeacherListPage", "New Teacher")
        : value;
}

[[nodiscard]] int widgetCount(const QWidget* root)
{
    return root
        ? 1 + root->findChildren<QWidget*>(QString{}, Qt::FindChildrenRecursively)
                  .size()
        : 0;
}

[[nodiscard]] QJsonObject memoryJson(
    const ProcessMemorySnapshot& before,
    const ProcessMemorySnapshot& rendered,
    const ProcessMemorySnapshot& released
    )
{
    const quint64 peakResidentBytes = qMax(
        rendered.peakWorkingSetBytes,
        released.peakWorkingSetBytes
        );
    return {
        {QStringLiteral("platform"), rendered.platform},
        {QStringLiteral("available"), rendered.isAvailable},
        {QStringLiteral("provider"), QStringLiteral(
             "PlatformProcessMemorySnapshotProvider using macOS TASK_VM_INFO")},
        {QStringLiteral("residentBytesBeforeWidgets"),
         static_cast<double>(before.workingSetBytes)},
        {QStringLiteral("residentBytesRendered"),
         static_cast<double>(rendered.workingSetBytes)},
        {QStringLiteral("peakResidentBytesAtRenderedSnapshot"),
         static_cast<double>(rendered.peakWorkingSetBytes)},
        {QStringLiteral("peakResidentBytesProcessAfterRelease"),
         static_cast<double>(peakResidentBytes)},
        {QStringLiteral("physicalFootprintBytesRendered"),
         static_cast<double>(rendered.privateUsageBytes)},
        {QStringLiteral("internalResidentBytesRendered"),
         static_cast<double>(rendered.privateWorkingSetBytes)},
        {QStringLiteral("compressedBytesRendered"),
         static_cast<double>(rendered.pagefileUsageBytes)},
        {QStringLiteral("residentBytesAfterRelease"),
         static_cast<double>(released.workingSetBytes)},
        {QStringLiteral("peakResidentBytesAfterRelease"),
         static_cast<double>(released.peakWorkingSetBytes)},
        {QStringLiteral("metricNotes"), QStringLiteral(
             "macOS resident and peak resident are available. physicalFootprintBytes "
             "is the provider's phys_footprint value; internalResidentBytes is "
             "TASK_VM_INFO.internal. The provider has no process commit metric. "
             "compressedBytes is compressed memory, not commit. No separate "
             "private-bytes counter is reported by this capture.")}
    };
}

[[nodiscard]] bool capturePair(
    Next::Resources::ResourceLoader& resourceLoader,
    const Next::Application::TeacherDirectoryReadSnapshot& snapshot,
    const QString& outputDirectory,
    const QString& locale,
    const QString& theme,
    QString* errorMessage
    )
{
    const QString resourceRoot = QString::fromUtf8(
        CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
    const QString stylesheet = readTextFile(QDir(resourceRoot).filePath(
        QStringLiteral("assets/styles/%1.qss").arg(theme)));
    if (stylesheet.isEmpty())
    {
        *errorMessage = QStringLiteral("Could not read the %1 theme stylesheet.")
            .arg(theme);
        return false;
    }
    qApp->setStyleSheet(stylesheet);

    std::unique_ptr<QTranslator> translator;
    if (locale == QStringLiteral("ko_KR"))
    {
        translator = std::make_unique<QTranslator>();
        const QString translationRoot = QFileInfo(QString::fromUtf8(
            CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG)).absolutePath();
        const QString translationPath = QDir(translationRoot).filePath(
            QStringLiteral("assets/translations/ClassMngr_ko_KR.qm"));
        if (!translator->load(translationPath)
            || !qApp->installTranslator(translator.get()))
        {
            *errorMessage = QStringLiteral("Could not load Korean translations.");
            return false;
        }
    }

    PlatformProcessMemorySnapshotProvider memoryProvider;
    const ProcessMemorySnapshot before = memoryProvider.snapshot();
    const int widgetsBefore = QApplication::allWidgets().size();
    QPointer<Sidebar> legacyGuard;
    QPointer<Next::Ui::ApplicationShell> nextGuard;
    QJsonObject surfaces;
    ProcessMemorySnapshot rendered;
    int widgetsAfterRelease = -1;
    QString legacyTreeFontFamily;
    QString nextTreeFontFamily;
    qreal legacyTreeFontPointSize = -1.0;
    qreal nextTreeFontPointSize = -1.0;
    int legacyTreeIndentationPixels = -1;
    int nextTreeIndentationPixels = -1;
    int legacyTeacherRowHeightPixels = -1;
    int nextTeacherRowHeightPixels = -1;
    int legacyTopLevelRowHeightPixels = -1;
    int nextTopLevelRowHeightPixels = -1;

    {
        Sidebar legacySidebar;
        legacyGuard = &legacySidebar;
        legacySidebar.setDatabaseSectionsVisible(true);
        struct LegacyTeacherRow final
        {
            qint64 id;
            Teacher fields;
        };
        std::vector<LegacyTeacherRow> legacyTeachers;
        legacyTeachers.reserve(snapshot.teachers.size());
        for (const Next::Application::TeacherDirectoryEntry& entry :
             snapshot.teachers)
        {
            bool idOk = false;
            const qint64 id = QString::fromStdString(entry.teacherId.value())
                .toLongLong(&idOk);
            if (!idOk || id <= 0
                || id > std::numeric_limits<int>::max())
            {
                *errorMessage = QStringLiteral("Fixture returned a bad legacy teacher ID.");
                return false;
            }
            Teacher legacyTeacher{
                .id = static_cast<int>(id),
                .teacherKr = QString::fromStdU16String(entry.teacherKr),
                .teacherEn = QString::fromStdU16String(entry.teacherEn),
                .preferredRomanization = QString::fromStdU16String(
                    entry.preferredRomanization),
                .preferredName = QString::fromStdU16String(entry.preferredName)
            };
            legacyTeachers.push_back({id, std::move(legacyTeacher)});
        }
        std::sort(
            legacyTeachers.begin(),
            legacyTeachers.end(),
            [](const LegacyTeacherRow& left, const LegacyTeacherRow& right) {
                if (SidebarNodeNaming::teacherDisplayLessThan(
                        left.fields, right.fields))
                {
                    return true;
                }
                if (SidebarNodeNaming::teacherDisplayLessThan(
                        right.fields, left.fields))
                {
                    return false;
                }
                return left.id < right.id;
            }
            );
        for (const LegacyTeacherRow& teacher : legacyTeachers)
        {
            legacySidebar.addTeacherNode(
                SidebarNodeNaming::formatTeacherDisplayName(teacher.fields),
                teacher.id,
                false
                );
        }

        auto* const legacyTree = legacySidebar.findChild<QTreeWidget*>(
            QStringLiteral("sidebarTree"));
        if (!legacyTree || legacyTree->topLevelItemCount() <= 4)
        {
            *errorMessage = QStringLiteral("Legacy teacher sidebar tree is unavailable.");
            return false;
        }
        legacyTree->collapseAll();
        QTreeWidgetItem* const campusStaff = legacyTree->topLevelItem(4);
        if (!campusStaff || campusStaff->childCount() == 0)
        {
            *errorMessage = QStringLiteral("Legacy Campus Staff group is unavailable.");
            return false;
        }
        campusStaff->setExpanded(true);
        QTreeWidgetItem* const koreanTeachers = campusStaff->child(0);
        koreanTeachers->setExpanded(true);
        QTreeWidgetItem* const firstLegacyTeacher = koreanTeachers->child(0);
        if (!firstLegacyTeacher)
        {
            *errorMessage = QStringLiteral("Legacy Korean teacher rows are empty.");
            return false;
        }
        legacyTree->clearSelection();
        legacyTree->setCurrentItem(nullptr);
        legacyTree->setFixedSize(Viewport);
        const int legacyTeacherItems = countTeacherTreeItems(campusStaff);
        const int legacyTreeItems = countTreeItems(campusStaff);
        const int legacyWidgets = widgetCount(legacyTree);
        legacyTreeFontFamily = legacyTree->font().family();
        legacyTreeFontPointSize = legacyTree->font().pointSizeF();
        legacyTreeIndentationPixels = legacyTree->indentation();

        legacySidebar.resize(Viewport);
        legacySidebar.show();
        QApplication::processEvents();
        legacyTree->verticalScrollBar()->setValue(0);
        legacyTree->horizontalScrollBar()->setValue(0);
        legacyTeacherRowHeightPixels =
            legacyTree->visualItemRect(firstLegacyTeacher).height();
        legacyTopLevelRowHeightPixels =
            legacyTree->visualItemRect(legacyTree->topLevelItem(0)).height();
        if (QWidget* const focused = QApplication::focusWidget())
        {
            focused->clearFocus();
        }
        QApplication::processEvents();
        const QString legacyPath = QDir(outputDirectory).filePath(
            QStringLiteral("legacy_%1-%2.png").arg(locale, theme));
        const QImage legacyImage = legacyTree->grab().toImage();
        if (legacyImage.size() != Viewport || !legacyImage.save(legacyPath, "PNG"))
        {
            *errorMessage = QStringLiteral("Could not save the legacy sidebar tree capture.");
            return false;
        }

        EmptyPreferencesReadPort preferencesReadPort;
        EmptyPreferencesPersistPort preferencesPersistPort;
        Next::Application::PreferencesModel preferences(
            preferencesReadPort,
            preferencesPersistPort
            );
        Next::Ui::ApplicationShell nextShell(resourceLoader, preferences);
        nextGuard = &nextShell;
        const auto registerPage = [&nextShell](
            const QString& id,
            const QString& label
            ) {
            const auto pageId = Next::Ui::PageId::fromString(id);
            return pageId
                && nextShell.pageHost().registerPage(Next::Ui::PageDescriptor{
                    .id = *pageId,
                    .label = label,
                    .factory = [label](const Next::Ui::PageContext& context)
                        -> Next::Ui::PageOperationResult<
                            std::unique_ptr<Next::Ui::Page>> {
                        return std::unique_ptr<Next::Ui::Page>(
                            std::make_unique<Next::Ui::PlaceholderPage>(
                                context,
                                label
                                )
                            );
                    }
                });
        };
        const QString teacherProfileLabel = QCoreApplication::translate(
            "TeacherProfilePage", "Teacher Profile");
        if (!registerPage(QStringLiteral("teacher_profile"), teacherProfileLabel)
            || !registerPage(QStringLiteral("my_workspace"),
                QCoreApplication::translate("NextNavigation", "My Workspace"))
            || !registerPage(QStringLiteral("classes"),
                QCoreApplication::translate("NextNavigation", "Classes"))
            || !registerPage(QStringLiteral("sub_prep"),
                QCoreApplication::translate("NextNavigation", "Sub Prep"))
            || !registerPage(QStringLiteral("native_english_teachers"),
                QCoreApplication::translate("NextNavigation", "Native English Teachers"))
            || !registerPage(QStringLiteral("gs_team"),
                QCoreApplication::translate("NextNavigation", "GS Team")))
        {
            *errorMessage = QStringLiteral("Next sidebar page routes did not register.");
            return false;
        }

        const Next::Ui::PageId profilePageId = *Next::Ui::PageId::fromString(
            QStringLiteral("teacher_profile"));
        using Next::Ui::NavigationNode;
        using Next::Ui::NavigationWorkspaceAvailability;
        constexpr auto workspace =
            NavigationWorkspaceAvailability::WorkspaceOpen;
        const std::vector<NavigationNode> nodes{
            NavigationNode::page(
                *Next::Ui::PageId::fromString(QStringLiteral("my_workspace")),
                workspace
                ),
            NavigationNode::page(
                *Next::Ui::PageId::fromString(QStringLiteral("classes")),
                workspace
                ),
            NavigationNode::page(
                *Next::Ui::PageId::fromString(QStringLiteral("sub_prep")),
                workspace
                ),
            NavigationNode::group(
                QStringLiteral("co_teachers"),
                QCoreApplication::translate("NextNavigation", "Co-Teachers"),
                {},
                workspace,
                true
                ),
            NavigationNode::group(
                QStringLiteral("campus_staff"),
                        QCoreApplication::translate("NextNavigation", "Campus Staff"),
                {
                    NavigationNode::group(
                        QStringLiteral("teachers_all_korean"),
                        QCoreApplication::translate(
                            "NextNavigation", "Korean Teachers"),
                        {
                            NavigationNode::page(
                                profilePageId,
                                workspace,
                                false
                                )
                        },
                        workspace,
                        true
                        ),
                    NavigationNode::page(
                        *Next::Ui::PageId::fromString(
                            QStringLiteral("native_english_teachers")),
                        workspace
                        ),
                    NavigationNode::page(
                        *Next::Ui::PageId::fromString(
                            QStringLiteral("gs_team")),
                        workspace
                        )
                },
                workspace
                ),
            NavigationNode::group(
                QStringLiteral("useful_links"),
                QCoreApplication::translate("NextNavigation", "Useful Links"),
                {
                    NavigationNode::placeholder(
                        QStringLiteral("useful_dropbox"),
                        QCoreApplication::translate("NextNavigation", "Dropbox"),
                        workspace
                        ),
                    NavigationNode::placeholder(
                        QStringLiteral("useful_calendar"),
                        QCoreApplication::translate("NextNavigation", "Calendar"),
                        workspace
                        )
                },
                workspace
                )
        };
        const auto configured = nextShell.configureNavigation(
            nodes,
            profilePageId,
            profilePageId,
            Next::Application::WorkspaceLifecycleState::Open
            );
        if (!configured)
        {
            *errorMessage = QStringLiteral("Next sidebar navigation did not configure: %1")
                .arg(configured.error().message);
            return false;
        }

        std::vector<Next::Ui::KoreanTeacherNavigationRow> nextRows;
        nextRows.reserve(snapshot.teachers.size());
        for (const Next::Application::TeacherDirectoryEntry& teacher :
             snapshot.teachers)
        {
            nextRows.push_back({
                .teacherId = teacher.teacherId,
                .label = teacherDisplayName(teacher)
            });
        }
        nextShell.setKoreanTeacherNavigationRows(std::move(nextRows));
        if (!nextShell.navigateTo(profilePageId, Next::Ui::NavigationCause::Initial))
        {
            *errorMessage = QStringLiteral("Next static teacher profile route did not activate.");
            return false;
        }

        auto* const nextTree = nextShell.findChild<QTreeWidget*>(
            QStringLiteral("classMngrNextNavigationTree"));
        QTreeWidgetItem* const nextCampusStaff = itemByNodeId(
            nextTree,
            QStringLiteral("campus_staff")
            );
        QTreeWidgetItem* const nextKoreanTeachers = itemByNodeId(
            nextTree,
            QStringLiteral("teachers_all_korean")
            );
        if (!nextTree || !nextCampusStaff || !nextKoreanTeachers
            || countTeacherTreeItems(nextCampusStaff)
                != static_cast<int>(ExpectedFixtureTeacherCount))
        {
            *errorMessage = QStringLiteral("Next sidebar teacher tree is incomplete.");
            return false;
        }
        nextCampusStaff->setExpanded(true);
        nextKoreanTeachers->setExpanded(true);
        auto* const firstNextTeacher = nextKoreanTeachers->child(0);
        if (!firstNextTeacher)
        {
            *errorMessage = QStringLiteral("Next Korean teacher sidebar rows are empty.");
            return false;
        }
        nextTree->setCurrentItem(nullptr);
        nextTree->setFixedSize(Viewport);
        const int nextTeacherItems = countTeacherTreeItems(nextCampusStaff);
        const int nextTreeItems = countTreeItems(nextCampusStaff);
        const int nextWidgets = widgetCount(nextTree);
        nextTreeFontFamily = nextTree->font().family();
        nextTreeFontPointSize = nextTree->font().pointSizeF();
        nextTreeIndentationPixels = nextTree->indentation();

        nextShell.resize(Viewport);
        nextShell.show();
        QApplication::processEvents();
        nextTree->verticalScrollBar()->setValue(0);
        nextTree->horizontalScrollBar()->setValue(0);
        nextTeacherRowHeightPixels =
            nextTree->visualItemRect(firstNextTeacher).height();
        nextTopLevelRowHeightPixels =
            nextTree->visualItemRect(nextTree->topLevelItem(0)).height();
        const bool navigationTypographyMatchesLegacy =
            legacyTreeFontPointSize
                == Next::Ui::NavigationFontPolicy::StandardPointSize
            && nextTreeFontPointSize == legacyTreeFontPointSize
            && legacyTreeFontFamily.compare(QStringLiteral("Inter"),
                                            Qt::CaseInsensitive) == 0
            && nextTreeFontFamily.compare(legacyTreeFontFamily,
                                          Qt::CaseInsensitive) == 0
            && legacyTreeIndentationPixels == 12
            && nextTreeIndentationPixels == legacyTreeIndentationPixels
            && legacyTeacherRowHeightPixels
                == Next::Ui::NavigationFontPolicy::MinimumRowHeightPixels
            && nextTeacherRowHeightPixels == legacyTeacherRowHeightPixels
            && nextTopLevelRowHeightPixels == legacyTopLevelRowHeightPixels;
        if (!navigationTypographyMatchesLegacy)
        {
            *errorMessage = QStringLiteral(
                "Next navigation font, indentation, or row metrics did not match legacy."
                );
            return false;
        }
        const QString nextPath = QDir(outputDirectory).filePath(
            QStringLiteral("next_%1-%2.png").arg(locale, theme));
        const QImage nextImage = nextTree->grab().toImage();
        if (nextImage.size() != Viewport || !nextImage.save(nextPath, "PNG"))
        {
            *errorMessage = QStringLiteral("Could not save the Next sidebar tree capture.");
            return false;
        }

        rendered = memoryProvider.snapshot();
        surfaces.insert(QStringLiteral("legacySidebarTree"), QJsonObject{
            {QStringLiteral("capture"), QFileInfo(legacyPath).fileName()},
            {QStringLiteral("teacherRows"), legacyTeacherItems},
            {QStringLiteral("itemsInCampusStaffSubtree"), legacyTreeItems},
            {QStringLiteral("widgetsInTreeSubtree"), legacyWidgets},
            {QStringLiteral("treeFontFamily"), legacyTreeFontFamily},
            {QStringLiteral("treeFontPointSize"), legacyTreeFontPointSize},
            {QStringLiteral("treeIndentationPixels"),
             legacyTreeIndentationPixels},
            {QStringLiteral("teacherRowHeightPixels"),
             legacyTeacherRowHeightPixels},
            {QStringLiteral("topLevelRowHeightPixels"),
             legacyTopLevelRowHeightPixels}
        });
        surfaces.insert(QStringLiteral("nextSidebarTree"), QJsonObject{
            {QStringLiteral("capture"), QFileInfo(nextPath).fileName()},
            {QStringLiteral("teacherRows"), nextTeacherItems},
            {QStringLiteral("itemsInCampusStaffSubtree"), nextTreeItems},
            {QStringLiteral("widgetsInTreeSubtree"), nextWidgets},
            {QStringLiteral("treeFontFamily"), nextTreeFontFamily},
            {QStringLiteral("treeFontPointSize"), nextTreeFontPointSize},
            {QStringLiteral("treeIndentationPixels"),
             nextTreeIndentationPixels},
            {QStringLiteral("teacherRowHeightPixels"),
             nextTeacherRowHeightPixels},
            {QStringLiteral("topLevelRowHeightPixels"),
             nextTopLevelRowHeightPixels}
        });
    }

    QApplication::processEvents();
    widgetsAfterRelease = QApplication::allWidgets().size();
    const ProcessMemorySnapshot released = memoryProvider.snapshot();
    if (!legacyGuard.isNull() || !nextGuard.isNull()
        || widgetsAfterRelease != widgetsBefore)
    {
        *errorMessage = QStringLiteral("Legacy sidebar or Next shell widgets remained after owner teardown.");
        return false;
    }

    const QJsonObject manifest{
        {QStringLiteral("purpose"), QStringLiteral(
             "Matched legacy and Next Korean teacher sidebar tree captures for #17")},
        {QStringLiteral("taskId"), QStringLiteral(
             "P7B17-KOREAN-TEACHER-LIST-VERIFICATION")},
        {QStringLiteral("viewport"), QJsonObject{
             {QStringLiteral("width"), Viewport.width()},
             {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("qtPlatform"), QStringLiteral("offscreen")},
        {QStringLiteral("style"), QStringLiteral("Fusion")},
        {QStringLiteral("locale"), locale},
        {QStringLiteral("theme"), theme},
        {QStringLiteral("fixture"), QStringLiteral(
             "tests/fixtures/workspaces/large_startup.sql")},
        {QStringLiteral("fixtureTeacherRows"),
         static_cast<int>(snapshot.teachers.size())},
        {QStringLiteral("navigationTypographyParity"), QJsonObject{
             {QStringLiteral("legacyAndNextPrimaryFamily"),
              nextTreeFontFamily},
             {QStringLiteral("legacyAndNextFontPointSize"),
              nextTreeFontPointSize},
             {QStringLiteral("legacyAndNextIndentationPixels"),
              nextTreeIndentationPixels},
             {QStringLiteral("legacyAndNextTeacherRowHeightPixels"),
              nextTeacherRowHeightPixels},
             {QStringLiteral("legacyAndNextTopLevelRowHeightPixels"),
              nextTopLevelRowHeightPixels},
             {QStringLiteral("matchesLegacyStandard"), true}}},
        {QStringLiteral("surfaceComparability"), QStringLiteral(
             "Both captures are the actual application QTreeWidget sidebar surfaces "
             "at the same fixed size, locale, theme, and fixture. The Korean teacher "
             "rows are under Campus Staff in each tree. Next uses the legacy Inter "
             "primary family, 12 px indentation, 14 pt font, and 30 px row metrics. "
             "The Next profile route is "
             "present in navigation history but hidden from the visible tree. This "
             "compares sidebar tree presentation, not full application windows.")},
        {QStringLiteral("widgetCountsBeforeAndAfter"), QJsonObject{
             {QStringLiteral("before"), widgetsBefore},
             {QStringLiteral("afterRelease"), widgetsAfterRelease}}},
        {QStringLiteral("ownerTeardown"), QJsonObject{
             {QStringLiteral("legacySidebarDestroyed"), legacyGuard.isNull()},
            {QStringLiteral("nextApplicationShellDestroyed"), nextGuard.isNull()}}},
        {QStringLiteral("surfaces"), surfaces},
        {QStringLiteral("memory"), memoryJson(before, rendered, released)}
    };
    const QString manifestPath = QDir(outputDirectory).filePath(
        QStringLiteral("manifest_%1-%2.json").arg(locale, theme));
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || manifestFile.write(QJsonDocument(manifest).toJson(
               QJsonDocument::Indented)) < 0)
    {
        *errorMessage = QStringLiteral("Could not write the capture manifest.");
        return false;
    }

    if (translator)
    {
        qApp->removeTranslator(translator.get());
    }
    qInfo().noquote() << "Captured" << locale << theme << "teacher directories with"
                      << snapshot.teachers.size() << "fixture rows in"
                      << outputDirectory;
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    if (argc != 4)
    {
        qCritical("Usage: capture OUTPUT_DIR LOCALE THEME");
        return 2;
    }

    const QString outputDirectory = QString::fromLocal8Bit(argv[1]);
    const QString locale = QString::fromLocal8Bit(argv[2]);
    const QString theme = QString::fromLocal8Bit(argv[3]);
    if ((locale != QStringLiteral("en_US") && locale != QStringLiteral("ko_KR"))
        || (theme != QStringLiteral("light") && theme != QStringLiteral("dark")))
    {
        qCritical("Supported captures are en_US light and ko_KR dark.");
        return 2;
    }
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create capture output directory");
        return 2;
    }

    const QString resourceRoot = QString::fromUtf8(
        CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
    const auto catalog = Next::Resources::ResourceCatalog::load(
        QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG), resourceRoot);
    if (!catalog)
    {
        qCritical().noquote() << "Could not load Next resource catalog:"
                              << catalog.error().message;
        return 1;
    }
    Next::Resources::ResourceLoader resourceLoader(std::move(*catalog));

    QString interFamily;
    QString pretendardFamily;
    for (const QString& font : {
             QStringLiteral("Inter.ttc"),
             QStringLiteral("PretendardVariable.ttf")
         })
    {
        const QString fontPath = QDir(resourceRoot).filePath(
            QStringLiteral("assets/fonts/") + font);
        const int fontId = QFile::exists(fontPath)
            ? QFontDatabase::addApplicationFont(fontPath)
            : -1;
        const QStringList families = fontId >= 0
            ? QFontDatabase::applicationFontFamilies(fontId)
            : QStringList{};
        if (families.empty())
        {
            qCritical().noquote() << "Could not register capture font:" << fontPath;
            return 1;
        }
        if (font == QStringLiteral("Inter.ttc"))
        {
            interFamily = families.front();
        }
        else
        {
            pretendardFamily = families.front();
        }
    }
    Next::Ui::FontSizePresenter fontSizePresenter(application);
    fontSizePresenter.applyStartupPreference(
        Next::Application::FontSize::Normal,
        locale,
        interFamily,
        pretendardFamily
        );

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
    {
        qCritical("Could not create temporary workspace directory");
        return 1;
    }
    Next::Platform::WorkspaceStoreGateway gateway;
    Next::Application::TeacherDirectoryReadSnapshot snapshot;
    QString errorMessage;
    if (!loadLargeFixtureDirectory(
            gateway,
            temporaryDirectory,
            &snapshot,
            &errorMessage))
    {
        qCritical().noquote() << "Could not load large teacher fixture:"
                              << errorMessage;
        return 1;
    }

    if (!capturePair(resourceLoader, snapshot, outputDirectory, locale, theme,
                     &errorMessage))
    {
        qCritical().noquote() << errorMessage;
        return 1;
    }
    return 0;
}
