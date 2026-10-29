#include "core/fontmanager.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/teacher_repository.h"
#include "data/repositories/testing_class_repository.h"
#include "features/roster/services/roster_template_print_service.h"
#include "next/platform/workspace_store_roster_actions.h"
#include "next/platform/workspace_store_roster_output.h"
#include "next/ui/roster_editor_widget.h"
#include "next/ui/roster_print_dialog.h"
#include "next/ui/roster_table_model.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPdfDocument>
#include <QPdfSelection>
#include <QPushButton>
#include <QSqlQuery>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QTranslator>
#include <QtTest>

using namespace ClassMngr::Next;
namespace R = RosterTemplateRenderer;
namespace
{
Domain::ClassId id(int value) { return *Domain::ClassId::fromString(std::to_string(value)); }
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
    result.savePdf = [](const QString& path, const Output::RosterOutputRequest& request, const QString& pdf, const R::Fonts& fonts) {
        return Platform::saveRosterOutputPdfAtPath(path, request, pdf, fonts);
    };
    result.fonts = {FontManager::getUiFont(-1), FontManager::getKoreanFont(-1)};
    return result;
}
QPushButton* findButton(QWidget& dialog, const char* name) { return dialog.findChild<QPushButton*>(QString::fromLatin1(name)); }
}
class RosterOutputTests final : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() { FontManager::loadFonts(); }
    void sparseNamesAndPositionalProjection();
    void scopesProfilesAndDailyPageOrder();
    void templateParity_data();
    void templateParity();
    void restrictionsAndAtomicFailure();
    void pageReleaseAndLargeRoute();
    void editorSaveGate();
    void dialogCancelFailureAndPreview();
    void printPortReceivesGeneratedPdfAndReportsCancelAndFailure();
    void captures();
};

void RosterOutputTests::sparseNamesAndPositionalProjection()
{
    QTemporaryDir dir; const auto path = dir.filePath(QStringLiteral("sparse.tps")); QVERIFY(seed(path));
    WorkspaceStore store; QVERIFY(store.openExisting(path)); auto db = store.database(); QSqlQuery query(db);
    QVERIFY(query.exec(QStringLiteral("DELETE FROM roster_data WHERE class_id=1")));
    for (int index = 0; index < 26; ++index)
    {
        query.prepare(QStringLiteral("INSERT INTO roster_data(class_id,row_index,col_index,value) VALUES(1,?,?,?)"));
        query.addBindValue(index == 0 ? 25LL : 1'000'000'000LL + index * 100LL);
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
    QCOMPARE(Platform::saveRosterOutputPdfAtPath(path, request, nextPath, actions().fonts).status, DocumentOutputStatus::Sent);
    QCOMPARE(RosterTemplatePrintService::saveRostersPdf(legacy, legacyPath, request.templateId, request.extraColumns, request.orientation).status, DocumentOutputStatus::Sent);
    QPdfDocument next, old; QCOMPARE(next.load(nextPath), QPdfDocument::Error::None); QCOMPARE(old.load(legacyPath), QPdfDocument::Error::None);
    QCOMPARE(next.pageCount(), old.pageCount());
    for (int page = 0; page < next.pageCount(); ++page)
    {
        QCOMPARE(next.pagePointSize(page), old.pagePointSize(page));
        const auto size = next.pagePointSize(page); QCOMPARE(size.width() > size.height(), landscape);
        QCOMPARE(next.getAllText(page).text(), old.getAllText(page).text());
        QCOMPARE(next.render(page, QSize(1000, 1000)), old.render(page, QSize(1000, 1000)));
    }
    if (templateId == 2) { const auto text = pdfText(nextPath); QVERIFY(text.contains(QStringLiteral("FIXTURE B"))); QVERIFY(!text.contains(QStringLiteral("FIXTURE C"))); QVERIFY(text.contains(QStringLiteral("FIXTURE_WIFI"))); QVERIFY(text.contains(QStringLiteral("FIXTURE_ZOOM"))); }
    if (templateId == 1) QVERIFY(!pdfText(nextPath).contains(QStringLiteral("FIXTURE_WIFI")));
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
    const auto loaded = Platform::readRosterWindowAtPath(path, id(1)); QVERIFY(loaded); QCOMPARE(loaded.value().roster.rows[0][0], std::u16string(u"Fixture Saved Edit"));
}

void RosterOutputTests::dialogCancelFailureAndPreview()
{
    QTemporaryDir dir; const auto path = dir.filePath(QStringLiteral("dialog.tps")); QVERIFY(seed(path));
    auto source = actions(); int outputs = 0, prints = 0;
    const auto generate = source.savePdf;
    source.savePdf = [&](const QString& p, const Output::RosterOutputRequest& r, const QString& out, const R::Fonts& fonts) { ++outputs; return generate(p, r, out, fonts); };
    source.chooseSavePath = [](QWidget*) { return QString{}; };
    source.printPdf = [&](QWidget*, const QString&, QPageLayout::Orientation) { ++prints; return DocumentOutputResult{DocumentOutputStatus::Canceled, {}}; };
    const auto choices = source.readChoices(path, id(1), false); QVERIFY(choices);
    Ui::RosterPrintDialog dialog(path, id(1), false, *choices, source);
    findButton(dialog, "nextRosterSavePdf")->click(); QCOMPARE(outputs, 0);
    findButton(dialog, "nextRosterPrint")->click(); QCOMPARE(outputs, 1); QCOMPARE(prints, 1);
    QTimer::singleShot(0, [&] {
        auto* preview = dialog.findChild<QDialog*>(QStringLiteral("nextRosterPreviewDialog")); QVERIFY(preview);
        QVERIFY(!preview->findChild<QLabel*>(QStringLiteral("nextRosterPreviewPage"))->pixmap().isNull());
        findButton(*preview, "nextRosterPreviewSavePdf")->click(); findButton(*preview, "nextRosterPreviewPrint")->click(); preview->reject();
    });
    findButton(dialog, "nextRosterPreview")->click(); QCOMPARE(outputs, 2); QCOMPARE(prints, 2);
    source.chooseSavePath = [&](QWidget*) { return dir.filePath(QStringLiteral("saved.pdf")); };
    Ui::RosterPrintDialog saved(path, id(1), false, *choices, source);
    findButton(saved, "nextRosterSavePdf")->click(); QCOMPARE(pageCount(dir.filePath(QStringLiteral("saved.pdf"))), 1);
    source.printPdf = [](QWidget*, const QString&, QPageLayout::Orientation) { return DocumentOutputResult{DocumentOutputStatus::Failed, QStringLiteral("FIXTURE PRINT FAILURE")}; };
    Ui::RosterPrintDialog printFailure(path, id(1), false, *choices, source);
    findButton(printFailure, "nextRosterPrint")->click();
    QCOMPARE(printFailure.findChild<QLabel*>(QStringLiteral("nextRosterOutputStatus"))->text(), QStringLiteral("FIXTURE PRINT FAILURE"));
    source.savePdf = [](const QString&, const Output::RosterOutputRequest&, const QString&, const R::Fonts&) { return DocumentOutputResult{DocumentOutputStatus::Failed, QStringLiteral("FIXTURE READ FAILURE")}; };
    source.chooseSavePath = [&](QWidget*) { return dir.filePath(QStringLiteral("failed.pdf")); };
    Ui::RosterPrintDialog failure(path, id(1), false, *choices, source);
    findButton(failure, "nextRosterSavePdf")->click(); QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("failed.pdf"))));
    QCOMPARE(failure.findChild<QLabel*>(QStringLiteral("nextRosterOutputStatus"))->text(), QStringLiteral("FIXTURE READ FAILURE"));
    findButton(failure, "nextRosterPreview")->click();
    QVERIFY(!failure.findChild<QDialog*>(QStringLiteral("nextRosterPreviewDialog")));
    Ui::RosterPrintDialog testing(path, id(1), true, *choices, source);
    QCOMPARE(testing.findChild<QComboBox*>(QStringLiteral("nextRosterTemplate"))->count(), 1);
    QCOMPARE(testing.request().templateId, R::TemplateId::PerClassWithExtraInfo); QCOMPARE(testing.request().scope, Output::RosterOutputScope::CurrentClass);
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
    QList<int> submittedPageCounts;
    QList<QPageLayout::Orientation> submittedOrientations;
    QList<bool> submittedPdfReadable;
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
    source.printPdf = [&](QWidget*, const QString& pdfPath,
                          QPageLayout::Orientation orientation) {
        ++printCount;
        submittedPaths.append(pdfPath);
        submittedOrientations.append(orientation);

        QPdfDocument document;
        const bool readable = QFileInfo(pdfPath).isFile()
            && document.load(pdfPath) == QPdfDocument::Error::None
            && document.pageCount() > 0;
        submittedPdfReadable.append(readable);
        submittedPageCounts.append(readable ? document.pageCount() : -1);
        submittedTexts.append(
            readable ? document.getAllText(0).text() : QString{}
            );
        return printResult;
    };

    const auto choices = source.readChoices(path, id(1), false);
    QVERIFY(choices);

    Ui::RosterPrintDialog canceled(path, id(1), false, *choices, source);
    findButton(canceled, "nextRosterPrint")->click();
    QCOMPARE(outputCount, 1);
    QCOMPARE(printCount, 1);
    QCOMPARE(submittedPaths, generatedPaths);
    QCOMPARE(submittedPdfReadable, QList<bool>({true}));
    QCOMPARE(submittedPageCounts, QList<int>({1}));
    QCOMPARE(submittedOrientations,
             QList<QPageLayout::Orientation>({QPageLayout::Landscape}));
    QVERIFY(submittedTexts.front().contains(QStringLiteral("FIXTURE S0")));
    QVERIFY(submittedTexts.front().contains(QStringLiteral("FIXTURE S1")));
    QVERIFY(canceled.findChild<QLabel*>(
        QStringLiteral("nextRosterOutputStatus"))->text()
        != QStringLiteral("FIXTURE PRINT CANCELED"));

    printResult = {
        DocumentOutputStatus::Failed,
        QStringLiteral("FIXTURE PRINT FAILURE")
    };
    Ui::RosterPrintDialog failed(path, id(1), false, *choices, source);
    findButton(failed, "nextRosterPrint")->click();
    QCOMPARE(outputCount, 2);
    QCOMPARE(printCount, 2);
    QCOMPARE(submittedPaths.size(), 2);
    QCOMPARE(submittedPaths.at(1), generatedPaths.at(1));
    QCOMPARE(submittedPdfReadable, QList<bool>({true, true}));
    QCOMPARE(submittedPageCounts, QList<int>({1, 1}));
    QCOMPARE(failed.findChild<QLabel*>(
        QStringLiteral("nextRosterOutputStatus"))->text(),
        QStringLiteral("FIXTURE PRINT FAILURE"));
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
    const auto source = actions(); const auto choices = source.readChoices(path, id(1), false); QVERIFY(choices);
    const auto testingChoices = source.readChoices(path, id(testingId), true); QVERIFY(testingChoices);
    const auto folder = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("roster-output-visual-captures")); QVERIFY(QDir().mkpath(folder));
    const auto original = qApp->palette(); QJsonArray captures;
    for (const auto& locale : {QStringLiteral("en_US"), QStringLiteral("ko_KR")})
    {
        QTranslator translator;
        if (locale == QStringLiteral("ko_KR")) { QVERIFY(translator.load(QStringLiteral(CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY) + QStringLiteral("/ClassMngr_ko_KR.qm"))); qApp->installTranslator(&translator); }
        for (bool dark : {false, true})
        {
            QPalette palette = original;
            palette.setColor(QPalette::Window, QColor(dark ? "#202327" : "#f5f6f7")); palette.setColor(QPalette::WindowText, QColor(dark ? "#eeeeee" : "#181818"));
            palette.setColor(QPalette::Base, QColor(dark ? "#17191c" : "#ffffff")); palette.setColor(QPalette::Text, QColor(dark ? "#eeeeee" : "#181818"));
            palette.setColor(QPalette::Button, QColor(dark ? "#30343a" : "#eeeeee")); palette.setColor(QPalette::ButtonText, QColor(dark ? "#eeeeee" : "#181818")); qApp->setPalette(palette);
            const auto tag = locale + (dark ? QStringLiteral("_dark") : QStringLiteral("_light"));
            Ui::RosterPrintDialog dialog(path, id(1), false, *choices, source); dialog.show(); QCoreApplication::processEvents();
            QCOMPARE(findButton(dialog, "nextRosterPreview")->text(), locale == QStringLiteral("ko_KR") ? QStringLiteral("미리보기") : QStringLiteral("Preview"));
            QVERIFY(dialog.grab().save(folder + QStringLiteral("/roster_templates_") + tag + QStringLiteral(".png")));
            QTimer::singleShot(0, [&] {
                auto* preview = dialog.findChild<QDialog*>(QStringLiteral("nextRosterPreviewDialog")); QVERIFY(preview); QCoreApplication::processEvents();
                QVERIFY(preview->grab().save(folder + QStringLiteral("/roster_preview_") + tag + QStringLiteral(".png"))); preview->reject();
            });
            findButton(dialog, "nextRosterPreview")->click();
            Ui::RosterPrintDialog testingDialog(path, id(testingId), true, *testingChoices, source); testingDialog.show(); QCoreApplication::processEvents();
            QVERIFY(testingDialog.grab().save(folder + QStringLiteral("/testing_roster_templates_") + tag + QStringLiteral(".png")));
            QTimer::singleShot(0, [&] {
                auto* preview = testingDialog.findChild<QDialog*>(QStringLiteral("nextRosterPreviewDialog")); QVERIFY(preview); QCoreApplication::processEvents();
                const auto pixmap = preview->findChild<QLabel*>(QStringLiteral("nextRosterPreviewPage"))->pixmap();
                QVERIFY(pixmap.height() > pixmap.width());
                QVERIFY(preview->grab().save(folder + QStringLiteral("/testing_roster_preview_") + tag + QStringLiteral(".png"))); preview->reject();
            });
            findButton(testingDialog, "nextRosterPreview")->click();
            captures.append(QJsonObject{{QStringLiteral("locale"), locale}, {QStringLiteral("theme"), dark ? QStringLiteral("dark") : QStringLiteral("light")}, {QStringLiteral("fixture"), QStringLiteral("Synthetic FIXTURE values only")}});
        }
        qApp->removeTranslator(&translator);
    }
    qApp->setPalette(original); QFile manifest(folder + QStringLiteral("/manifest.json")); QVERIFY(manifest.open(QIODevice::WriteOnly)); manifest.write(QJsonDocument(QJsonObject{{QStringLiteral("task"), QStringLiteral("P7-C37-IMPLEMENT")}, {QStringLiteral("captures"), captures}}).toJson());
}
QTEST_MAIN(RosterOutputTests)
#include "roster_output_tests.moc"
