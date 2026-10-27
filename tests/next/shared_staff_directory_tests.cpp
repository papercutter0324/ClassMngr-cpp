#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/application_shell.h"
#include "next/ui/navigation_model.h"
#include "next/ui/page_host.h"
#include "next/ui/page_host_active_page_changes_adapter.h"
#include "next/ui/shared_staff_directory_page.h"

#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QtTest/QtTest>

#include <memory>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

class EmptyPreferences final
    : public Application::PreferencesStateReadPort,
      public Application::PreferencesSnapshotPersistPort
{
public:
    [[nodiscard]] Application::PreferencesStateReadResult read() const override
    {
        return Application::PreferencesStateReadResult::success({});
    }
    [[nodiscard]] Application::PreferencesPersistResult persist(
        const Application::UserPreferencesSnapshot&
        ) const override
    {
        return Application::PreferencesPersistResult::success();
    }
    Application::PreferencesModel model{*this, *this};
};

Application::NativeEnglishTeacherDirectorySnapshot nativeRecords(int count)
{
    Application::NativeEnglishTeacherDirectorySnapshot rows;
    rows.reserve(static_cast<std::size_t>(count));
    for (int row = 0; row < count; ++row)
    {
        rows.push_back({
            .id = Domain::NativeEnglishTeacherId(row + 1),
            .name = u"Native " + std::u16string(1, static_cast<char16_t>(u'A' + row % 26)),
            .position = row == 0 ? u"Co-ordinator" : u"NET",
            .phoneNumber = u"010-1000",
            .email = u"native@example.test",
            .birthday = u"01-02",
            .nationality = u"CA"
        });
    }
    return rows;
}

Application::GsTeamDirectorySnapshot gsRecords(int count)
{
    Application::GsTeamDirectorySnapshot rows;
    rows.reserve(static_cast<std::size_t>(count));
    for (int row = 0; row < count; ++row)
    {
        rows.push_back({
            .id = Domain::GsTeamMemberId(row + 100),
            .name = u"GS " + std::u16string(1, static_cast<char16_t>(u'A' + row % 26)),
            .koreanName = u"지나",
            .position = u"Free-form role",
            .phoneNumber = u"010-2000",
            .birthday = u"02-03"
        });
    }
    return rows;
}

int visibleLineEditors(const QTableView* table)
{
    int visible = 0;
    for (const QLineEdit* const editor : table->findChildren<QLineEdit*>())
    {
        visible += editor->isVisible();
    }
    return visible;
}

void closeVisibleEditor(QTableView* table)
{
    for (QLineEdit* const editor : table->findChildren<QLineEdit*>())
    {
        if (editor->isVisible())
        {
            QTest::keyClick(editor, Qt::Key_Escape);
            return;
        }
    }
}

struct DirectoryProbe final
{
    Application::NativeEnglishTeacherDirectorySnapshot native = nativeRecords(2);
    Application::GsTeamDirectorySnapshot gs = gsRecords(2);
    int nativeReads = 0;
    int gsReads = 0;
    int nativeSaves = 0;
    int gsSaves = 0;
    int nextNativeId = 10;
    int nextGsId = 110;
    bool confirmDelete = true;
    bool failNextNativeSave = false;
    bool failNextNativeRead = false;
    bool failNextGsSave = false;
    Application::NativeEnglishTeacherDirectorySaveRequest lastNativeSave;
    Application::GsTeamDirectorySaveRequest lastGsSave;
    QStringList warnings;

    [[nodiscard]] Ui::SharedStaffDirectoryPageActions actions()
    {
        return {
            .readNativeEnglishTeachers = [this] {
                ++nativeReads;
                if (failNextNativeRead)
                {
                    failNextNativeRead = false;
                    return Application::NativeEnglishTeacherDirectoryReadResult::failure({
                        .code = Domain::ErrorCode::Technical,
                        .message = "injected Native English read failure",
                        .recoverable = true
                    });
                }
                return Application::NativeEnglishTeacherDirectoryReadResult::success(native);
            },
            .saveNativeEnglishTeachers = [this](
                const Application::NativeEnglishTeacherDirectorySaveRequest& request
                ) {
                ++nativeSaves;
                lastNativeSave = request;
                if (failNextNativeSave)
                {
                    failNextNativeSave = false;
                    return Application::NativeEnglishTeacherDirectorySaveOutcome::failure({
                        .code = Domain::ErrorCode::Technical,
                        .message = "injected Native English save failure",
                        .recoverable = true
                    });
                }
                native.clear();
                for (const auto& row : request.rows)
                {
                    const Domain::NativeEnglishTeacherId id = row.id
                        ? *row.id
                        : Domain::NativeEnglishTeacherId(nextNativeId++);
                    native.push_back({
                        .id = id,
                        .name = row.name,
                        .position = row.position,
                        .phoneNumber = row.phoneNumber,
                        .email = row.email,
                        .birthday = row.birthday,
                        .nationality = row.nationality
                    });
                }
                return Application::NativeEnglishTeacherDirectorySaveOutcome::success();
            },
            .readGsTeam = [this] {
                ++gsReads;
                return Application::GsTeamDirectoryReadResult::success(gs);
            },
            .saveGsTeam = [this](
                const Application::GsTeamDirectorySaveRequest& request
                ) {
                ++gsSaves;
                lastGsSave = request;
                if (failNextGsSave)
                {
                    failNextGsSave = false;
                    return Application::GsTeamDirectorySaveOutcome::failure({
                        .code = Domain::ErrorCode::Technical,
                        .message = "injected GS Team save failure",
                        .recoverable = true
                    });
                }
                gs.clear();
                for (const auto& row : request.rows)
                {
                    const Domain::GsTeamMemberId id = row.id
                        ? *row.id
                        : Domain::GsTeamMemberId(nextGsId++);
                    gs.push_back({
                        .id = id,
                        .name = row.name,
                        .koreanName = row.koreanName,
                        .position = row.position,
                        .phoneNumber = row.phoneNumber,
                        .birthday = row.birthday
                    });
                }
                return Application::GsTeamDirectorySaveOutcome::success();
            },
            .confirmDelete = [this](int) { return confirmDelete; },
            .reportWarning = [this](const QString&, const QString& message) {
                warnings.push_back(message);
            },
            .saveMode = Application::SaveMode::Manual
        };
    }
};

} // namespace

class SharedStaffDirectoryTests final : public QObject
{
    Q_OBJECT

private slots:
    void compactModelKeepsTypedBranchesAndRefreshesInPlace();
    void routePageAddEditDeleteAndSavePreserveTypedIds();
    void saveFailureKeepsDraftForRetry();
    void activeRefreshReadsExternalChangesInPlace();
    void dirtyRouteLeaveGuardRejectsAndRetainsDraft();

private:
    [[nodiscard]] QString catalogPath() const;
    [[nodiscard]] QString resourceRoot() const;
};

QString SharedStaffDirectoryTests::catalogPath() const
{
    return QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG);
}

QString SharedStaffDirectoryTests::resourceRoot() const
{
    return QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
}

void SharedStaffDirectoryTests::compactModelKeepsTypedBranchesAndRefreshesInPlace()
{
    Ui::SharedStaffDirectoryTableModel native(
        Ui::StaffDirectoryKind::NativeEnglishTeachers);
    const auto* const identity = &native;
    const auto one = nativeRecords(1);
    native.replaceRows(one);
    QCOMPARE(native.rowCount(), 1);
    QCOMPARE(native.columnCount(), 6);
    QCOMPARE(native.storedRecordCount(), std::size_t(1));
    QCOMPARE(native.nativeRow(0).id,
        std::optional<Domain::NativeEnglishTeacherId>(Domain::NativeEnglishTeacherId(1)));
    QCOMPARE(native.index(0, 1).data(Qt::DisplayRole).toString(),
        QStringLiteral("Coordinator"));
    QCOMPARE(native.index(0, 1).data(Qt::EditRole).toString(),
        QStringLiteral("Co-ordinator"));

    const auto many = nativeRecords(180);
    native.replaceRows(many);
    QCOMPARE(&native, identity);
    QCOMPARE(native.rowCount(), 180);
    QCOMPARE(native.storedRecordCount(), std::size_t(180));
    QCOMPARE(native.columnCount(), 6);
    QCOMPARE(native.nativeRow(179).id,
        std::optional<Domain::NativeEnglishTeacherId>(Domain::NativeEnglishTeacherId(180)));

    Ui::SharedStaffDirectoryTableModel gs(Ui::StaffDirectoryKind::GsTeam);
    gs.replaceRows(gsRecords(180));
    QCOMPARE(gs.rowCount(), 180);
    QCOMPARE(gs.columnCount(), 5);
    QCOMPARE(gs.storedRecordCount(), std::size_t(180));
    QCOMPARE(gs.gsRow(0).id,
        std::optional<Domain::GsTeamMemberId>(Domain::GsTeamMemberId(100)));
    QCOMPARE(gs.index(0, 2).data(Qt::EditRole).toString(),
        QStringLiteral("Free-form role"));
}

void SharedStaffDirectoryTests::routePageAddEditDeleteAndSavePreserveTypedIds()
{
    auto catalog = Resources::ResourceCatalog::load(catalogPath(), resourceRoot());
    QVERIFY2(catalog.has_value(), catalog ? "" : qPrintable(catalog.error().message));
    Resources::ResourceLoader loader(std::move(*catalog));
    EmptyPreferences preferences;
    Ui::PageHost host(loader, preferences.model);
    DirectoryProbe probe;
    const auto nativePageId = *Ui::PageId::fromString(
        QStringLiteral("native_english_teachers"));
    const auto gsPageId = *Ui::PageId::fromString(QStringLiteral("gs_team"));
    const auto registerStaffPage = [&host, &probe](
        const Ui::PageId& id,
        const Ui::StaffDirectoryKind kind
        ) {
        return host.registerPage({
            .id = id,
            .label = id.value(),
            .factory = [&probe, kind](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                return std::unique_ptr<Ui::Page>(
                    std::make_unique<Ui::SharedStaffDirectoryPage>(
                        context, kind, probe.actions()));
            }
        });
    };
    QVERIFY(registerStaffPage(nativePageId,
        Ui::StaffDirectoryKind::NativeEnglishTeachers));
    QVERIFY(registerStaffPage(gsPageId, Ui::StaffDirectoryKind::GsTeam));
    host.resize(960, 700);
    host.show();
    QApplication::processEvents();
    QVERIFY(host.activatePage(nativePageId.value()));

    auto* const nativePage = dynamic_cast<Ui::SharedStaffDirectoryPage*>(
        host.pageInstance(nativePageId.value()));
    QVERIFY(nativePage);
    QCOMPARE(nativePage->directoryModel()->rowCount(), 2);
    QVERIFY(!nativePage->tableView()->isSortingEnabled());
    QVERIFY(nativePage->tableView()->model() == nativePage->directoryModel());
    QVERIFY(qobject_cast<QTableWidget*>(nativePage->tableView()) == nullptr);
    QCOMPARE(nativePage->directoryModel()->storedRecordCount(), std::size_t(2));
    int nativeCellWidgets = 0;
    for (int row = 0; row < nativePage->directoryModel()->rowCount(); ++row)
    {
        for (int column = 0; column < nativePage->directoryModel()->columnCount(); ++column)
        {
            nativeCellWidgets += nativePage->tableView()->indexWidget(
                nativePage->directoryModel()->index(row, column)) != nullptr;
        }
    }
    QCOMPARE(nativeCellWidgets, 0);
    QCOMPARE(visibleLineEditors(nativePage->tableView()), 0);
    nativePage->tableView()->edit(nativePage->directoryModel()->index(0, 0));
    QApplication::processEvents();
    QCOMPARE(visibleLineEditors(nativePage->tableView()), 1);
    closeVisibleEditor(nativePage->tableView());
    QApplication::processEvents();
    QCOMPARE(visibleLineEditors(nativePage->tableView()), 0);

    // Refresh through a cached page instance and verify the model object is
    // retained while a larger record set replaces it.
    auto* const modelIdentity = nativePage->directoryModel();
    probe.native = nativeRecords(90);
    QVERIFY(host.activatePage(gsPageId.value()));
    QCOMPARE(probe.gsReads, 1);
    probe.failNextNativeRead = true;
    QVERIFY(host.activatePage(nativePageId.value()));
    QCOMPARE(nativePage->directoryModel()->rowCount(), 2);
    QCOMPARE(probe.warnings.size(), 1);
    QVERIFY(host.activatePage(gsPageId.value()));
    QVERIFY(host.activatePage(nativePageId.value()));
    QCOMPARE(nativePage->directoryModel(), modelIdentity);
    QCOMPARE(nativePage->directoryModel()->rowCount(), 90);
    QCOMPARE(nativePage->directoryModel()->storedRecordCount(), std::size_t(90));
    QVERIFY(nativePage->tableView()->model() == modelIdentity);
    nativeCellWidgets = 0;
    for (int row = 0; row < nativePage->directoryModel()->rowCount(); ++row)
    {
        for (int column = 0; column < nativePage->directoryModel()->columnCount(); ++column)
        {
            nativeCellWidgets += nativePage->tableView()->indexWidget(
                nativePage->directoryModel()->index(row, column)) != nullptr;
        }
    }
    QCOMPARE(nativeCellWidgets, 0);
    QCOMPARE(visibleLineEditors(nativePage->tableView()), 0);
    nativePage->tableView()->edit(nativePage->directoryModel()->index(0, 0));
    QApplication::processEvents();
    QCOMPARE(visibleLineEditors(nativePage->tableView()), 1);
    closeVisibleEditor(nativePage->tableView());
    QApplication::processEvents();
    QCOMPARE(visibleLineEditors(nativePage->tableView()), 0);

    auto* const addButton = nativePage->findChild<QPushButton*>(
        QStringLiteral("staffDirectoryAddButton"));
    QVERIFY(addButton);
    addButton->click();
    QCOMPARE(nativePage->directoryModel()->rowCount(), 91);
    QVERIFY(nativePage->hasUnsavedChanges());
    QVERIFY(nativePage->directoryModel()->setData(
        nativePage->directoryModel()->index(90, 0),
        QStringLiteral("New Native"), Qt::EditRole));
    QVERIFY(nativePage->saveChanges());
    QCOMPARE(probe.nativeSaves, 1);
    QCOMPARE(probe.lastNativeSave.rows.size(), std::size_t(91));
    QVERIFY(!probe.lastNativeSave.rows.back().id.has_value());
    QCOMPARE(probe.lastNativeSave.rows.back().name, std::u16string(u"New Native"));
    QVERIFY(!nativePage->hasUnsavedChanges());
    QCOMPARE(nativePage->directoryModel()->rowCount(), 91);

    probe.confirmDelete = false;
    nativePage->tableView()->selectionModel()->select(
        nativePage->directoryModel()->index(0, 0),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    auto* const deleteButton = nativePage->findChild<QPushButton*>(
        QStringLiteral("staffDirectoryDeleteButton"));
    QVERIFY(deleteButton);
    deleteButton->click();
    QCOMPARE(nativePage->directoryModel()->rowCount(), 91);
    QVERIFY(!nativePage->hasUnsavedChanges());

    // Deletion confirmation and persistence retain the Native ID type and
    // never issue a GS delete request.
    probe.confirmDelete = true;
    nativePage->tableView()->selectionModel()->select(
        nativePage->directoryModel()->index(0, 0),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    deleteButton->click();
    QVERIFY(nativePage->hasUnsavedChanges());
    QVERIFY(nativePage->saveChanges());
    QCOMPARE(probe.nativeSaves, 2);
    QCOMPARE(probe.lastNativeSave.deletedIds.size(), std::size_t(1));
    QCOMPARE(probe.lastNativeSave.deletedIds.front(),
        Domain::NativeEnglishTeacherId(1));
    QCOMPARE(probe.gsSaves, 0);

    // The GS route owns its distinct model shape and ID type.
    QVERIFY(host.activatePage(gsPageId.value()));
    auto* const gsPage = dynamic_cast<Ui::SharedStaffDirectoryPage*>(
        host.pageInstance(gsPageId.value()));
    QVERIFY(gsPage);
    QCOMPARE(gsPage->kind(), Ui::StaffDirectoryKind::GsTeam);
    QCOMPARE(gsPage->directoryModel()->columnCount(), 5);
    QVERIFY(!gsPage->tableView()->isSortingEnabled());
    QCOMPARE(gsPage->directoryModel()->gsRow(0).id,
        std::optional<Domain::GsTeamMemberId>(Domain::GsTeamMemberId(100)));
    QVERIFY(gsPage->directoryModel()->setData(
        gsPage->directoryModel()->index(0, 2),
        QStringLiteral("A custom GS position"), Qt::EditRole));
    QVERIFY(gsPage->saveChanges());
    QCOMPARE(probe.gsSaves, 1);
    QCOMPARE(probe.lastGsSave.rows[0].position, std::u16string(u"A custom GS position"));
    QVERIFY(probe.lastGsSave.rows[0].id.has_value());
    QCOMPARE(*probe.lastGsSave.rows[0].id, Domain::GsTeamMemberId(100));
    QCOMPARE(probe.nativeSaves, 2);

    auto* const gsAddButton = gsPage->findChild<QPushButton*>(
        QStringLiteral("staffDirectoryAddButton"));
    QVERIFY(gsAddButton);
    gsAddButton->click();
    QCOMPARE(gsPage->directoryModel()->rowCount(), 3);
    QVERIFY(gsPage->directoryModel()->setData(
        gsPage->directoryModel()->index(2, 0),
        QStringLiteral("New GS member"), Qt::EditRole));
    const qsizetype warningsBeforeGsFailure = probe.warnings.size();
    probe.failNextGsSave = true;
    QVERIFY(!gsPage->saveChanges());
    QVERIFY(gsPage->hasUnsavedChanges());
    QCOMPARE(probe.lastGsSave.rows.size(), std::size_t(3));
    QVERIFY(!probe.lastGsSave.rows.back().id.has_value());
    QCOMPARE(probe.lastGsSave.rows.back().name, std::u16string(u"New GS member"));
    QCOMPARE(probe.warnings.size(), warningsBeforeGsFailure + 1);
    QVERIFY(probe.warnings.back().contains(QStringLiteral("injected GS Team save failure")));
    QVERIFY(gsPage->saveChanges());
    QVERIFY(!gsPage->hasUnsavedChanges());
    QCOMPARE(probe.gsSaves, 3);
    QCOMPARE(gsPage->directoryModel()->gsRow(2).id,
        std::optional<Domain::GsTeamMemberId>(Domain::GsTeamMemberId(110)));

    auto* const gsDeleteButton = gsPage->findChild<QPushButton*>(
        QStringLiteral("staffDirectoryDeleteButton"));
    QVERIFY(gsDeleteButton);
    gsPage->tableView()->selectionModel()->select(
        gsPage->directoryModel()->index(0, 0),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    probe.confirmDelete = false;
    gsDeleteButton->click();
    QCOMPARE(gsPage->directoryModel()->rowCount(), 3);
    QVERIFY(!gsPage->hasUnsavedChanges());

    probe.confirmDelete = true;
    gsDeleteButton->click();
    QCOMPARE(gsPage->directoryModel()->rowCount(), 2);
    QVERIFY(gsPage->hasUnsavedChanges());
    QVERIFY(gsPage->saveChanges());
    QCOMPARE(probe.gsSaves, 4);
    QCOMPARE(probe.lastGsSave.deletedIds.size(), std::size_t(1));
    QCOMPARE(probe.lastGsSave.deletedIds.front(), Domain::GsTeamMemberId(100));
    QCOMPARE(probe.nativeSaves, 2);
}

void SharedStaffDirectoryTests::saveFailureKeepsDraftForRetry()
{
    auto catalog = Resources::ResourceCatalog::load(catalogPath(), resourceRoot());
    QVERIFY2(catalog.has_value(), catalog ? "" : qPrintable(catalog.error().message));
    Resources::ResourceLoader loader(std::move(*catalog));
    EmptyPreferences preferences;
    Ui::PageHost host(loader, preferences.model);
    DirectoryProbe probe;
    const auto pageId = *Ui::PageId::fromString(
        QStringLiteral("native_english_teachers"));
    QVERIFY(host.registerPage({
        .id = pageId,
        .label = QStringLiteral("Native English Teachers"),
        .factory = [&probe](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>(
                std::make_unique<Ui::SharedStaffDirectoryPage>(
                    context,
                    Ui::StaffDirectoryKind::NativeEnglishTeachers,
                    probe.actions()));
        }
    }));
    QVERIFY(host.activatePage(pageId.value()));
    auto* const page = dynamic_cast<Ui::SharedStaffDirectoryPage*>(
        host.pageInstance(pageId.value()));
    QVERIFY(page);
    QVERIFY(page->directoryModel()->setData(
        page->directoryModel()->index(0, 0),
        QStringLiteral("Discard me"), Qt::EditRole));
    QVERIFY(page->hasUnsavedChanges());
    QVERIFY(page->discardChanges());
    QVERIFY(!page->hasUnsavedChanges());
    QCOMPARE(page->directoryModel()->index(0, 0).data(Qt::EditRole).toString(),
        QStringLiteral("Native A"));
    QVERIFY(page->directoryModel()->setData(
        page->directoryModel()->index(0, 0),
        QStringLiteral("Draft retained"), Qt::EditRole));
    probe.failNextNativeSave = true;
    QVERIFY(!page->saveChanges());
    QVERIFY(page->hasUnsavedChanges());
    QCOMPARE(page->directoryModel()->index(0, 0).data(Qt::EditRole).toString(),
        QStringLiteral("Draft retained"));
    QCOMPARE(probe.warnings.size(), 1);
    QVERIFY(page->saveChanges());
    QVERIFY(!page->hasUnsavedChanges());
    QCOMPARE(probe.nativeSaves, 2);
    QCOMPARE(probe.native[0].name, std::u16string(u"Draft retained"));
}

void SharedStaffDirectoryTests::activeRefreshReadsExternalChangesInPlace()
{
    auto catalog = Resources::ResourceCatalog::load(catalogPath(), resourceRoot());
    QVERIFY2(catalog.has_value(), catalog ? "" : qPrintable(catalog.error().message));
    Resources::ResourceLoader loader(std::move(*catalog));
    EmptyPreferences preferences;
    Ui::PageHost host(loader, preferences.model);
    DirectoryProbe probe;
    const auto pageId = *Ui::PageId::fromString(QStringLiteral("gs_team"));
    QVERIFY(host.registerPage({
        .id = pageId,
        .label = QStringLiteral("GS Team"),
        .factory = [&probe](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>(
                std::make_unique<Ui::SharedStaffDirectoryPage>(
                    context, Ui::StaffDirectoryKind::GsTeam, probe.actions()));
        }
    }));
    QVERIFY(host.activatePage(pageId.value()));
    QCOMPARE(host.activePageKey(), std::optional<QString>(pageId.value()));
    auto* const page = dynamic_cast<Ui::SharedStaffDirectoryPage*>(
        host.pageInstance(pageId.value()));
    QVERIFY(page);
    auto* const modelIdentity = page->directoryModel();
    QCOMPARE(modelIdentity->rowCount(), 2);

    probe.gs = gsRecords(4);
    probe.gs[0].name = u"Updated outside the page";
    QString error;
    QVERIFY2(page->refreshFromStore(&error), qPrintable(error));
    QCOMPARE(probe.gsReads, 2);
    QCOMPARE(page->directoryModel(), modelIdentity);
    QCOMPARE(page->tableView()->model(), modelIdentity);
    QCOMPARE(modelIdentity->rowCount(), 4);
    QCOMPARE(modelIdentity->gsRow(0).name, QStringLiteral("Updated outside the page"));
    QVERIFY(!page->hasUnsavedChanges());
}

void SharedStaffDirectoryTests::dirtyRouteLeaveGuardRejectsAndRetainsDraft()
{
    auto catalog = Resources::ResourceCatalog::load(catalogPath(), resourceRoot());
    QVERIFY2(catalog.has_value(), catalog ? "" : qPrintable(catalog.error().message));
    Resources::ResourceLoader loader(std::move(*catalog));
    EmptyPreferences preferences;
    Ui::ApplicationShell shell(loader, preferences.model);
    DirectoryProbe probe;
    const auto staffPageId = *Ui::PageId::fromString(QStringLiteral("gs_team"));
    const auto destinationPageId = *Ui::PageId::fromString(QStringLiteral("destination"));
    QVERIFY(shell.pageHost().registerPage({
        .id = staffPageId,
        .label = QStringLiteral("GS Team"),
        .factory = [&probe](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>(
                std::make_unique<Ui::SharedStaffDirectoryPage>(
                    context, Ui::StaffDirectoryKind::GsTeam, probe.actions()));
        }
    }));
    QVERIFY(shell.pageHost().registerPage({
        .id = destinationPageId,
        .label = QStringLiteral("Destination"),
        .factory = [](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::make_unique<Ui::Page>(context);
        }
    }));
    QVERIFY(shell.configureNavigation({
            Ui::NavigationNode::page(staffPageId),
            Ui::NavigationNode::page(destinationPageId)
        }, staffPageId, staffPageId,
        Application::WorkspaceLifecycleState::Closed));

    Ui::PageHostActivePageChangesAdapter activePage(shell.pageHost());
    int guardCalls = 0;
    shell.setNavigationGuard([&activePage, &guardCalls](
        const Ui::NavigationGuardRequest& request
        ) {
        Q_UNUSED(request);
        ++guardCalls;
        return activePage.hasUnsavedChanges()
            ? Ui::NavigationGuardDecision::Reject
            : Ui::NavigationGuardDecision::Allow;
    });
    QVERIFY(shell.navigateTo(staffPageId, Ui::NavigationCause::Initial));
    auto* const page = dynamic_cast<Ui::SharedStaffDirectoryPage*>(
        shell.pageHost().pageInstance(staffPageId.value()));
    QVERIFY(page);
    QVERIFY(page->directoryModel()->setData(
        page->directoryModel()->index(0, 0),
        QStringLiteral("Draft before canceled navigation"), Qt::EditRole));
    QVERIFY(page->hasUnsavedChanges());
    QVERIFY(activePage.hasUnsavedChanges());
    const std::vector<Ui::PageId> historyBefore =
        shell.navigationModel()->history();

    QVERIFY(!shell.navigateTo(destinationPageId));
    QCOMPARE(guardCalls, 2);
    QCOMPARE(shell.pageHost().activePageKey(),
        std::optional<QString>(staffPageId.value()));
    QCOMPARE(shell.navigationModel()->currentPageId(),
        std::optional<Ui::PageId>(staffPageId));
    QCOMPARE(shell.navigationModel()->history(), historyBefore);
    QCOMPARE(page->directoryModel()->index(0, 0).data(Qt::EditRole).toString(),
        QStringLiteral("Draft before canceled navigation"));
    QVERIFY(page->hasUnsavedChanges());
    QCOMPARE(probe.gsSaves, 0);
}

QTEST_MAIN(SharedStaffDirectoryTests)
#include "shared_staff_directory_tests.moc"
