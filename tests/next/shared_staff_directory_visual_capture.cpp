#include "core/application_services.h"
#include "core/process_memory_snapshot.h"
#include "data/database/workspace_store.h"
#include "features/teacher/ui/staff_directory_page.h"
#include "next/application/preferences_model.h"
#include "next/platform/workspace_store_gateway.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/page_host.h"
#include "next/ui/shared_staff_directory_page.h"
#include "next/platform/settings_manager_save_mode_preferences_port.h"

#include <QApplication>
#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QPushButton>
#include <QTableView>
#include <QTableWidget>
#include <QStringList>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStyleFactory>
#include <QTemporaryDir>
#include <QTranslator>
#include <QUuid>

#include <functional>
#include <memory>
#include <string>
#include <utility>

using namespace ClassMngr;

namespace
{

constexpr QSize CaptureSize{1024, 768};
constexpr int FixtureRows = 120;

class EmptyPreferences final
    : public Next::Application::PreferencesStateReadPort,
      public Next::Application::PreferencesSnapshotPersistPort
{
public:
    [[nodiscard]] Next::Application::PreferencesStateReadResult read() const override
    {
        return Next::Application::PreferencesStateReadResult::success({});
    }

    [[nodiscard]] Next::Application::PreferencesPersistResult persist(
        const Next::Application::UserPreferencesSnapshot&
        ) const override
    {
        return Next::Application::PreferencesPersistResult::success();
    }

    Next::Application::PreferencesModel model{*this, *this};
};

QString errorText(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

struct ExpectedDirectories final
{
    Next::Application::NativeEnglishTeacherDirectorySnapshot native;
    Next::Application::GsTeamDirectorySnapshot gs;
};

struct SurfaceMetrics final
{
    int rows = 0;
    int columns = 0;
    int tableItemCount = 0;
    int cellWidgetCount = 0;
    int compactRecordCount = -1;
    bool sortingEnabled = false;
    bool repositoryOrderMatched = false;
    QStringList headers;
};

struct CaptureMemorySnapshots final
{
    ProcessMemorySnapshot before;
    ProcessMemorySnapshot rendered;
    ProcessMemorySnapshot released;
};

QStringList expectedHeaders(const Next::Ui::StaffDirectoryKind kind)
{
    if (kind == Next::Ui::StaffDirectoryKind::NativeEnglishTeachers)
    {
        return {
            QCoreApplication::translate("StaffDirectoryPage", "Name"),
            QCoreApplication::translate("StaffDirectoryPage", "Position"),
            QCoreApplication::translate("StaffDirectoryPage", "Phone Number"),
            QCoreApplication::translate("StaffDirectoryPage", "Email"),
            QCoreApplication::translate("StaffDirectoryPage", "Birthday"),
            QCoreApplication::translate("StaffDirectoryPage", "Nationality")
        };
    }
    return {
        QCoreApplication::translate("StaffDirectoryPage", "Name"),
        QCoreApplication::translate("StaffDirectoryPage", "Korean Name"),
        QCoreApplication::translate("StaffDirectoryPage", "Position"),
        QCoreApplication::translate("StaffDirectoryPage", "Phone Number"),
        QCoreApplication::translate("StaffDirectoryPage", "Birthday")
    };
}

QString expectedCell(
    const ExpectedDirectories& expected,
    const Next::Ui::StaffDirectoryKind kind,
    const int row,
    const int column
    )
{
    if (kind == Next::Ui::StaffDirectoryKind::NativeEnglishTeachers)
    {
        const auto& record = expected.native.at(static_cast<std::size_t>(row));
        switch (column)
        {
        case 0: return QString::fromStdU16String(record.name);
        case 1: return QString::fromStdU16String(record.position);
        case 2: return QString::fromStdU16String(record.phoneNumber);
        case 3: return QString::fromStdU16String(record.email);
        case 4: return QString::fromStdU16String(record.birthday);
        case 5: return QString::fromStdU16String(record.nationality);
        default: return {};
        }
    }

    const auto& record = expected.gs.at(static_cast<std::size_t>(row));
    switch (column)
    {
    case 0: return QString::fromStdU16String(record.name);
    case 1: return QString::fromStdU16String(record.koreanName);
    case 2: return QString::fromStdU16String(record.position);
    case 3: return QString::fromStdU16String(record.phoneNumber);
    case 4: return QString::fromStdU16String(record.birthday);
    default: return {};
    }
}

int expectedRows(
    const ExpectedDirectories& expected,
    const Next::Ui::StaffDirectoryKind kind
    )
{
    return static_cast<int>(kind == Next::Ui::StaffDirectoryKind::NativeEnglishTeachers
        ? expected.native.size()
        : expected.gs.size());
}

bool validateSurface(
    QTableView* table,
    const Next::Ui::StaffDirectoryKind kind,
    const ExpectedDirectories& expected,
    const QString& surfaceName,
    SurfaceMetrics* metrics,
    QString* error
    )
{
    if (!table || !table->model() || !metrics)
    {
        if (error)
        {
            *error = QStringLiteral("%1 directory table or model is missing.")
                .arg(surfaceName);
        }
        return false;
    }

    const QAbstractItemModel* const model = table->model();
    const QStringList headers = expectedHeaders(kind);
    metrics->rows = model->rowCount();
    metrics->columns = model->columnCount();
    metrics->sortingEnabled = table->isSortingEnabled();
    metrics->headers.reserve(metrics->columns);
    for (int column = 0; column < metrics->columns; ++column)
    {
        metrics->headers.push_back(
            model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString());
    }

    if (metrics->rows != expectedRows(expected, kind))
    {
        if (error)
        {
            *error = QStringLiteral(
                "%1 directory row count was %2; expected %3 repository rows.")
                .arg(surfaceName).arg(metrics->rows).arg(expectedRows(expected, kind));
        }
        return false;
    }
    if (metrics->columns != headers.size())
    {
        if (error)
        {
            *error = QStringLiteral("%1 directory has %2 columns; expected %3.")
                .arg(surfaceName).arg(metrics->columns).arg(headers.size());
        }
        return false;
    }
    if (metrics->headers != headers)
    {
        if (error)
        {
            *error = QStringLiteral("%1 directory field order does not match the expected headers.")
                .arg(surfaceName);
        }
        return false;
    }
    if (metrics->sortingEnabled)
    {
        if (error)
        {
            *error = QStringLiteral("%1 directory unexpectedly enables table sorting.")
                .arg(surfaceName);
        }
        return false;
    }
    QTableWidget* tableWithItems = qobject_cast<QTableWidget*>(table);
    if (!tableWithItems && table->parentWidget())
    {
        tableWithItems = table->parentWidget()->findChild<QTableWidget*>();
    }
    if (tableWithItems)
    {
        for (int row = 0; row < tableWithItems->rowCount(); ++row)
        {
            for (int column = 0; column < tableWithItems->columnCount(); ++column)
            {
                metrics->tableItemCount += tableWithItems->item(row, column) != nullptr;
            }
        }
        if (error)
        {
            *error = QStringLiteral(
                "%1 directory allocates a QTableWidget with %2 table items.")
                .arg(surfaceName).arg(metrics->tableItemCount);
        }
        return false;
    }

    for (int row = 0; row < metrics->rows; ++row)
    {
        for (int column = 0; column < metrics->columns; ++column)
        {
            const QModelIndex index = model->index(row, column);
            const QString actual = model->data(index, Qt::EditRole).toString();
            const QString wanted = expectedCell(expected, kind, row, column);
            if (actual != wanted)
            {
                if (error)
                {
                    *error = QStringLiteral(
                        "%1 directory row %2, column %3 differs from repository order/data.")
                        .arg(surfaceName).arg(row).arg(column);
                }
                return false;
            }
            metrics->cellWidgetCount += table->indexWidget(index) != nullptr;
        }
    }
    metrics->repositoryOrderMatched = true;
    if (metrics->cellWidgetCount != 0)
    {
        if (error)
        {
            *error = QStringLiteral("%1 directory creates %2 cell widgets.")
                .arg(surfaceName).arg(metrics->cellWidgetCount);
        }
        return false;
    }
    return true;
}

QJsonObject surfaceMetricsJson(const SurfaceMetrics& metrics)
{
    QJsonObject result{
        {QStringLiteral("rows"), metrics.rows},
        {QStringLiteral("columns"), metrics.columns},
        {QStringLiteral("headers"), QJsonArray::fromStringList(metrics.headers)},
        {QStringLiteral("sortingEnabled"), metrics.sortingEnabled},
        {QStringLiteral("repositoryOrderMatched"), metrics.repositoryOrderMatched},
        {QStringLiteral("tableItemCount"), metrics.tableItemCount},
        {QStringLiteral("cellWidgetCount"), metrics.cellWidgetCount}
    };
    if (metrics.compactRecordCount >= 0)
    {
        result.insert(QStringLiteral("compactRecordCount"),
            metrics.compactRecordCount);
    }
    return result;
}

QJsonObject processMemoryJson(const ProcessMemorySnapshot& snapshot)
{
    return {
        {QStringLiteral("available"), snapshot.isAvailable},
        {QStringLiteral("platform"), snapshot.platform},
        {QStringLiteral("capturedAt"), snapshot.capturedAt.toString(Qt::ISODateWithMs)},
        {QStringLiteral("workingSetBytes"), static_cast<double>(snapshot.workingSetBytes)},
        {QStringLiteral("peakWorkingSetBytes"), static_cast<double>(snapshot.peakWorkingSetBytes)},
        {QStringLiteral("privateUsageBytes"), static_cast<double>(snapshot.privateUsageBytes)},
        {QStringLiteral("pagefileUsageBytes"), static_cast<double>(snapshot.pagefileUsageBytes)},
        {QStringLiteral("privateWorkingSetBytes"), static_cast<double>(snapshot.privateWorkingSetBytes)},
        {QStringLiteral("privateDirtyBytes"), static_cast<double>(snapshot.privateDirtyBytes)},
        {QStringLiteral("handleCount"), static_cast<int>(snapshot.handleCount)},
        {QStringLiteral("threadCount"), static_cast<int>(snapshot.threadCount)}
    };
}

QJsonObject captureMemoryJson(const CaptureMemorySnapshots& snapshots)
{
    return {
        {QStringLiteral("beforePage"), processMemoryJson(snapshots.before)},
        {QStringLiteral("whilePageVisible"), processMemoryJson(snapshots.rendered)},
        {QStringLiteral("afterPageRelease"), processMemoryJson(snapshots.released)},
        {QStringLiteral("scopeNotes"), QStringLiteral(
             "Whole-process snapshots include Qt, the fixture workspace, and shared runtime state. "
             "They are not per-page allocation deltas or a memory-budget claim.")}
    };
}

bool createDatabase(const QString& path, QString* error)
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.open(path);
    if (!opened)
    {
        if (error)
        {
            *error = opened.error().diagnostic;
        }
        return false;
    }
    store.close();

    const QString connectionName = QStringLiteral("staff-capture-fixture-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool inserted = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(path);
        if (!database.open())
        {
            if (error)
            {
                *error = database.lastError().text();
            }
        }
        else
        {
            inserted = true;
            QSqlQuery native(database);
            native.prepare(QStringLiteral(
                "INSERT INTO native_english_teachers "
                "(id,name,position,phone_number,birthday,nationality,email) "
                "VALUES (:id,:name,:position,:phone,:birthday,:nationality,:email)"));
            const QStringList nativePositions{
                QStringLiteral("Co-ordinator"), QStringLiteral("Team Leader"),
                QStringLiteral("M3 Song's"), QStringLiteral("M2 Song's"),
                QStringLiteral("M1 Song's"), QStringLiteral("E6 Song's"),
                QStringLiteral("E5 Athena"), QStringLiteral("NET")
            };
            for (int id = 1; id <= FixtureRows; ++id)
            {
                native.bindValue(QStringLiteral(":id"), id);
                native.bindValue(QStringLiteral(":name"),
                    QStringLiteral("Native Staff %1").arg(id, 3, 10, QLatin1Char('0')));
                native.bindValue(QStringLiteral(":position"),
                    nativePositions.at((id - 1) % nativePositions.size()));
                native.bindValue(QStringLiteral(":phone"),
                    QStringLiteral("010-%1-%2").arg(2100 + id, 4, 10, QLatin1Char('0'))
                        .arg(id, 4, 10, QLatin1Char('0')));
                native.bindValue(QStringLiteral(":birthday"),
                    QStringLiteral("%1-%2").arg(1 + id % 12, 2, 10, QLatin1Char('0'))
                        .arg(1 + id % 27, 2, 10, QLatin1Char('0')));
                native.bindValue(QStringLiteral(":nationality"), id % 2
                    ? QStringLiteral("Canada") : QStringLiteral("United States"));
                native.bindValue(QStringLiteral(":email"),
                    QStringLiteral("staff%1@example.test").arg(id));
                if (!native.exec())
                {
                    inserted = false;
                    if (error)
                    {
                        *error = native.lastError().text();
                    }
                    break;
                }
            }

            if (inserted)
            {
                QSqlQuery gs(database);
                gs.prepare(QStringLiteral(
                    "INSERT INTO gs_team "
                    "(id,name,korean_name,position,phone_number,birthday) "
                    "VALUES (:id,:name,:korean,:position,:phone,:birthday)"));
                const QStringList gsPositions{
                    QStringLiteral("Branch Manager"), QStringLiteral("M3"),
                    QStringLiteral("M2"), QStringLiteral("M1"),
                    QStringLiteral("C3"), QStringLiteral("C2"),
                    QStringLiteral("C1")
                };
                for (int id = 1; id <= FixtureRows; ++id)
                {
                    gs.bindValue(QStringLiteral(":id"), id);
                    gs.bindValue(QStringLiteral(":name"),
                        QStringLiteral("GS Staff %1").arg(id, 3, 10, QLatin1Char('0')));
                    gs.bindValue(QStringLiteral(":korean"),
                        QStringLiteral("지나 %1").arg(id, 3, 10, QLatin1Char('0')));
                    gs.bindValue(QStringLiteral(":position"),
                        gsPositions.at((id - 1) % gsPositions.size()));
                    gs.bindValue(QStringLiteral(":phone"),
                        QStringLiteral("010-%1-%2").arg(3100 + id, 4, 10, QLatin1Char('0'))
                            .arg(id, 4, 10, QLatin1Char('0')));
                    gs.bindValue(QStringLiteral(":birthday"),
                        QStringLiteral("%1-%2").arg(1 + id % 12, 2, 10, QLatin1Char('0'))
                            .arg(1 + id % 27, 2, 10, QLatin1Char('0')));
                    if (!gs.exec())
                    {
                        inserted = false;
                        if (error)
                        {
                            *error = gs.lastError().text();
                        }
                        break;
                    }
                }
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return inserted;
}

bool captureLegacy(
    const QString& databasePath,
    const Next::Ui::StaffDirectoryKind kind,
    const QString& output,
    const ExpectedDirectories& expected,
    PlatformProcessMemorySnapshotProvider& memoryProvider,
    SurfaceMetrics* metrics,
    CaptureMemorySnapshots* memory,
    QString* error
    )
{
    ApplicationServices services;
    const RepositoryStatus opened = services.openExistingDatabase(databasePath);
    if (!opened)
    {
        if (error)
        {
            *error = opened.error().diagnostic;
        }
        return false;
    }
    if (!metrics || !memory)
    {
        if (error)
        {
            *error = QStringLiteral("Legacy capture metrics are unavailable.");
        }
        return false;
    }

    memory->before = memoryProvider.snapshot();
    const bool captured = [&] {
        const StaffDirectoryKind legacyKind =
            kind == Next::Ui::StaffDirectoryKind::NativeEnglishTeachers
                ? StaffDirectoryKind::NativeEnglishTeachers
                : StaffDirectoryKind::GsTeam;
        StaffDirectoryPage page(&services, legacyKind);
        page.setSaveMode(SaveMode::Manual);
        if (!page.loadDirectory())
        {
            if (error)
            {
                *error = QStringLiteral("Legacy staff directory did not load.");
            }
            return false;
        }
        auto* const table = page.findChild<QTableView*>(
            kind == Next::Ui::StaffDirectoryKind::NativeEnglishTeachers
                ? QStringLiteral("nativeEnglishTeachersTable")
                : QStringLiteral("gsTeamTable"));
        if (!validateSurface(table, kind, expected,
                QStringLiteral("Legacy"), metrics, error))
        {
            return false;
        }
        page.resize(CaptureSize);
        page.show();
        QApplication::processEvents();
        memory->rendered = memoryProvider.snapshot();
        const QImage image = page.grab().toImage();
        if (image.size() != CaptureSize || !image.save(output, "PNG"))
        {
            if (error)
            {
                *error = QStringLiteral("Could not save legacy directory capture.");
            }
            return false;
        }
        return true;
    }();
    QApplication::processEvents();
    memory->released = memoryProvider.snapshot();
    if (!captured)
    {
        return false;
    }
    return true;
}

bool captureNext(
    Next::Platform::WorkspaceStoreGateway& gateway,
    Next::Resources::ResourceLoader& loader,
    const Next::Ui::StaffDirectoryKind kind,
    const QString& output,
    const ExpectedDirectories& expected,
    PlatformProcessMemorySnapshotProvider& memoryProvider,
    SurfaceMetrics* metrics,
    CaptureMemorySnapshots* memory,
    QString* error
    )
{
    if (!metrics || !memory)
    {
        if (error)
        {
            *error = QStringLiteral("Next capture metrics are unavailable.");
        }
        return false;
    }

    memory->before = memoryProvider.snapshot();
    const bool captured = [&] {
        EmptyPreferences preferences;
        Next::Ui::PageHost host(loader, preferences.model);
        const QString route = kind == Next::Ui::StaffDirectoryKind::NativeEnglishTeachers
            ? QStringLiteral("native_english_teachers")
            : QStringLiteral("gs_team");
        const Next::Ui::PageId id = *Next::Ui::PageId::fromString(route);
        const auto registered = host.registerPage({
            .id = id,
            .label = route,
            .factory = [&gateway, kind](const Next::Ui::PageContext& context)
                -> Next::Ui::PageOperationResult<std::unique_ptr<Next::Ui::Page>> {
                Next::Ui::SharedStaffDirectoryPageActions actions;
                actions.readNativeEnglishTeachers = [&gateway] {
                    return gateway.readNativeEnglishTeacherDirectory();
                };
                actions.saveNativeEnglishTeachers = [&gateway](
                    const Next::Application::NativeEnglishTeacherDirectorySaveRequest& request
                    ) {
                    return gateway.saveNativeEnglishTeacherDirectory(request);
                };
                actions.readGsTeam = [&gateway] {
                    return gateway.readGsTeamDirectory();
                };
                actions.saveGsTeam = [&gateway](
                    const Next::Application::GsTeamDirectorySaveRequest& request
                    ) {
                    return gateway.saveGsTeamDirectory(request);
                };
                actions.saveMode = Next::Application::SaveMode::Manual;
                return std::unique_ptr<Next::Ui::Page>(
                    std::make_unique<Next::Ui::SharedStaffDirectoryPage>(
                        context, kind, std::move(actions)));
            }
        });
        if (!registered)
        {
            if (error)
            {
                *error = registered.error().message;
            }
            return false;
        }
        const auto activated = host.activatePage(route);
        if (!activated)
        {
            if (error)
            {
                *error = activated.error().message;
            }
            return false;
        }
        auto* const page = dynamic_cast<Next::Ui::SharedStaffDirectoryPage*>(
            host.pageInstance(route));
        if (!page)
        {
            if (error)
            {
                *error = QStringLiteral("Next staff directory page is unavailable.");
            }
            return false;
        }
        host.resize(CaptureSize);
        host.show();
        QApplication::processEvents();

        if (!validateSurface(page->tableView(), kind, expected,
                QStringLiteral("Next"), metrics, error))
        {
            return false;
        }
        metrics->compactRecordCount = static_cast<int>(
            page->directoryModel()->storedRecordCount());
        if (metrics->compactRecordCount != expectedRows(expected, kind))
        {
            if (error)
            {
                *error = QStringLiteral("Next compact model does not store one record per row.");
            }
            return false;
        }
        // QTableView with the shared QAbstractTableModel owns one compact
        // record per row and no QTableWidgetItems. Keep both counts separate
        // in the manifest so growth is not reported as cells.
        memory->rendered = memoryProvider.snapshot();
        const QImage image = page->grab().toImage();
        if (image.size() != CaptureSize || !image.save(output, "PNG"))
        {
            if (error)
            {
                *error = QStringLiteral("Could not save Next directory capture.");
            }
            return false;
        }
        return true;
    }();
    QApplication::processEvents();
    memory->released = memoryProvider.snapshot();
    return captured;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    if (application.arguments().size() != 5)
    {
        qCritical("usage: capture <output-dir> <locale> <theme> <native|gs>");
        return 2;
    }
    const QString outputDirectory = application.arguments().at(1);
    const QString locale = application.arguments().at(2);
    const QString theme = application.arguments().at(3);
    const QString kindText = application.arguments().at(4);
    if ((kindText != QStringLiteral("native") && kindText != QStringLiteral("gs"))
        || (theme != QStringLiteral("light") && theme != QStringLiteral("dark")))
    {
        qCritical("invalid capture kind or theme");
        return 2;
    }
    const auto kind = kindText == QStringLiteral("native")
        ? Next::Ui::StaffDirectoryKind::NativeEnglishTeachers
        : Next::Ui::StaffDirectoryKind::GsTeam;
    const bool korean = locale.startsWith(QStringLiteral("ko"));
    QLocale::setDefault(QLocale(locale));
    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    const QString resourceRoot = QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
    const QString generatedRoot = QFileInfo(
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG)).absolutePath();
    QTranslator translator;
    if (!translator.load(QDir(generatedRoot).filePath(
            QStringLiteral("assets/translations/ClassMngr_%1.qm").arg(locale))))
    {
        qCritical().noquote() << "Could not load translation:" << locale;
        return 1;
    }
    application.installTranslator(&translator);

    QString englishFontFamily;
    QString koreanFontFamily;
    for (const auto& font : {
             std::pair{QStringLiteral("Inter.ttc"), &englishFontFamily},
             std::pair{QStringLiteral("PretendardVariable.ttf"), &koreanFontFamily}
         })
    {
        const QString path = QDir(resourceRoot).filePath(
            QStringLiteral("assets/fonts/") + font.first);
        const int id = QFontDatabase::addApplicationFont(path);
        const QStringList families = id < 0
            ? QStringList{}
            : QFontDatabase::applicationFontFamilies(id);
        if (families.empty())
        {
            qCritical().noquote() << "Could not load capture font:" << path;
            return 1;
        }
        *font.second = families.front();
    }
    QFont appFont(korean ? koreanFontFamily : englishFontFamily, 11);
    application.setFont(appFont);
    QFile stylesheet(QDir(resourceRoot).filePath(
        QStringLiteral("assets/styles/%1.qss").arg(theme)));
    if (!stylesheet.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qCritical("Could not load capture theme stylesheet");
        return 1;
    }
    application.setStyleSheet(QString::fromUtf8(stylesheet.readAll()));

    auto catalog = Next::Resources::ResourceCatalog::load(
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG), resourceRoot);
    if (!catalog)
    {
        qCritical().noquote() << catalog.error().message;
        return 1;
    }
    Next::Resources::ResourceLoader loader(std::move(*catalog));

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
    {
        qCritical("Could not create capture workspace");
        return 1;
    }
    const QString databasePath = temporaryDirectory.filePath(
        QStringLiteral("staff-directory.tps"));
    QString error;
    if (!createDatabase(databasePath, &error))
    {
        qCritical().noquote() << "Could not seed the large directory fixture:" << error;
        return 1;
    }

    QDir().mkpath(outputDirectory);
    const QString suffix = QStringLiteral("%1-%2").arg(locale, theme);
    const QString legacyPath = QDir(outputDirectory).filePath(
        QStringLiteral("legacy_%1_%2.png").arg(kindText, suffix));
    const QString nextPath = QDir(outputDirectory).filePath(
        QStringLiteral("next_%1_%2.png").arg(kindText, suffix));
    Next::Platform::WorkspaceStoreGateway gateway;
    const auto opened = gateway.openWorkspace({
        .location = Next::Application::WorkspaceLocation(
            databasePath.toUtf8().toStdString())
    });
    if (!opened)
    {
        qCritical().noquote() << "Could not open Next capture workspace:"
                              << errorText(opened.error().message);
        return 1;
    }
    const auto nativeSnapshot = gateway.readNativeEnglishTeacherDirectory();
    const auto gsSnapshot = gateway.readGsTeamDirectory();
    if (!nativeSnapshot || !gsSnapshot)
    {
        qCritical() << "Could not load expected repository-order snapshots for captures.";
        return 1;
    }
    ExpectedDirectories expected{
        .native = nativeSnapshot.value(),
        .gs = gsSnapshot.value()
    };
    if (expected.native.size() != FixtureRows || expected.gs.size() != FixtureRows)
    {
        qCritical() << "Large capture fixture did not produce 120 rows in each directory.";
        return 1;
    }

    PlatformProcessMemorySnapshotProvider memoryProvider;
    SurfaceMetrics legacyMetrics;
    SurfaceMetrics nextMetrics;
    CaptureMemorySnapshots legacyMemory;
    CaptureMemorySnapshots nextMemory;
    if (!captureLegacy(databasePath, kind, legacyPath, expected,
            memoryProvider, &legacyMetrics, &legacyMemory, &error))
    {
        qCritical().noquote() << error;
        return 1;
    }
    if (!captureNext(gateway, loader, kind, nextPath, expected,
            memoryProvider, &nextMetrics, &nextMemory, &error))
    {
        qCritical().noquote() << error;
        return 1;
    }

    const QJsonObject manifest{
        {QStringLiteral("task"), QStringLiteral("phase7_018_shared_staff_directory_implementation")},
        {QStringLiteral("kind"), kindText},
        {QStringLiteral("locale"), locale},
        {QStringLiteral("theme"), theme},
        {QStringLiteral("viewport"), QJsonArray{CaptureSize.width(), CaptureSize.height()}},
        {QStringLiteral("fixtureRows"), FixtureRows},
        {QStringLiteral("columns"), nextMetrics.columns},
        {QStringLiteral("sortingEnabled"), nextMetrics.sortingEnabled},
        {QStringLiteral("nextModelRecords"), nextMetrics.compactRecordCount},
        {QStringLiteral("nextCellWidgetCount"), nextMetrics.cellWidgetCount},
        {QStringLiteral("repositoryOrderExpectedFrom"), QStringLiteral(
             "WorkspaceStoreGateway directory snapshots")},
        {QStringLiteral("legacySurface"), surfaceMetricsJson(legacyMetrics)},
        {QStringLiteral("nextSurface"), surfaceMetricsJson(nextMetrics)},
        {QStringLiteral("processMemory"), QJsonObject{
             {QStringLiteral("legacy"), captureMemoryJson(legacyMemory)},
             {QStringLiteral("next"), captureMemoryJson(nextMemory)}}},
        {QStringLiteral("legacyCapture"), QFileInfo(legacyPath).fileName()},
        {QStringLiteral("nextCapture"), QFileInfo(nextPath).fileName()}
    };
    const QString manifestPath = QDir(outputDirectory).filePath(
        QStringLiteral("manifest_%1_%2.json").arg(kindText, suffix));
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || manifestFile.write(QJsonDocument(manifest).toJson(
            QJsonDocument::Indented)) < 0)
    {
        qCritical("Could not write visual capture manifest");
        return 1;
    }
    return 0;
}
