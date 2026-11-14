#include "core/fontmanager.h"
#include "core/settingsmanager.h"
#include "data/database/database_schema_manager.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/settings_repository.h"
#include "data/repositories/teacher_repository.h"
#include "fakes/fake_user_prompt_service.h"
#include "data/repositories/testing_class_repository.h"
#include "features/roster/services/roster_template_print_service.h"
#include "next/platform/workspace_store_roster_actions.h"
#include "next/platform/workspace_store_roster_output.h"
#include "next/platform/pdf_print_operation.h"
#include "next/ui/roster_editor_widget.h"
#include "next/ui/roster_print_dialog.h"
#include "next/ui/roster_table_model.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QApplication>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPdfDocument>
#include <QPdfSelection>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStyle>
#include <QSysInfo>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QTranslator>
#include <QUuid>
#include <QtTest>

#include <stdexcept>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>

using namespace ClassMngr::Next;
namespace R = RosterTemplateRenderer;
namespace
{
Domain::ClassId id(int value) { return *Domain::ClassId::fromString(std::to_string(value)); }
class CallbackPdfPrintOperation final : public Platform::PdfPrintOperation
{
public:
    using Handler = std::function<Platform::PdfPrintResult(
        const Platform::ReadyPdfPrintRequest&)>;

    explicit CallbackPdfPrintOperation(Handler handler)
        : m_handler(std::move(handler))
    {
    }

    Platform::PdfPrintResult printReadyDocument(
        const Platform::ReadyPdfPrintRequest& request) override
    {
        return m_handler(request);
    }

    Platform::PdfPrintResult printLocalDocuments(
        const Platform::LocalPdfBatchPrintRequest&) override
    {
        return {Platform::PdfPrintStatus::Failed, QStringLiteral("Unexpected local batch print")};
    }

private:
    Handler m_handler;
};

void installPrintHandler(
    Ui::RosterOutputActions& actions,
    CallbackPdfPrintOperation::Handler handler)
{
    actions.printOperationFactory = [handler = std::move(handler)] {
        return std::make_unique<CallbackPdfPrintOperation>(handler);
    };
}

void installSaveFileSelection(
    Ui::RosterOutputActions& actions,
    const std::optional<QString>& path)
{
    actions.saveFileDialog = [path](
        QWidget*, const Application::SaveFileDialogRequest&) {
        Ui::RosterOutputActions::SaveFileDialogResponse response;
        if (path)
        {
            response.selection = Application::SavedFileDialogSelection{
                Application::FileDialogPath(path->toUtf8().toStdString()),
                false
            };
        }
        return Domain::Result<Ui::RosterOutputActions::SaveFileDialogResponse>::success(
            std::move(response));
    };
}

QString pdfText(const QString& path, int page = -1)
{
    QPdfDocument document;
    if (document.load(path) != QPdfDocument::Error::None) return {};
    QString result;
    for (int index = 0; index < document.pageCount(); ++index)
        if (page < 0 || page == index) result += document.getAllText(index).text();
    return result;
}
int pageCount(const QString& path)
{
    QPdfDocument document;
    return document.load(path) == QPdfDocument::Error::None ? document.pageCount() : -1;
}
bool seed(const QString& path, const QString& prefix = QStringLiteral("FIXTURE"), int classCount = 2)
{
    WorkspaceStore store;
    if (!store.open(path)) return false;
    Teacher teacher; teacher.teacherEn = QStringLiteral("FIXTURE TEACHER"); teacher.teacherKr = QStringLiteral("시험교사");
    teacher.roomNumber = QStringLiteral("FIXTURE ROOM"); teacher.wifiName = QStringLiteral("FIXTURE_WIFI");
    teacher.wifiPassword = QStringLiteral("FIXTURE_WIFI_PASSWORD"); teacher.zoomId = QStringLiteral("FIXTURE_ZOOM"); teacher.zoomPassword = QStringLiteral("FIXTURE_ZOOM_PASSWORD");
    const auto teacherId = store.teacherRepository()->createTeacher(teacher); if (!teacherId) return false;
    for (int index = 0; index < classCount; ++index)
    {
        const auto created = store.classRepository()->createClass(QStringLiteral("%1 CLASS %2").arg(prefix).arg(index, 4, 10, QLatin1Char('0')));
        if (!created) return false;
        ClassInfo info; info.classId = *created; info.teacherId = *teacherId;
        info.classGrade = QStringLiteral("FIXTURE G%1").arg(index); info.classLevel = QStringLiteral("LEVEL");
        info.classTimes = {{QStringLiteral("Monday"), QStringLiteral("%1:00 PM").arg(4 + index % 6), QStringLiteral("4:50 PM")}};
        if (!store.classInfoRepository()->saveClassInfo(info)) return false;
        Roster roster; roster.columns = {QStringLiteral("English"), QStringLiteral("Korean"), QStringLiteral("Winter"),
            QStringLiteral("FIXTURE EXTRA A"), QStringLiteral("FIXTURE EXTRA B"), QStringLiteral("FIXTURE EXTRA C"), QStringLiteral("FIXTURE EXTRA D"),
            QStringLiteral("FIXTURE EXTRA E"), QStringLiteral("FIXTURE EXTRA F"), QStringLiteral("FIXTURE EXTRA G"), QStringLiteral("FIXTURE EXTRA H")};
        roster.rows = {{QStringLiteral("%1 S%2").arg(prefix).arg(index), QStringLiteral("시험명"), QStringLiteral("A"),
            QStringLiteral("FIXTURE A"), QStringLiteral("FIXTURE B"), QStringLiteral("FIXTURE C"), QStringLiteral("FIXTURE D"),
            QStringLiteral("FIXTURE E"), QStringLiteral("FIXTURE F"), QStringLiteral("FIXTURE G"), QStringLiteral("FIXTURE H")}};
        if (!store.rosterRepository()->saveRoster(*created, roster)) return false;
    }
    return true;
}
Ui::RosterOutputActions actions()
{
    Ui::RosterOutputActions result;
    result.readChoices = Platform::rosterOutputChoicesAtPath;
    result.inspectTruncation = Platform::rosterOutputTruncationAtPath;
    result.savePdf = [](const QString& path, const Output::RosterOutputRequest& request, const QString& pdf, const R::Fonts& fonts) {
        return Platform::saveRosterOutputPdfAtPath(path, request, pdf, fonts);
    };
    installSaveFileSelection(result, std::nullopt);
    installPrintHandler(result, [](const Platform::ReadyPdfPrintRequest&) {
        return Platform::PdfPrintResult{
            Platform::PdfPrintStatus::Canceled,
            QStringLiteral("Fixture print was not expected")
        };
    });
    result.pdfPreviewSessionFactory = Platform::makeQtRosterPdfPreviewSession;
    result.fonts = {FontManager::getUiFont(-1), FontManager::getKoreanFont(-1)};
    return result;
}
DialogGeometryPreferencesCallbacks noOpGeometryPreferencesCallbacks()
{
    return {
        [](std::string_view) { return std::string{}; },
        [](std::string_view, std::string_view) {}
    };
}
QPushButton* findButton(QWidget& dialog, const char* name) { return dialog.findChild<QPushButton*>(QString::fromLatin1(name)); }
QByteArray fileSha256(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly)
        ? QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex()
        : QByteArray{};
}
QString representativeStartupSqlPath()
{
    return QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
        QStringLiteral("tests/fixtures/workspaces/representative_startup.sql")
        );
}
bool executeFixtureSql(QSqlDatabase& database, const QString& sqlPath, QString* errorMessage)
{
    QFile sqlFile(sqlPath);
    if (!sqlFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        *errorMessage = sqlFile.errorString();
        return false;
    }
    QSqlQuery query(database);
    const QStringList statements = QString::fromUtf8(sqlFile.readAll())
        .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString& rawStatement : statements)
    {
        const QString statement = rawStatement.trimmed();
        if (!statement.isEmpty() && !query.exec(statement))
        {
            *errorMessage = QStringLiteral("%1: %2")
                .arg(statement.simplified().left(160), query.lastError().text());
            return false;
        }
    }
    return true;
}
bool materializeSyntheticRepresentativeProfile(const QString& profilePath, QString* errorMessage)
{
    const QString connectionName = QStringLiteral("roster-output-seed-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool success = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(profilePath);
        if (!database.open())
        {
            *errorMessage = database.lastError().text();
        }
        else
        {
            const auto schema = DatabaseSchemaManager::createSchema(database);
            if (!schema)
            {
                *errorMessage = schema.error().diagnostic;
            }
            else
            {
                success = executeFixtureSql(database, representativeStartupSqlPath(), errorMessage);
            }
        }
        database.close();
        database = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return success;
}
qsizetype nonWhitePixelCount(const QImage& source)
{
    const QImage image = source.convertToFormat(QImage::Format_RGB32);
    qsizetype count = 0;
    for (int row = 0; row < image.height(); ++row)
    {
        const auto* pixels = reinterpret_cast<const QRgb*>(image.constScanLine(row));
        for (int column = 0; column < image.width(); ++column)
            if (qRed(pixels[column]) < 245 || qGreen(pixels[column]) < 245
                || qBlue(pixels[column]) < 245) ++count;
    }
    return count;
}
struct RejectPreviewOnExit final
{
    QDialog& dialog;
    ~RejectPreviewOnExit() { dialog.reject(); }
};

}
class RosterOutputTests final : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QVERIFY(m_settingsDirectory.isValid());
        qputenv("CLASSMNGR_SETTINGS_ROOT",
            m_settingsDirectory.path().toUtf8());
        SettingsManager::instance().clear();
        SettingsManager::instance().sync();
        FontManager::loadFonts();
    }
    void cleanup()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
        QVERIFY(SettingsManager::instance().writeMaximumRosterRowsPreference(25));
    }
    void sparseNamesAndPositionalProjection();
    void scopesProfilesAndDailyPageOrder();
    void templateParity_data();
    void templateParity();
    void syntheticRepresentativeSchemaV6DbProfileExitEvidenceParity();
    void restrictionsAndAtomicFailure();
    void pageReleaseAndLargeRoute();
    void editorSaveGate();
    void rosterDialogRequiresCompleteGeometryCallbacks();
    void dialogCancelFailureAndPreview();
    void saveFileDialogReceivesRosterPdfRequestAndHandlesFailure();
    void outputWarnsBeforeGeneratingWhenStudentsOverflowTemplate();
    void rosterPreviewSessionOwnsAndRendersGeneratedPdf();
    void printPortReceivesGeneratedPdfAndReportsCancelAndFailure();
    void captures();

private:
    QTemporaryDir m_settingsDirectory;
};

void RosterOutputTests::sparseNamesAndPositionalProjection()
{
    QVERIFY(SettingsManager::instance().writeMaximumRosterRowsPreference(100));
    QTemporaryDir dir; const auto path = dir.filePath(QStringLiteral("sparse.tps")); QVERIFY(seed(path));
    WorkspaceStore store; QVERIFY(store.openExisting(path)); auto db = store.database(); QSqlQuery query(db);
    QVERIFY(query.exec(QStringLiteral("DELETE FROM roster_data WHERE class_id=1")));
    for (int index = 0; index < 26; ++index)
    {
        query.prepare(QStringLiteral("INSERT INTO roster_data(class_id,row_index,col_index,value) VALUES(1,?,?,?)"));
        query.addBindValue(25LL + index * 3LL);
        query.addBindValue(index == 0 ? 1 : 0);
        query.addBindValue(QStringLiteral("SPARSE_%1").arg(index, 2, 10, QLatin1Char('0'))); QVERIFY(query.exec());
    }
    // Unprinted tail cells and unselected fields are deliberately ignored.
    QVERIFY(query.exec(QStringLiteral("INSERT INTO roster_data VALUES(1,2000000000,0,x'010203')")));
    QVERIFY(query.exec(QStringLiteral("INSERT INTO roster_data VALUES(1,0,2,x'010203')")));
    const auto names = store.rosterRepository()->loadTemplatePrintProjection(1, {QStringLiteral("English"), QStringLiteral("Korean")}, false);
    QVERIFY(names); QCOMPARE(names->rows.size(), 25); QCOMPARE(names->rows.first().at(1), QStringLiteral("SPARSE_00"));
    QCOMPARE(names->rows.last().first(), QStringLiteral("SPARSE_24")); QCOMPARE(names->scannedCellCount, 25);
    const auto positional = store.rosterRepository()->loadTemplatePrintProjection(1, {QStringLiteral("English"), QStringLiteral("Korean")}, true);
    QVERIFY(positional); QCOMPARE(positional->rows.size(), 23); QVERIFY(positional->rows.first().first().isEmpty());
    for (auto templateId : {R::TemplateId::ByDay, R::TemplateId::Daily, R::TemplateId::PerClassWithExtraInfo})
    {
        Output::RosterOutputRequest request{id(1)}; request.scope = Output::RosterOutputScope::CurrentClass; request.templateId = templateId;
        const auto pdf = dir.filePath(QStringLiteral("%1.pdf").arg(static_cast<int>(templateId)));
        Output::RosterOutputMetrics metrics;
        const auto saved = Platform::saveRosterOutputPdfAtPath(path, request, pdf, actions().fonts, &metrics);
        QCOMPARE(saved.status, DocumentOutputStatus::Sent);
        const auto text = pdfText(pdf);
        QCOMPARE(text.contains(QStringLiteral("SPARSE_00")), templateId != R::TemplateId::PerClassWithExtraInfo);
        QCOMPARE(text.contains(QStringLiteral("SPARSE_24")), templateId != R::TemplateId::PerClassWithExtraInfo);
        QVERIFY(!text.contains(QStringLiteral("SPARSE_25")));
        QVERIFY(metrics.peakProjectedRows <= 25);
    }
}

void RosterOutputTests::scopesProfilesAndDailyPageOrder()
{
    QTemporaryDir dir; const auto a = dir.filePath(QStringLiteral("a.tps")), b = dir.filePath(QStringLiteral("b.tps"));
    QVERIFY(seed(a, QStringLiteral("FIXTURE_A"), 8)); QVERIFY(seed(b, QStringLiteral("FIXTURE_B"), 2));
    Output::RosterOutputRequest request{id(1)}; request.templateId = R::TemplateId::PerClassWithExtraInfo;
    request.scope = Output::RosterOutputScope::SelectedClasses; request.selectedClassIds = {id(2), id(1), id(2)};
    auto pdf = dir.filePath(QStringLiteral("selected.pdf")); QCOMPARE(Platform::saveRosterOutputPdfAtPath(a, request, pdf).status, DocumentOutputStatus::Sent);
    QCOMPARE(pageCount(pdf), 2); QVERIFY(pdfText(pdf, 0).contains(QStringLiteral("FIXTURE_A S1"))); QVERIFY(pdfText(pdf, 1).contains(QStringLiteral("FIXTURE_A S0")));
    request.scope = Output::RosterOutputScope::CurrentClass; pdf = dir.filePath(QStringLiteral("profileB.pdf"));
    QCOMPARE(Platform::saveRosterOutputPdfAtPath(b, request, pdf).status, DocumentOutputStatus::Sent); QVERIFY(pdfText(pdf).contains(QStringLiteral("FIXTURE_B S0"))); QVERIFY(!pdfText(pdf).contains(QStringLiteral("FIXTURE_A")));
    request.scope = Output::RosterOutputScope::AllClasses; request.templateId = R::TemplateId::Daily;
    pdf = dir.filePath(QStringLiteral("daily.pdf")); QCOMPARE(Platform::saveRosterOutputPdfAtPath(a, request, pdf).status, DocumentOutputStatus::Sent);
    QCOMPARE(pageCount(pdf), 2);
    const auto first = pdfText(pdf, 0), second = pdfText(pdf, 1);
    QVERIFY(first.indexOf(QStringLiteral("FIXTURE_A S0")) < first.indexOf(QStringLiteral("FIXTURE_A S6")));
    QVERIFY(first.indexOf(QStringLiteral("FIXTURE_A S6")) < first.indexOf(QStringLiteral("FIXTURE_A S1")));
    QVERIFY(second.contains(QStringLiteral("FIXTURE_A S4"))); QVERIFY(second.contains(QStringLiteral("FIXTURE_A S5")));
    request.templateId = R::TemplateId::PerClassWithExtraInfo; pdf = dir.filePath(QStringLiteral("all.pdf"));
    QCOMPARE(Platform::saveRosterOutputPdfAtPath(a, request, pdf).status, DocumentOutputStatus::Sent); QCOMPARE(pageCount(pdf), 8);
    for (int index = 0; index < 8; ++index) QVERIFY(pdfText(pdf, index).contains(QStringLiteral("FIXTURE_A S%1").arg(index)));
}

void RosterOutputTests::templateParity_data()
{
    QTest::addColumn<int>("templateId"); QTest::addColumn<bool>("landscape");
    QTest::newRow("By Day") << 0 << true; QTest::newRow("Daily") << 1 << false;
    QTest::newRow("Per Class portrait") << 2 << false; QTest::newRow("Per Class landscape") << 2 << true;
}
void RosterOutputTests::templateParity()
{
    QFETCH(int, templateId); QFETCH(bool, landscape);
    QTemporaryDir dir; const auto path = dir.filePath(QStringLiteral("parity.tps")); QVERIFY(seed(path));
    WorkspaceStore store; QVERIFY(store.openExisting(path));
    QList<RosterTemplatePrintService::RosterClassData> legacy;
    const auto classrooms = store.classRepository()->getClasses(); QVERIFY(classrooms);
    for (const auto& classroom : *classrooms)
    {
        const auto info = store.classInfoRepository()->loadRosterPrintClassInfoRecord(classroom.id); QVERIFY(info);
        const auto roster = store.rosterRepository()->loadRoster(classroom.id); QVERIFY(roster);
        RosterTemplatePrintService::RosterClassData item; item.classroom = classroom; item.roster = *roster;
        item.classInfo = {info->classGrade, info->classLevel, info->teacherEnglishName, info->teacherKoreanName,
            info->roomNumber, info->wifiName, info->wifiPassword, info->zoomId, info->zoomPassword, {}};
        for (const auto& time : info->regularTimes) item.classInfo.regularSchedule.append({time.day, time.startTime, time.endTime});
        legacy.append(item);
    }
    Output::RosterOutputRequest request{id(1)}; request.templateId = static_cast<R::TemplateId>(templateId);
    request.orientation = landscape ? QPageLayout::Landscape : QPageLayout::Portrait;
    request.extraColumns = {QStringLiteral("FIXTURE EXTRA B"), QStringLiteral("FIXTURE EXTRA A")};
    const auto nextPath = dir.filePath(QStringLiteral("next.pdf")), legacyPath = dir.filePath(QStringLiteral("legacy.pdf"));
    const R::Fonts fonts = actions().fonts;
    QCOMPARE(Platform::saveRosterOutputPdfAtPath(path, request, nextPath, fonts).status, DocumentOutputStatus::Sent);
    QCOMPARE(RosterTemplatePrintService::saveRostersPdf(legacy, legacyPath, request.templateId, request.extraColumns, request.orientation).status, DocumentOutputStatus::Sent);
    QPdfDocument next, old; QCOMPARE(next.load(nextPath), QPdfDocument::Error::None); QCOMPARE(old.load(legacyPath), QPdfDocument::Error::None);
    QCOMPARE(next.pageCount(), old.pageCount());
    QString templateSlug;
    switch (templateId)
    {
    case 0: templateSlug = QStringLiteral("by-day"); break;
    case 1: templateSlug = QStringLiteral("daily"); break;
    default: templateSlug = QStringLiteral("per-class"); break;
    }
    const QString caseSlug = templateSlug + QLatin1Char('_')
        + (landscape ? QStringLiteral("landscape") : QStringLiteral("portrait"));
    const QString evidenceRoot = qEnvironmentVariable(
        "CLASSMNGR_ROSTER_OUTPUT_PARITY_DIR").trimmed();
    QString evidenceDirectory;
    if (!evidenceRoot.isEmpty())
    {
        evidenceDirectory = QDir(evidenceRoot).filePath(caseSlug);
        QVERIFY2(QDir().mkpath(evidenceDirectory), qPrintable(evidenceDirectory));
    }
    QJsonArray pageEvidence;
    QStringList nextDocumentText;
    for (int page = 0; page < next.pageCount(); ++page)
    {
        QCOMPARE(next.pagePointSize(page), old.pagePointSize(page));
        const auto size = next.pagePointSize(page); QCOMPARE(size.width() > size.height(), landscape);
        const QString nextText = next.getAllText(page).text();
        const QString legacyText = old.getAllText(page).text();
        QCOMPARE(nextText, legacyText);
        QVERIFY2(nextText.contains(QStringLiteral("FIXTURE")),
            "Roster PDF page is missing synthetic fixture content.");
        nextDocumentText.append(nextText);
        const QImage nextRaster = next.render(page, QSize(1000, 1000));
        const QImage legacyRaster = old.render(page, QSize(1000, 1000));
        QVERIFY(!nextRaster.isNull());
        QVERIFY(!legacyRaster.isNull());
        QCOMPARE(nextRaster.convertToFormat(QImage::Format_RGB32),
            legacyRaster.convertToFormat(QImage::Format_RGB32));
        if (!evidenceDirectory.isEmpty())
        {
            const QString pageSuffix = QStringLiteral("page-%1.png").arg(page + 1);
            const QString nextRasterPath = QDir(evidenceDirectory).filePath(
                QStringLiteral("next-") + pageSuffix);
            const QString legacyRasterPath = QDir(evidenceDirectory).filePath(
                QStringLiteral("legacy-") + pageSuffix);
            QVERIFY(nextRaster.save(nextRasterPath, "PNG"));
            QVERIFY(legacyRaster.save(legacyRasterPath, "PNG"));
            pageEvidence.append(QJsonObject{
                {QStringLiteral("page"), page + 1},
                {QStringLiteral("widthPoints"), size.width()},
                {QStringLiteral("heightPoints"), size.height()},
                {QStringLiteral("renderWidth"), nextRaster.width()},
                {QStringLiteral("renderHeight"), nextRaster.height()},
                {QStringLiteral("textEqual"), true},
                {QStringLiteral("rasterEqual"), true},
                {QStringLiteral("legacyText"), legacyText},
                {QStringLiteral("nextText"), nextText},
                {QStringLiteral("legacyImage"), QFileInfo(legacyRasterPath).fileName()},
                {QStringLiteral("legacyImageSha256"),
                    QString::fromLatin1(fileSha256(legacyRasterPath))},
                {QStringLiteral("nextImage"), QFileInfo(nextRasterPath).fileName()},
                {QStringLiteral("nextImageSha256"),
                    QString::fromLatin1(fileSha256(nextRasterPath))}
            });
        }
    }
    if (templateId == 2) { const auto text = pdfText(nextPath); QVERIFY(text.contains(QStringLiteral("FIXTURE B"))); QVERIFY(!text.contains(QStringLiteral("FIXTURE C"))); QVERIFY(text.contains(QStringLiteral("FIXTURE_WIFI"))); QVERIFY(text.contains(QStringLiteral("FIXTURE_ZOOM"))); }
    if (templateId == 1) QVERIFY(!pdfText(nextPath).contains(QStringLiteral("FIXTURE_WIFI")));
    if (!evidenceDirectory.isEmpty())
    {
        const QString nextEvidencePdf = QDir(evidenceDirectory).filePath(
            QStringLiteral("next.pdf"));
        const QString legacyEvidencePdf = QDir(evidenceDirectory).filePath(
            QStringLiteral("legacy.pdf"));
        QFile::remove(nextEvidencePdf);
        QFile::remove(legacyEvidencePdf);
        QVERIFY(QFile::copy(nextPath, nextEvidencePdf));
        QVERIFY(QFile::copy(legacyPath, legacyEvidencePdf));
        const QByteArray nextText = nextDocumentText.join(QLatin1Char('\n')).toUtf8();
        const QJsonObject evidence{
            {QStringLiteral("case"), caseSlug},
            {QStringLiteral("fixture"), QStringLiteral("Synthetic FIXTURE values only")},
            {QStringLiteral("templateId"), templateId},
            {QStringLiteral("template"), templateSlug},
            {QStringLiteral("orientation"), landscape
                ? QStringLiteral("landscape") : QStringLiteral("portrait")},
            {QStringLiteral("platform"), QJsonObject{
                {QStringLiteral("product"), QSysInfo::prettyProductName()},
                {QStringLiteral("operatingSystem"),
                    QSysInfo::productType() + QStringLiteral(" ")
                        + QSysInfo::productVersion()},
                {QStringLiteral("architecture"), QSysInfo::currentCpuArchitecture()},
                {QStringLiteral("qtVersion"), QString::fromLatin1(qVersion())}
            }},
            {QStringLiteral("fonts"), QJsonObject{
                {QStringLiteral("ui"), fonts.ui.family()},
                {QStringLiteral("korean"), fonts.korean.family()}
            }},
            {QStringLiteral("pageCount"), next.pageCount()},
            {QStringLiteral("pageCountEqual"), true},
            {QStringLiteral("pageTextEqual"), true},
            {QStringLiteral("expectedContentMarkers"),
                QJsonArray{QStringLiteral("FIXTURE")}},
            {QStringLiteral("combinedTextSha256"), QString::fromLatin1(
                QCryptographicHash::hash(nextText, QCryptographicHash::Sha256).toHex())},
            {QStringLiteral("legacyPdf"), QFileInfo(legacyEvidencePdf).fileName()},
            {QStringLiteral("legacyPdfSha256"), QString::fromLatin1(
                fileSha256(legacyEvidencePdf))},
            {QStringLiteral("nextPdf"), QFileInfo(nextEvidencePdf).fileName()},
            {QStringLiteral("nextPdfSha256"), QString::fromLatin1(
                fileSha256(nextEvidencePdf))},
            {QStringLiteral("pages"), pageEvidence},
            {QStringLiteral("physicalPrintPerformed"), false}
        };
        QFile evidenceFile(QDir(evidenceDirectory).filePath(
            QStringLiteral("output-evidence.json")));
        QVERIFY(evidenceFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QVERIFY(evidenceFile.write(QJsonDocument(evidence).toJson(
            QJsonDocument::Indented)) >= 0);
    }
}

void RosterOutputTests::syntheticRepresentativeSchemaV6DbProfileExitEvidenceParity()
{
    // This is deterministic synthetic coverage sourced from
    // representative_startup.sql; it is not a historical user profile.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString profilePath = dir.filePath(QStringLiteral("representative.db"));
    QString errorMessage;
    QVERIFY2(materializeSyntheticRepresentativeProfile(profilePath, &errorMessage),
        qPrintable(errorMessage));
    QVERIFY(QFileInfo(profilePath).isFile());
    QCOMPARE(QFileInfo(profilePath).suffix(), QStringLiteral("db"));
    const QString unsupportedSiblingPath = dir.filePath(QStringLiteral("representative.tps"));
    QVERIFY(!QFileInfo::exists(unsupportedSiblingPath));

    WorkspaceStore store;
    const auto opened = store.openExisting(profilePath);
    if (!opened)
        QFAIL(qPrintable(opened.error().diagnostic));
    QSqlDatabase database = store.database();
    const auto schemaVersion = DatabaseSchemaManager::schemaVersion(database);
    if (!schemaVersion)
        QFAIL(qPrintable(schemaVersion.error().diagnostic));
    QCOMPARE(*schemaVersion, 6);

    QSqlQuery foreignKeyCheck(database);
    QVERIFY(foreignKeyCheck.exec(QStringLiteral("PRAGMA foreign_key_check")));
    QVERIFY(!foreignKeyCheck.next());

    const auto classes = store.classRepository()->getClasses();
    QVERIFY(classes);
    QCOMPARE(classes->size(), 8);
    const auto regularClass = store.classRepository()->getClassById(1);
    QVERIFY(regularClass);
    QCOMPARE(regularClass->name, QStringLiteral("Bluebird E4"));
    const auto intensiveClass = store.classRepository()->getClassById(7);
    QVERIFY(intensiveClass);
    QCOMPARE(intensiveClass->name, QStringLiteral("Cedar Intensive"));

    const auto regularInfo = store.classInfoRepository()->loadClassInfo(1);
    QVERIFY(regularInfo);
    QCOMPARE(regularInfo->teacherId, 1);
    QCOMPARE(regularInfo->classTimes.size(), 2);
    QCOMPARE(regularInfo->classTimes.at(0).day, QStringLiteral("Monday"));
    QCOMPARE(regularInfo->classTimes.at(0).startTime, QStringLiteral("4:00 PM"));
    QCOMPARE(regularInfo->classTimes.at(1).day, QStringLiteral("Thursday"));
    const auto intensiveInfo = store.classInfoRepository()->loadClassInfo(7);
    QVERIFY(intensiveInfo);
    QCOMPARE(intensiveInfo->teacherId, 3);
    QCOMPARE(intensiveInfo->intensiveTimes.size(), 2);
    QCOMPARE(intensiveInfo->intensiveTimes.at(0).day, QStringLiteral("Monday"));
    QCOMPARE(intensiveInfo->intensiveTimes.at(0).startTime, QStringLiteral("9:00 AM"));
    QCOMPARE(intensiveInfo->intensiveTimes.at(1).day, QStringLiteral("Wednesday"));

    const auto dana = store.teacherRepository()->getTeacher(1);
    QVERIFY(dana);
    QCOMPARE(dana->teacherEn, QStringLiteral("Dana Kim"));
    QCOMPARE(dana->teacherKr, QString::fromUtf8("김다은"));
    const auto minseo = store.teacherRepository()->getTeacher(3);
    QVERIFY(minseo);
    QCOMPARE(minseo->teacherEn, QStringLiteral("Minseo Park"));

    const auto roster = store.rosterRepository()->loadRoster(1);
    QVERIFY(roster);
    QCOMPARE(roster->columns, QStringList({QStringLiteral("Student"), QStringLiteral("School"),
        QStringLiteral("Level"), QStringLiteral("Notes")}));
    QCOMPARE(roster->rows.size(), 4);
    QCOMPARE(roster->rows.at(0).size(), 4);
    QCOMPARE(roster->rows.at(0).at(0), QStringLiteral("Avery"));
    QCOMPARE(roster->rows.at(0).at(1), QStringLiteral("River School"));
    QCOMPARE(roster->rows.at(3).at(0), QStringLiteral("Devon"));
    const auto scheduleMode = store.settingsRepository()->loadSetting(
        QStringLiteral("schedule_display_mode"));
    QVERIFY(scheduleMode);
    QCOMPARE(scheduleMode->toString(), QStringLiteral("regular"));
    const auto campus = store.settingsRepository()->loadSetting(QStringLiteral("myInfo/campus"));
    QVERIFY(campus);
    QCOMPARE(campus->toString(), QStringLiteral("Fixture Campus"));

    const auto printInfo = store.classInfoRepository()->loadRosterPrintClassInfoRecord(1);
    QVERIFY(printInfo);
    RosterTemplatePrintService::RosterClassData legacyClass;
    legacyClass.classroom = *regularClass;
    legacyClass.roster = *roster;
    legacyClass.classInfo = {printInfo->classGrade, printInfo->classLevel,
        printInfo->teacherEnglishName, printInfo->teacherKoreanName,
        printInfo->roomNumber, printInfo->wifiName, printInfo->wifiPassword,
        printInfo->zoomId, printInfo->zoomPassword, {}};
    for (const auto& time : printInfo->regularTimes)
        legacyClass.classInfo.regularSchedule.append({time.day, time.startTime, time.endTime});

    Output::RosterOutputRequest request{id(1)};
    request.scope = Output::RosterOutputScope::CurrentClass;
    request.templateId = R::TemplateId::PerClassWithExtraInfo;
    request.orientation = QPageLayout::Landscape;
    request.extraColumns = {QStringLiteral("Student"), QStringLiteral("School"),
        QStringLiteral("Level"), QStringLiteral("Notes")};
    const QString nextPath = dir.filePath(QStringLiteral("next.pdf"));
    const QString legacyPath = dir.filePath(QStringLiteral("legacy.pdf"));
    const auto nextResult = Platform::saveRosterOutputPdfAtPath(
        profilePath, request, nextPath, actions().fonts);
    QCOMPARE(nextResult.status, DocumentOutputStatus::Sent);
    const auto legacyResult = RosterTemplatePrintService::saveRostersPdf(
        {legacyClass}, legacyPath, request.templateId, request.extraColumns, request.orientation);
    QCOMPARE(legacyResult.status, DocumentOutputStatus::Sent);

    QPdfDocument nextDocument;
    QPdfDocument legacyDocument;
    QCOMPARE(nextDocument.load(nextPath), QPdfDocument::Error::None);
    QCOMPARE(legacyDocument.load(legacyPath), QPdfDocument::Error::None);
    QCOMPARE(nextDocument.pageCount(), 1);
    QCOMPARE(nextDocument.pageCount(), legacyDocument.pageCount());
    const QString nextText = nextDocument.getAllText(0).text();
    const QString legacyText = legacyDocument.getAllText(0).text();
    QCOMPARE(nextText, legacyText);
    for (const QString& visibleFixtureValue : {QStringLiteral("E4 Blue"),
             QStringLiteral("Dana Kim"), QStringLiteral("Avery"), QStringLiteral("Devon"),
             QStringLiteral("River School"), QStringLiteral("Fixture-WiFi"),
             QStringLiteral("301")})
        QVERIFY2(nextText.contains(visibleFixtureValue), qPrintable(visibleFixtureValue));
    QCOMPARE(nextDocument.pagePointSize(0), legacyDocument.pagePointSize(0));
    QVERIFY(nextDocument.pagePointSize(0).width() > nextDocument.pagePointSize(0).height());
    const QImage nextPage = nextDocument.render(0, QSize(1000, 1000));
    const QImage legacyPage = legacyDocument.render(0, QSize(1000, 1000));
    QVERIFY(!nextPage.isNull());
    QVERIFY(!legacyPage.isNull());
    QCOMPARE(nextPage.size(), legacyPage.size());
    QVERIFY(nonWhitePixelCount(nextPage) > 10'000);
    QCOMPARE(nextPage.convertToFormat(QImage::Format_RGB32),
        legacyPage.convertToFormat(QImage::Format_RGB32));
    QVERIFY(!QFileInfo::exists(unsupportedSiblingPath));
}

void RosterOutputTests::restrictionsAndAtomicFailure()
{
    QTemporaryDir dir; const auto path = dir.filePath(QStringLiteral("failure.tps")); QVERIFY(seed(path, QStringLiteral("FIXTURE"), 8));
    const auto pdf = dir.filePath(QStringLiteral("unchanged.pdf")); QFile file(pdf); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write("UNCHANGED"), 9); file.close();
    Output::RosterOutputRequest request{id(1)};
    const auto duplicate = Platform::saveRosterOutputPdfAtPath(path, request, pdf); QCOMPARE(duplicate.status, DocumentOutputStatus::Failed); QVERIFY(duplicate.message.contains(QStringLiteral("slot")));
    QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("UNCHANGED")); file.close();
    request.scope = Output::RosterOutputScope::SelectedClasses; request.selectedClassIds = {id(999)};
    QCOMPARE(Platform::saveRosterOutputPdfAtPath(path, request, pdf).status, DocumentOutputStatus::Failed);
    WorkspaceStore store; QVERIFY(store.openExisting(path)); TestingClass testing; testing.name = QStringLiteral("FIXTURE TESTING"); testing.grade = QStringLiteral("M1"); testing.level = QStringLiteral("Major"); testing.room = QStringLiteral("FIXTURE TEST ROOM");
    const auto created = store.testingClassRepository()->createTestingClass(testing); QVERIFY(created);
    request = Output::RosterOutputRequest{id(*created)}; request.testingClass = true;
    QCOMPARE(Platform::saveRosterOutputPdfAtPath(path, request, pdf).status, DocumentOutputStatus::Failed);
    request.scope = Output::RosterOutputScope::CurrentClass; request.templateId = R::TemplateId::PerClassWithExtraInfo;
    QCOMPARE(Platform::saveRosterOutputPdfAtPath(path, request, pdf).status, DocumentOutputStatus::Sent); QCOMPARE(pageCount(pdf), 1);
    request.extraColumns = {"a", "b", "c", "d", "e"}; QCOMPARE(Platform::saveRosterOutputPdfAtPath(path, request, pdf).status, DocumentOutputStatus::Failed);
    request.orientation = QPageLayout::Landscape; QCOMPARE(Platform::saveRosterOutputPdfAtPath(path, request, pdf).status, DocumentOutputStatus::Sent);
    request = Output::RosterOutputRequest{id(1)}; request.scope = Output::RosterOutputScope::CurrentClass;
    request.templateId = R::TemplateId::PerClassWithExtraInfo;
    request.extraColumns = {"FIXTURE EXTRA A", "FIXTURE EXTRA B", "FIXTURE EXTRA C", "FIXTURE EXTRA D", "FIXTURE EXTRA E", "FIXTURE EXTRA F", "FIXTURE EXTRA G", "FIXTURE EXTRA H"};
    QCOMPARE(Platform::saveRosterOutputPdfAtPath(path, request, pdf).status, DocumentOutputStatus::Failed);
    request.orientation = QPageLayout::Landscape;
    QCOMPARE(Platform::saveRosterOutputPdfAtPath(path, request, pdf).status, DocumentOutputStatus::Sent);
    QVERIFY(pdfText(pdf).contains(QStringLiteral("FIXTURE H")));
    QCOMPARE(Platform::saveRosterOutputPdfAtPath(path, request, dir.path()).status, DocumentOutputStatus::Failed);
    auto db = store.database(); QSqlQuery query(db); QVERIFY(query.exec(QStringLiteral("UPDATE roster_data SET value=x'010203' WHERE class_id=2 AND col_index=0")));
    request = Output::RosterOutputRequest{id(1)}; request.templateId = R::TemplateId::PerClassWithExtraInfo;
    const auto failed = dir.filePath(QStringLiteral("failed.pdf")); QCOMPARE(Platform::saveRosterOutputPdfAtPath(path, request, failed).status, DocumentOutputStatus::Failed); QVERIFY(!QFile::exists(failed));
    QVERIFY(query.exec(QStringLiteral("UPDATE roster_data SET value='FIXTURE REPAIRED' WHERE class_id=2 AND col_index=0")));
    QCOMPARE(Platform::saveRosterOutputPdfAtPath(path, request, failed).status, DocumentOutputStatus::Sent);
    request.templateId = R::TemplateId::Daily;
    QVERIFY(query.exec(QStringLiteral("UPDATE class_times SET day=x'010203' WHERE class_id=2")));
    const auto malformedSchedule = dir.filePath(QStringLiteral("malformed-schedule.pdf"));
    QCOMPARE(Platform::saveRosterOutputPdfAtPath(path, request, malformedSchedule).status, DocumentOutputStatus::Failed);
    QVERIFY(!QFile::exists(malformedSchedule));
    QVERIFY(query.exec(QStringLiteral("UPDATE class_times SET day='Monday' WHERE class_id=2")));
    QCOMPARE(Platform::saveRosterOutputPdfAtPath(path, request, malformedSchedule).status, DocumentOutputStatus::Sent);
    QCOMPARE(Platform::saveRosterOutputPdfAtPath(dir.filePath(QStringLiteral("missing.tps")), request, failed).status, DocumentOutputStatus::Failed);
}

void RosterOutputTests::pageReleaseAndLargeRoute()
{
    QTemporaryDir dir; const auto path = dir.filePath(QStringLiteral("large.tps")); QVERIFY(seed(path, QStringLiteral("FIXTURE_LARGE"), 120));
    {
        WorkspaceStore store; QVERIFY(store.openExisting(path)); auto db = store.database(); QVERIFY(db.transaction()); QSqlQuery query(db);
        QVERIFY(query.prepare(QStringLiteral("INSERT INTO roster_data(class_id,row_index,col_index,value) VALUES(?,?,0,?)")));
        for (int classId = 1; classId <= 120; ++classId)
            for (int row = 1; row < 25; ++row)
            {
                query.bindValue(0, classId); query.bindValue(1, row);
                query.bindValue(2, QStringLiteral("FIXTURE %1").arg(row)); QVERIFY(query.exec());
            }
        QVERIFY(db.commit());
    }
    Output::RosterOutputRequest request{id(1)}; request.templateId = R::TemplateId::Daily;
    Output::RosterOutputMetrics metrics; const auto pdf = dir.filePath(QStringLiteral("large.pdf"));
    QCOMPARE(Platform::saveRosterOutputPdfAtPath(path, request, pdf, actions().fonts, &metrics).status, DocumentOutputStatus::Sent);
    QCOMPARE(metrics.pagesReleased, 20); QCOMPARE(metrics.peakProjectedClasses, 6); QCOMPARE(metrics.peakProjectedRows, 150); QCOMPARE(pageCount(pdf), 20);
    // The renderer calls release after the producer's returned page values
    // and paint hash are destroyed, before requesting another page.
    int produced = 0, released = 0;
    QCOMPARE(R::savePagesPdf(dir.filePath(QStringLiteral("lifetime.pdf")), R::TemplateId::Daily, QPageLayout::Portrait,
        [&]() -> R::PageResult { if (produced != released) return std::unexpected(QStringLiteral("Page was retained")); if (produced == 3) return std::optional<R::Page>{}; ++produced; return std::optional<R::Page>(R::Page{QStringLiteral("Monday"), {{QStringLiteral("Monday"), 3, 1, QStringLiteral("FIXTURE PAGE")}}}); },
        actions().fonts, [&](qsizetype count) { QCOMPARE(count, 1); ++released; }).status, DocumentOutputStatus::Sent);
    QCOMPARE(released, 3);
    qInfo() << "C37 large route classes=120 pages=" << metrics.pagesReleased << "peak classes=" << metrics.peakProjectedClasses << "peak rows=" << metrics.peakProjectedRows << "peak cells=" << metrics.peakPageCells;
}

void RosterOutputTests::editorSaveGate()
{
    QTemporaryDir dir; const auto path = dir.filePath(QStringLiteral("gate.tps")); QVERIFY(seed(path));
    Ui::RosterEditorActions editorActions; editorActions.saveMode = Application::SaveMode::Manual;
    editorActions.readRoster = Platform::readRosterWindowAtPath;
    bool fail = true; int saves = 0, reads = 0;
    editorActions.saveRoster = [&](const QString& p, const Application::RosterSaveRequest& r) { ++saves; return fail ? Domain::Result<void>::failure({Domain::ErrorCode::Technical, "injected", true}) : Platform::saveRosterAtPath(p, r); };
    editorActions.output = actions(); editorActions.output.readChoices = [&](const QString& p, const Domain::ClassId& selected, bool testing) {
        ++reads; return Platform::rosterOutputChoicesAtPath(p, selected, testing);
    };
    Ui::RosterEditorWidget editor(editorActions); editor.resize(1200, 700); editor.enterClassSection(path, id(1)); editor.show();
    editor.tableView()->setCurrentIndex(editor.model()->index(0, 0)); editor.tableView()->edit(editor.model()->index(0, 0)); QCoreApplication::processEvents();
    auto* input = editor.tableView()->findChild<QLineEdit*>(); QVERIFY(input); input->setText(QStringLiteral("FIXTURE SAVED EDIT"));
    editor.openRosterOutput(); QCOMPARE(saves, 1); QCOMPARE(reads, 0); QVERIFY(editor.hasUnsavedChanges());
    fail = false;
    QTimer::singleShot(0, [&] { if (auto* dialog = editor.findChild<QDialog*>(QStringLiteral("nextRosterPrintDialog"))) dialog->reject(); });
    editor.openRosterOutput(); QCOMPARE(saves, 2); QCOMPARE(reads, 1); QVERIFY(!editor.hasUnsavedChanges());
    QVERIFY(!SettingsManager::instance()
        .get(QStringLiteral("ui/dialogs/nextRosterPrint/geometry"))
        .toByteArray()
        .isEmpty());
    const auto loaded = Platform::readRosterWindowAtPath(path, id(1)); QVERIFY(loaded); QCOMPARE(loaded.value().roster.rows[0][0], std::u16string(u"Fixture Saved Edit"));
}

void RosterOutputTests::rosterDialogRequiresCompleteGeometryCallbacks()
{
    const Output::RosterOutputChoices choices;
    const Ui::RosterOutputActions outputActions;
    const auto construct = [&](DialogGeometryPreferencesCallbacks callbacks)
    {
        Ui::RosterPrintDialog dialog(
            QString(),
            id(1),
            false,
            choices,
            outputActions,
            std::move(callbacks)
            );
        static_cast<void>(dialog);
    };

    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        construct(DialogGeometryPreferencesCallbacks{})
        );

    DialogGeometryPreferencesCallbacks readOnly;
    readOnly.read = [](std::string_view) { return std::string{}; };
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        construct(std::move(readOnly))
        );

    DialogGeometryPreferencesCallbacks writeOnly;
    writeOnly.write = [](std::string_view, std::string_view) {};
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        construct(std::move(writeOnly))
        );
}

void RosterOutputTests::dialogCancelFailureAndPreview()
{
    QTemporaryDir dir; const auto path = dir.filePath(QStringLiteral("dialog.tps")); QVERIFY(seed(path));
    auto source = actions(); int outputs = 0, prints = 0;
    QString lastPrintPath;
    bool previewPdfAliveWhileModal = false;
    bool previewSaveMatchedSessionPdf = false;
    int saveDialogCalls = 0;
    QStringList generatedPaths;
    const auto generate = source.savePdf;
    source.savePdf = [&](const QString& p, const Output::RosterOutputRequest& r, const QString& out, const R::Fonts& fonts) {
        ++outputs;
        generatedPaths.append(out);
        return generate(p, r, out, fonts);
    };
    source.saveFileDialog = [&](
        QWidget*, const Application::SaveFileDialogRequest&) {
        Ui::RosterOutputActions::SaveFileDialogResponse response;
        if (++saveDialogCalls == 2)
        {
            response.selection = Application::SavedFileDialogSelection{
                Application::FileDialogPath(
                    dir.filePath(QStringLiteral("preview-saved.pdf"))
                        .toUtf8().toStdString()),
                false
            };
        }
        return Domain::Result<Ui::RosterOutputActions::SaveFileDialogResponse>::success(
            std::move(response));
    };
    installPrintHandler(source, [&](const Platform::ReadyPdfPrintRequest& request) {
        ++prints;
        lastPrintPath = request.documentPath;
        return Platform::PdfPrintResult{Platform::PdfPrintStatus::Canceled, {}};
    });
    const auto choices = source.readChoices(path, id(1), false); QVERIFY(choices);
    Ui::RosterPrintDialog dialog(path, id(1), false, *choices, source,
        noOpGeometryPreferencesCallbacks());
    findButton(dialog, "nextRosterSavePdf")->click(); QCOMPARE(outputs, 0);
    findButton(dialog, "nextRosterPrint")->click(); QCOMPARE(outputs, 1); QCOMPARE(prints, 1);
    QVERIFY(!QFileInfo::exists(lastPrintPath));
    QTimer::singleShot(0, [&] {
        auto* preview = dialog.findChild<QDialog*>(QStringLiteral("nextRosterPreviewDialog")); QVERIFY(preview);
        QVERIFY(!preview->findChild<QLabel*>(QStringLiteral("nextRosterPreviewPage"))->pixmap().isNull());
        findButton(*preview, "nextRosterPreviewSavePdf")->click();
        previewSaveMatchedSessionPdf =
            generatedPaths.size() == 2
            && QFileInfo::exists(generatedPaths.at(1))
            && fileSha256(generatedPaths.at(1))
                == fileSha256(dir.filePath(QStringLiteral("preview-saved.pdf")));
        findButton(*preview, "nextRosterPreviewPrint")->click();
        previewPdfAliveWhileModal = QFileInfo::exists(lastPrintPath);
        preview->reject();
    });
    findButton(dialog, "nextRosterPreview")->click(); QCOMPARE(outputs, 2); QCOMPARE(prints, 2);
    QVERIFY(previewSaveMatchedSessionPdf);
    QCOMPARE(pageCount(dir.filePath(QStringLiteral("preview-saved.pdf"))), 1);
    QVERIFY(previewPdfAliveWhileModal);
    QVERIFY(!QFileInfo::exists(lastPrintPath));
    installSaveFileSelection(source, dir.filePath(QStringLiteral("saved.pdf")));
    Ui::RosterPrintDialog saved(path, id(1), false, *choices, source,
        noOpGeometryPreferencesCallbacks());
    const int printsBeforeSave = prints;
    findButton(saved, "nextRosterSavePdf")->click(); QCOMPARE(pageCount(dir.filePath(QStringLiteral("saved.pdf"))), 1);
    QCOMPARE(prints, printsBeforeSave);
    installPrintHandler(source, [](const Platform::ReadyPdfPrintRequest&) {
        return Platform::PdfPrintResult{
            Platform::PdfPrintStatus::Failed,
            QStringLiteral("FIXTURE PRINT FAILURE")
        };
    });
    Ui::RosterPrintDialog printFailure(path, id(1), false, *choices, source,
        noOpGeometryPreferencesCallbacks());
    findButton(printFailure, "nextRosterPrint")->click();
    QCOMPARE(printFailure.findChild<QLabel*>(QStringLiteral("nextRosterOutputStatus"))->text(), QStringLiteral("FIXTURE PRINT FAILURE"));
    source.savePdf = [](const QString&, const Output::RosterOutputRequest&, const QString&, const R::Fonts&) { return DocumentOutputResult{DocumentOutputStatus::Failed, QStringLiteral("FIXTURE READ FAILURE")}; };
    installSaveFileSelection(source, dir.filePath(QStringLiteral("failed.pdf")));
    Ui::RosterPrintDialog failure(path, id(1), false, *choices, source,
        noOpGeometryPreferencesCallbacks());
    findButton(failure, "nextRosterSavePdf")->click(); QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("failed.pdf"))));
    QCOMPARE(failure.findChild<QLabel*>(QStringLiteral("nextRosterOutputStatus"))->text(), QStringLiteral("FIXTURE READ FAILURE"));
    findButton(failure, "nextRosterPreview")->click();
    QVERIFY(!failure.findChild<QDialog*>(QStringLiteral("nextRosterPreviewDialog")));
    Ui::RosterPrintDialog testing(path, id(1), true, *choices, source,
        noOpGeometryPreferencesCallbacks());
    QCOMPARE(testing.findChild<QComboBox*>(QStringLiteral("nextRosterTemplate"))->count(), 1);
    QCOMPARE(testing.request().templateId, R::TemplateId::PerClassWithExtraInfo); QCOMPARE(testing.request().scope, Output::RosterOutputScope::CurrentClass);
}

void RosterOutputTests::saveFileDialogReceivesRosterPdfRequestAndHandlesFailure()
{
    QTemporaryDir dir;
    const QString databasePath = dir.filePath(QStringLiteral("save-dialog.tps"));
    QVERIFY(seed(databasePath));
    auto source = actions();
    const auto choices = source.readChoices(databasePath, id(1), false);
    QVERIFY(choices);

    Application::SaveFileDialogRequest capturedRequest;
    int saveCalls = 0;
    int printCalls = 0;
    QString generatedPath;
    source.saveFileDialog = [&capturedRequest](
        QWidget*, const Application::SaveFileDialogRequest& request) {
        capturedRequest = request;
        Ui::RosterOutputActions::SaveFileDialogResponse response;
        response.selection = Application::SavedFileDialogSelection{
            Application::FileDialogPath("selected-roster-output"),
            false
        };
        return Domain::Result<Ui::RosterOutputActions::SaveFileDialogResponse>::success(
            std::move(response));
    };
    source.savePdf = [&](const QString&, const Output::RosterOutputRequest&,
                         const QString& outputPath, const R::Fonts&) {
        ++saveCalls;
        generatedPath = outputPath;
        return DocumentOutputResult{
            DocumentOutputStatus::Sent,
            QStringLiteral("FIXTURE SAVE SENT")
        };
    };
    installPrintHandler(source, [&](const Platform::ReadyPdfPrintRequest&) {
        ++printCalls;
        return Platform::PdfPrintResult{Platform::PdfPrintStatus::Sent, {}};
    });

    Ui::RosterPrintDialog accepted(databasePath, id(1), false, *choices, source,
        noOpGeometryPreferencesCallbacks());
    findButton(accepted, "nextRosterSavePdf")->click();
    QCOMPARE(capturedRequest.purpose, Application::FileDialogPurpose::GeneratedPdf);
    QCOMPARE(QString::fromStdString(capturedRequest.title),
        QStringLiteral("Save Rosters as PDF"));
    QCOMPARE(QString::fromStdString(capturedRequest.suggestedFileName),
        QStringLiteral("Rosters.pdf"));
    QCOMPARE(capturedRequest.nameFilters.size(), std::size_t(1));
    QCOMPARE(QString::fromStdString(capturedRequest.nameFilters.front()),
        QStringLiteral("PDF Files (*.pdf)"));
    QCOMPARE(QString::fromStdString(capturedRequest.defaultSuffix),
        QStringLiteral("pdf"));
    QCOMPARE(generatedPath, QStringLiteral("selected-roster-output.pdf"));
    QCOMPARE(saveCalls, 1);
    QCOMPARE(printCalls, 0);

    auto canceledSource = actions();
    int canceledSaveCalls = 0;
    installSaveFileSelection(canceledSource, std::nullopt);
    canceledSource.savePdf = [&](const QString&, const Output::RosterOutputRequest&,
                                 const QString&, const R::Fonts&) {
        ++canceledSaveCalls;
        return DocumentOutputResult{DocumentOutputStatus::Sent, {}};
    };
    Ui::RosterPrintDialog canceled(databasePath, id(1), false, *choices,
        canceledSource, noOpGeometryPreferencesCallbacks());
    findButton(canceled, "nextRosterSavePdf")->click();
    QCOMPARE(canceledSaveCalls, 0);
    QVERIFY(canceled.findChild<QLabel*>(QStringLiteral("nextRosterOutputStatus"))
        ->text().isEmpty());

    auto failedSource = actions();
    failedSource.saveFileDialog = [](
        QWidget*, const Application::SaveFileDialogRequest&) {
        return Domain::Result<Ui::RosterOutputActions::SaveFileDialogResponse>::failure({
            Domain::ErrorCode::Technical,
            "FIXTURE CHOOSER FAILURE",
            true
        });
    };
    int failedSaveCalls = 0;
    failedSource.savePdf = [&](const QString&, const Output::RosterOutputRequest&,
                               const QString&, const R::Fonts&) {
        ++failedSaveCalls;
        return DocumentOutputResult{DocumentOutputStatus::Sent, {}};
    };
    Ui::RosterPrintDialog failed(databasePath, id(1), false, *choices,
        failedSource, noOpGeometryPreferencesCallbacks());
    findButton(failed, "nextRosterSavePdf")->click();
    QCOMPARE(failedSaveCalls, 0);
    QCOMPARE(failed.findChild<QLabel*>(QStringLiteral("nextRosterOutputStatus"))
        ->text(), QStringLiteral("FIXTURE CHOOSER FAILURE"));
}

void RosterOutputTests::outputWarnsBeforeGeneratingWhenStudentsOverflowTemplate()
{
    using namespace RosterTemplatePrintTruncation;
    const auto compactSummary = summarize(R::TemplateId::ByDay, {{26, 0}});
    QCOMPARE(compactSummary.capacityPerClass, 25);
    QCOMPARE(compactSummary.totalStudents, 26);
    QCOMPARE(compactSummary.includedStudents, 25);
    QCOMPARE(compactSummary.omittedStudents(), 1);
    QVERIFY(compactSummary.requiresConfirmation());
    const auto positionalSummary = summarize(
        R::TemplateId::PerClassWithExtraInfo, {{30, 22}});
    QCOMPARE(positionalSummary.capacityPerClass, 23);
    QCOMPARE(positionalSummary.includedStudents, 22);
    QCOMPARE(positionalSummary.omittedStudents(), 8);

    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("overflow.tps"));
    QVERIFY(seed(path));
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        Roster roster;
        roster.columns = Roster::BaseColumns;
        roster.columnWidths.fill(120, roster.columns.size());
        for (int row = 0; row < 30; ++row)
        {
            roster.rows.append(
                QStringList(roster.columns.size(), QString{}));
        }
        for (int index = 0; index < 25; ++index)
        {
            roster.rows[index][0] =
                QStringLiteral("OVERFLOW STUDENT %1").arg(index + 1);
            roster.rows[index][1] = QStringLiteral("\uAE40\uBBFC\uC9C0");
        }
        roster.rows[29][0] = QStringLiteral("ROW 30 TAIL STUDENT");
        roster.rows[29][1] = QStringLiteral("\uAE40\uBBFC\uC9C0");
        QVERIFY(store.rosterRepository()->saveRoster(1, roster));
    }

    Output::RosterOutputRequest request{id(1)};
    request.scope = Output::RosterOutputScope::CurrentClass;
    request.templateId = R::TemplateId::ByDay;
    QVERIFY(SettingsManager::instance().writeMaximumRosterRowsPreference(25));
    const auto max25Summary = Platform::rosterOutputTruncationAtPath(
        path, request);
    QVERIFY(max25Summary);
    QCOMPARE(max25Summary->totalStudents, 25);
    QCOMPARE(max25Summary->includedStudents, 25);
    QVERIFY(!max25Summary->requiresConfirmation());
    const QString max25Pdf = directory.filePath(QStringLiteral("max25.pdf"));
    QCOMPARE(Platform::saveRosterOutputPdfAtPath(
                 path, request, max25Pdf, actions().fonts).status,
        DocumentOutputStatus::Sent);
    QVERIFY(!pdfText(max25Pdf).contains(QStringLiteral("ROW 30 TAIL")));

    QVERIFY(SettingsManager::instance().writeMaximumRosterRowsPreference(100));
    const auto max100Summary = Platform::rosterOutputTruncationAtPath(
        path, request);
    QVERIFY(max100Summary);
    QCOMPARE(max100Summary->totalStudents, 26);
    QCOMPARE(max100Summary->includedStudents, 25);
    QCOMPARE(max100Summary->omittedStudents(), 1);
    QVERIFY(max100Summary->requiresConfirmation());
    const QString max100Pdf = directory.filePath(QStringLiteral("max100.pdf"));
    QCOMPARE(Platform::saveRosterOutputPdfAtPath(
                 path, request, max100Pdf, actions().fonts).status,
        DocumentOutputStatus::Sent);
    QVERIFY(!pdfText(max100Pdf).contains(QStringLiteral("ROW 30 TAIL")));

    auto source = actions();
    int outputs = 0;
    source.savePdf = [&](const QString&, const Output::RosterOutputRequest&,
                         const QString&, const R::Fonts&)
    {
        ++outputs;
        return DocumentOutputResult{DocumentOutputStatus::Sent, {}};
    };
    installPrintHandler(source, [](const Platform::ReadyPdfPrintRequest&) {
        return Platform::PdfPrintResult{Platform::PdfPrintStatus::Canceled, {}};
    });
    const auto choices = source.readChoices(path, id(1), false);
    QVERIFY(choices);

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Rejected);
    prompts.scriptedChoices.enqueue(PromptChoice::Accepted);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    Ui::RosterPrintDialog dialog(path, id(1), false, *choices, source,
        noOpGeometryPreferencesCallbacks());
    auto* const scope = dialog.findChild<QComboBox*>(
        QStringLiteral("nextRosterScope"));
    QVERIFY(scope);
    const int currentClassIndex = scope->findData(
        static_cast<int>(Output::RosterOutputScope::CurrentClass));
    QVERIFY(currentClassIndex >= 0);
    scope->setCurrentIndex(currentClassIndex);

    QVERIFY(SettingsManager::instance().writeMaximumRosterRowsPreference(25));
    findButton(dialog, "nextRosterPrint")->click();
    QCOMPARE(prompts.confirmations.size(), 0);
    QCOMPARE(outputs, 1);

    QVERIFY(SettingsManager::instance().writeMaximumRosterRowsPreference(100));
    findButton(dialog, "nextRosterPrint")->click();
    QCOMPARE(prompts.confirmations.size(), 1);
    QVERIFY(prompts.confirmations.front().message.contains(
        QStringLiteral("25 of 26")));
    QVERIFY(prompts.confirmations.front().message.contains(
        QStringLiteral("1 students")));
    QCOMPARE(outputs, 1);

    findButton(dialog, "nextRosterPrint")->click();
    QCOMPARE(prompts.confirmations.size(), 2);
    QCOMPARE(outputs, 2);
}

void RosterOutputTests::printPortReceivesGeneratedPdfAndReportsCancelAndFailure()
{
    QTemporaryDir dir;
    const auto path = dir.filePath(QStringLiteral("print-port.tps"));
    QVERIFY(seed(path));

    auto source = actions();
    const auto generate = source.savePdf;
    QStringList generatedPaths;
    QStringList submittedPaths;
    QStringList submittedTexts;
    QStringList submittedTitles;
    QStringList temporaryDirectories;
    QList<int> submittedPageCounts;
    QList<QPageLayout::Orientation> submittedOrientations;
    QList<bool> submittedPdfReadable;
    QList<bool> submittedOptionsPreserved;
    int outputCount = 0;
    int printCount = 0;
    DocumentOutputResult printResult{
        DocumentOutputStatus::Canceled,
        QStringLiteral("FIXTURE PRINT CANCELED")
    };

    source.savePdf = [&](const QString& databasePath,
                         const Output::RosterOutputRequest& request,
                         const QString& outputPath,
                         const R::Fonts& fonts) {
        ++outputCount;
        generatedPaths.append(outputPath);
        return generate(databasePath, request, outputPath, fonts);
    };
    installPrintHandler(source, [&](const Platform::ReadyPdfPrintRequest& request) {
        ++printCount;
        submittedPaths.append(request.documentPath);
        submittedOrientations.append(request.pageOrientation);
        submittedTitles.append(request.dialogTitle);
        temporaryDirectories.append(QFileInfo(request.documentPath).absolutePath());

        QPdfDocument* document = request.document;
        const bool readable = document
            && QFileInfo(request.documentPath).isFile()
            && document->status() == QPdfDocument::Status::Ready
            && document->pageCount() > 0;
        submittedPdfReadable.append(readable);
        submittedPageCounts.append(readable ? document->pageCount() : -1);
        submittedTexts.append(
            readable ? document->getAllText(0).text() : QString{}
            );
        submittedOptionsPreserved.append(
            request.currentPageIndex == 0
            && request.dialogTitle == QStringLiteral("Print Rosters")
            && !request.fitToPageByDefault
            && request.preferredPageSize == QPageSize::A4
            && request.lockPreferredPageSize
            && request.uiFont == source.fonts.ui
            && static_cast<bool>(request.dialogGeometryPreferences.read)
            && static_cast<bool>(request.dialogGeometryPreferences.write)
            );
        return Platform::PdfPrintResult{
            printResult.status == DocumentOutputStatus::Canceled
                ? Platform::PdfPrintStatus::Canceled
                : printResult.status == DocumentOutputStatus::Sent
                    ? Platform::PdfPrintStatus::Sent
                    : Platform::PdfPrintStatus::Failed,
            printResult.message
        };
    });

    const auto choices = source.readChoices(path, id(1), false);
    QVERIFY(choices);

    Ui::RosterPrintDialog canceled(path, id(1), false, *choices, source,
        noOpGeometryPreferencesCallbacks());
    findButton(canceled, "nextRosterPrint")->click();
    QCOMPARE(outputCount, 1);
    QCOMPARE(printCount, 1);
    QCOMPARE(submittedPaths, generatedPaths);
    QCOMPARE(submittedPdfReadable, QList<bool>({true}));
    QCOMPARE(submittedPageCounts, QList<int>({1}));
    QCOMPARE(submittedOrientations,
             QList<QPageLayout::Orientation>({QPageLayout::Landscape}));
    QCOMPARE(submittedTitles, QStringList({QStringLiteral("Print Rosters")}));
    QCOMPARE(submittedOptionsPreserved, QList<bool>({true}));
    QVERIFY(submittedTexts.front().contains(QStringLiteral("FIXTURE S0")));
    QVERIFY(submittedTexts.front().contains(QStringLiteral("FIXTURE S1")));
    QVERIFY(canceled.findChild<QLabel*>(
        QStringLiteral("nextRosterOutputStatus"))->text()
        != QStringLiteral("FIXTURE PRINT CANCELED"));
    QVERIFY(!QFileInfo::exists(submittedPaths.front()));
    QVERIFY(!QFileInfo::exists(temporaryDirectories.front()));

    printResult = {
        DocumentOutputStatus::Failed,
        QStringLiteral("FIXTURE PRINT FAILURE")
    };
    Ui::RosterPrintDialog failed(path, id(1), false, *choices, source,
        noOpGeometryPreferencesCallbacks());
    findButton(failed, "nextRosterPrint")->click();
    QCOMPARE(outputCount, 2);
    QCOMPARE(printCount, 2);
    QCOMPARE(submittedPaths.size(), 2);
    QCOMPARE(submittedPaths.at(1), generatedPaths.at(1));
    QCOMPARE(submittedPdfReadable, QList<bool>({true, true}));
    QCOMPARE(submittedPageCounts, QList<int>({1, 1}));
    QCOMPARE(submittedOptionsPreserved, QList<bool>({true, true}));
    QVERIFY(!QFileInfo::exists(submittedPaths.at(1)));
    QVERIFY(!QFileInfo::exists(temporaryDirectories.at(1)));
    QCOMPARE(failed.findChild<QLabel*>(
        QStringLiteral("nextRosterOutputStatus"))->text(),
        QStringLiteral("FIXTURE PRINT FAILURE"));

    auto invalidPdfSource = actions();
    invalidPdfSource.savePdf = [](
        const QString&, const Output::RosterOutputRequest&,
        const QString& outputPath, const R::Fonts&) {
        QFile invalidPdf(outputPath);
        if (!invalidPdf.open(QIODevice::WriteOnly)
            || invalidPdf.write("not a PDF") != 9)
        {
            return DocumentOutputResult{
                DocumentOutputStatus::Failed,
                QStringLiteral("FIXTURE INVALID PDF WRITE FAILURE")
            };
        }
        return DocumentOutputResult{DocumentOutputStatus::Sent, {}};
    };
    int invalidPdfPrintCalls = 0;
    installPrintHandler(invalidPdfSource, [&](const Platform::ReadyPdfPrintRequest&) {
        ++invalidPdfPrintCalls;
        return Platform::PdfPrintResult{Platform::PdfPrintStatus::Sent, {}};
    });
    Ui::RosterPrintDialog invalidPdf(path, id(1), false, *choices,
        invalidPdfSource, noOpGeometryPreferencesCallbacks());
    findButton(invalidPdf, "nextRosterPrint")->click();
    QCOMPARE(invalidPdfPrintCalls, 0);
    QCOMPARE(invalidPdf.findChild<QLabel*>(
        QStringLiteral("nextRosterOutputStatus"))->text(),
        QStringLiteral("Unable to load the generated roster PDF."));
    findButton(invalidPdf, "nextRosterPreview")->click();
    QVERIFY(!invalidPdf.findChild<QDialog*>(
        QStringLiteral("nextRosterPreviewDialog")));
    QCOMPARE(invalidPdfPrintCalls, 0);
}

void RosterOutputTests::rosterPreviewSessionOwnsAndRendersGeneratedPdf()
{
    QTemporaryDir dir;
    const QString databasePath = dir.filePath(QStringLiteral("preview-session.tps"));
    QVERIFY(seed(databasePath, QStringLiteral("PREVIEW"), 2));

    auto source = actions();
    Output::RosterOutputRequest request{id(1)};
    request.templateId = R::TemplateId::PerClassWithExtraInfo;
    request.scope = Output::RosterOutputScope::AllClasses;

    auto session = source.pdfPreviewSessionFactory();
    QVERIFY(session);
    QVERIFY(session->isValid());
    const QString generatedPath = session->path();
    const QString temporaryDirectory = QFileInfo(generatedPath).absolutePath();
    QVERIFY(QFileInfo::exists(temporaryDirectory));
    QCOMPARE(source.savePdf(databasePath, request, generatedPath, source.fonts).status,
        DocumentOutputStatus::Sent);
    QVERIFY(QFileInfo::exists(generatedPath));
    QVERIFY(session->loadGeneratedPdf());
    QCOMPARE(session->pageCount(), 2);
    for (int page = 0; page < session->pageCount(); ++page)
    {
        const QImage image = session->renderPage(page, QSize(820, 630));
        QVERIFY(!image.isNull());
        QVERIFY(image.width() <= 820);
        QVERIFY(image.height() <= 630);
        QVERIFY(nonWhitePixelCount(image) > 0);
    }

    session->release();
    QCOMPARE(session->pageCount(), 0);
    QVERIFY(QFileInfo::exists(generatedPath));
    session.reset();
    QVERIFY(!QFileInfo::exists(temporaryDirectory));

    auto malformed = source.pdfPreviewSessionFactory();
    QVERIFY(malformed);
    const QString malformedPath = malformed->path();
    QFile invalidPdf(malformedPath);
    QVERIFY(invalidPdf.open(QIODevice::WriteOnly));
    QCOMPARE(invalidPdf.write("not a PDF"), qint64(9));
    invalidPdf.close();
    QVERIFY(!malformed->loadGeneratedPdf());
    QCOMPARE(malformed->pageCount(), 0);
    const QString malformedDirectory = QFileInfo(malformedPath).absolutePath();
    malformed.reset();
    QVERIFY(!QFileInfo::exists(malformedDirectory));
}

void RosterOutputTests::captures()
{
    QTemporaryDir dir; const auto path = dir.filePath(QStringLiteral("capture.tps")); QVERIFY(seed(path));
    int testingId = 0;
    {
        WorkspaceStore store; QVERIFY(store.openExisting(path)); TestingClass testing;
        testing.name = QStringLiteral("FIXTURE TESTING CLASS"); testing.grade = QStringLiteral("M1"); testing.level = QStringLiteral("Major"); testing.room = QStringLiteral("FIXTURE TEST ROOM"); testing.teacherId = 1;
        const auto created = store.testingClassRepository()->createTestingClass(testing); QVERIFY(created); testingId = *created;
        Roster roster; roster.columns = {QStringLiteral("English"), QStringLiteral("Korean"), QStringLiteral("FIXTURE EXTRA")};
        roster.rows = {{QStringLiteral("FIXTURE TEST"), QStringLiteral("시험명"), QStringLiteral("FIXTURE VALUE")}};
        QVERIFY(store.rosterRepository()->saveRoster(testingId, roster));
    }
    auto source = actions(); const auto choices = source.readChoices(path, id(1), false); QVERIFY(choices);
    const auto testingChoices = source.readChoices(path, id(testingId), true); QVERIFY(testingChoices);
    const auto folder = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("roster-output-visual-captures")); QVERIFY(QDir().mkpath(folder));
    const auto evidenceFolder = QDir(QCoreApplication::applicationDirPath()).filePath(
        QStringLiteral("roster-output-evidence/dark-preview-repair"));
    QVERIFY(QDir().mkpath(evidenceFolder));
    struct PdfEvidence final
    {
        QString path;
        QString text;
        QImage page;
        bool copied = false;
        int pages = 0;
    } regularPdf;
    QString currentCaptureTag;
    const auto generate = source.savePdf;
    source.savePdf = [&](const QString& databasePath, const Output::RosterOutputRequest& request,
                         const QString& outputPath, const R::Fonts& fonts) {
        const auto result = generate(databasePath, request, outputPath, fonts);
        if (!request.testingClass && result.status == DocumentOutputStatus::Sent)
        {
            regularPdf = {};
            regularPdf.path = evidenceFolder + QStringLiteral("/roster_preview_")
                + currentCaptureTag + QStringLiteral(".pdf");
            QFile::remove(regularPdf.path);
            regularPdf.copied = QFile::copy(outputPath, regularPdf.path);
        }
        return result;
    };
    const auto original = qApp->palette(); QJsonArray captures;
    for (const auto& locale : {QStringLiteral("en_US"), QStringLiteral("ko_KR")})
    {
        QTranslator translator;
        if (locale == QStringLiteral("ko_KR")) { QVERIFY(translator.load(QStringLiteral(CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY) + QStringLiteral("/ClassMngr_ko_KR.qm"))); qApp->installTranslator(&translator); }
        // Exercise dark before any light preview in a fresh capture run.
        for (bool dark : {true, false})
        {
            QPalette palette = original;
            palette.setColor(QPalette::Window, QColor(dark ? "#202327" : "#f5f6f7")); palette.setColor(QPalette::WindowText, QColor(dark ? "#eeeeee" : "#181818"));
            palette.setColor(QPalette::Base, QColor(dark ? "#17191c" : "#ffffff")); palette.setColor(QPalette::Text, QColor(dark ? "#eeeeee" : "#181818"));
            palette.setColor(QPalette::Button, QColor(dark ? "#30343a" : "#eeeeee")); palette.setColor(QPalette::ButtonText, QColor(dark ? "#eeeeee" : "#181818")); qApp->setPalette(palette);
            const auto tag = locale + (dark ? QStringLiteral("_dark") : QStringLiteral("_light"));
            currentCaptureTag = tag;
            bool regularCaptureVerified = false;
            QJsonObject regularCaptureEvidence;
            Ui::RosterPrintDialog dialog(path, id(1), false, *choices, source,
                noOpGeometryPreferencesCallbacks()); dialog.show(); QCoreApplication::processEvents();
            QCOMPARE(findButton(dialog, "nextRosterPreview")->text(), locale == QStringLiteral("ko_KR") ? QStringLiteral("미리보기") : QStringLiteral("Preview"));
            QVERIFY(dialog.grab().save(folder + QStringLiteral("/roster_templates_") + tag + QStringLiteral(".png")));
            QTimer::singleShot(0, &dialog, [&] {
                auto* preview = dialog.findChild<QDialog*>(QStringLiteral("nextRosterPreviewDialog")); QVERIFY(preview);
                const RejectPreviewOnExit closePreview{*preview};
                QCoreApplication::processEvents();
                // Capture first, before loading the independent reference, so
                // PDF/font cache warmup cannot hide a production paint issue.
                const QString imagePath = folder + QStringLiteral("/roster_preview_") + tag + QStringLiteral(".png");
                const QPixmap screenshot = preview->grab();
                QVERIFY(screenshot.save(imagePath));
                QVERIFY(regularPdf.copied);
                QPdfDocument reference;
                QCOMPARE(reference.load(regularPdf.path), QPdfDocument::Error::None);
                QCOMPARE(reference.status(), QPdfDocument::Status::Ready);
                regularPdf.pages = reference.pageCount();
                QCOMPARE(regularPdf.pages, 1);
                regularPdf.text = reference.getAllText(0).text();
                const QSize size = reference.pagePointSize(0).toSize().scaled(
                    QSize(820, 630), Qt::KeepAspectRatio);
                regularPdf.page = reference.render(0, size);
                QVERIFY(regularPdf.text.contains(QStringLiteral("Monday")));
                QVERIFY(regularPdf.text.contains(QStringLiteral("FIXTURE S0")));
                QVERIFY(regularPdf.text.contains(QStringLiteral("FIXTURE S1")));
                QVERIFY(!regularPdf.page.isNull());
                // A non-null white raster must not count as a rendered page.
                const qsizetype paintedPixels = nonWhitePixelCount(regularPdf.page);
                QVERIFY(paintedPixels > 10'000);
                auto* pageLabel = preview->findChild<QLabel*>(QStringLiteral("nextRosterPreviewPage"));
                QVERIFY(pageLabel);
                const QPixmap page = pageLabel->pixmap();
                QVERIFY(!page.isNull());
                QCOMPARE(page.toImage().convertToFormat(QImage::Format_RGB32),
                    regularPdf.page.convertToFormat(QImage::Format_RGB32));

                // Inspect the actual PNG artifact, not just the QLabel's
                // backing pixmap, so a blank offscreen capture fails too.
                QCOMPARE(screenshot.devicePixelRatio(), 1.0);
                QRect pageRect = QStyle::alignedRect(pageLabel->layoutDirection(),
                    pageLabel->alignment(), page.size(), pageLabel->contentsRect());
                pageRect.translate(pageLabel->mapTo(preview, QPoint{}));
                const QImage capturedPage = QImage(imagePath).copy(pageRect);
                QCOMPARE(capturedPage.convertToFormat(QImage::Format_RGB32),
                    regularPdf.page.convertToFormat(QImage::Format_RGB32));
                const QString renderedPagePath = evidenceFolder + QStringLiteral("/roster_preview_")
                    + tag + QStringLiteral("-page.png");
                QVERIFY(regularPdf.page.save(renderedPagePath));
                regularCaptureEvidence = {
                    {QStringLiteral("pdf"), regularPdf.path},
                    {QStringLiteral("pdfSha256"), QString::fromLatin1(fileSha256(regularPdf.path))},
                    {QStringLiteral("image"), imagePath},
                    {QStringLiteral("imageSha256"), QString::fromLatin1(fileSha256(imagePath))},
                    {QStringLiteral("renderedPage"), renderedPagePath},
                    {QStringLiteral("pdfPageCount"), regularPdf.pages},
                    {QStringLiteral("expectedPdfText"), QJsonArray{QStringLiteral("Monday"), QStringLiteral("FIXTURE S0"), QStringLiteral("FIXTURE S1")}},
                    {QStringLiteral("pdfContainsExpectedFixtureText"), true},
                    {QStringLiteral("visiblePageMatchesPdfRaster"), true},
                    {QStringLiteral("pageNonWhitePixels"), static_cast<qint64>(paintedPixels)}
                };
                regularCaptureVerified = true;
            });
            findButton(dialog, "nextRosterPreview")->click();
            QVERIFY(regularCaptureVerified);
            Ui::RosterPrintDialog testingDialog(path, id(testingId), true, *testingChoices, source,
                noOpGeometryPreferencesCallbacks()); testingDialog.show(); QCoreApplication::processEvents();
            QVERIFY(testingDialog.grab().save(folder + QStringLiteral("/testing_roster_templates_") + tag + QStringLiteral(".png")));
            QTimer::singleShot(0, &testingDialog, [&] {
                auto* preview = testingDialog.findChild<QDialog*>(QStringLiteral("nextRosterPreviewDialog")); QVERIFY(preview); QCoreApplication::processEvents();
                const auto pixmap = preview->findChild<QLabel*>(QStringLiteral("nextRosterPreviewPage"))->pixmap();
                QVERIFY(pixmap.height() > pixmap.width());
                QVERIFY(preview->grab().save(folder + QStringLiteral("/testing_roster_preview_") + tag + QStringLiteral(".png"))); preview->reject();
            });
            findButton(testingDialog, "nextRosterPreview")->click();
            captures.append(QJsonObject{{QStringLiteral("locale"), locale}, {QStringLiteral("theme"), dark ? QStringLiteral("dark") : QStringLiteral("light")}, {QStringLiteral("fixture"), QStringLiteral("Synthetic FIXTURE values only")}, {QStringLiteral("regularPreviewEvidence"), regularCaptureEvidence}});
        }
        qApp->removeTranslator(&translator);
    }
    qApp->setPalette(original); QFile manifest(folder + QStringLiteral("/manifest.json")); QVERIFY(manifest.open(QIODevice::WriteOnly)); manifest.write(QJsonDocument(QJsonObject{{QStringLiteral("task"), QStringLiteral("P7-C37-IMPLEMENT")}, {QStringLiteral("captures"), captures}}).toJson());
}
QTEST_MAIN(RosterOutputTests)
#include "roster_output_tests.moc"
