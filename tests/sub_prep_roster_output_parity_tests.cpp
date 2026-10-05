#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "domain/models/class_info.h"
#include "domain/models/roster.h"
#include "domain/models/teacher.h"
#include "features/sub_prep/services/sub_prep_package_service.h"

#if __has_include("next/platform/application_services_sub_prep_roster_output_source_port.h")
#include "next/platform/application_services_sub_prep_roster_output_source_port.h"
#define CLASSMNGR_PARITY_TYPED_ROSTER_SOURCE 1
#endif

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPdfDocument>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <string>

namespace
{
#ifdef CLASSMNGR_PARITY_TYPED_ROSTER_SOURCE
using SourcePort = ClassMngr::Next::Platform::ApplicationServicesSubPrepRosterOutputSourcePort;
#else
struct SourcePort
{
    explicit SourcePort(ApplicationServices&) {}
};
#endif

template<class Request, class Port>
void setSource(Request& request, ApplicationServices& services, const QList<int>& ids, Port& port)
{
    if constexpr (requires { request.services; request.classIds; })
    {
        request.services = &services;
        request.classIds = ids;
    }
    else
    {
        request.rosterOutputSourceReadPort = &port;
        using Id = typename decltype(request.selectedClassIds)::value_type;
        for (const int id : ids)
            request.selectedClassIds.push_back(*Id::fromString(std::to_string(id)));
    }
}

struct Fixture
{
    QTemporaryDir directory;
    ApplicationServices services;

    bool open()
    {
        return directory.isValid()
            && services.openDatabase(directory.filePath(QStringLiteral("roster-output.tps")));
    }

    int teacher(const QString& name, const QString& room)
    {
        Teacher value;
        value.teacherEn = name;
        value.preferredName = name;
        value.roomNumber = room;
        value.internetType = QStringLiteral("WiFi");
        value.wifiName = name + QStringLiteral(" Network");
        value.wifiPassword = QStringLiteral("network-key");
        value.zoomId = name.toLower() + QStringLiteral(".zoom");
        value.zoomPassword = QStringLiteral("zoom-key");
        value.projectionType = QStringLiteral("HDMI");
        const auto created = services.teacherService()->create(value);
        return created ? *created : -1;
    }

    int classroom(const QString& name, const int teacherId, const QString& grade,
        const QString& level, const QList<ClassTime>& regular,
        const QList<ClassTime>& intensive, const QString& student)
    {
        const auto created = services.classService()->create(name);
        if (!created)
            return -1;
        const auto loaded = services.classService()->classInfo(*created);
        if (!loaded)
            return -1;
        ClassInfo info = *loaded;
        info.teacherId = teacherId;
        info.classGrade = grade;
        info.classLevel = level;
        info.classTimes = regular;
        info.intensiveTimes = intensive;
        if (!services.classService()->saveClassInfo(info))
            return -1;
        Roster roster;
        roster.columns = Roster::BaseColumns;
        roster.columns.append({QStringLiteral("Notes"), QStringLiteral("Allergies"), QStringLiteral("Hidden")});
        roster.rows = {{student, QStringLiteral("\uAE40\uBBFC\uC218"), {}, {}, {}, {},
            student + QStringLiteral(" note"), student + QStringLiteral(" allergy"),
            student + QStringLiteral(" secret")}};
        return services.rosterService()->saveRoster(*created, roster) ? *created : -1;
    }
};

SubPrepPackageService::Request requestFor(const QString& target, const QList<QDate>& dates,
    const RosterTemplatePrintService::TemplateId rosterTemplate)
{
    SubPrepPackageService::Request request;
    request.targetRoot = target;
    request.userName = QStringLiteral("Parity");
    request.selectedDates = dates;
    request.rosterTemplate = rosterTemplate;
    request.openFolderAfterGeneration = false;
    request.printPaperCopies = false;
    request.subPrep.subNotes = QStringLiteral("Roster output parity packet.");
    request.subPrep.schedule.days = {QStringLiteral("Monday"), QStringLiteral("Tuesday"), QStringLiteral("Wednesday")};
    return request;
}

SubPrepPackageService::Result generate(Fixture& fixture, SubPrepPackageService::Request request,
    const QList<int>& ids)
{
    SourcePort port(fixture.services);
    setSource(request, fixture.services, ids, port);
    return SubPrepPackageService::generate(request);
}

QString normalizedText(const QString& text)
{
    return QString(text).replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" ")).trimmed();
}

QString compactText(const QString& text)
{
    return text.normalized(QString::NormalizationForm_KC).remove(QRegularExpression(QStringLiteral("\\s+")));
}

struct Pdf
{
    int pages = 0;
    QString text;
    bool loaded = false;
};

Pdf readPdf(const QString& path)
{
    QPdfDocument document;
    Pdf pdf;
    if (document.load(path) != QPdfDocument::Error::None
        || document.status() != QPdfDocument::Status::Ready)
        return pdf;
    pdf.pages = document.pageCount();
    QStringList texts;
    for (int index = 0; index < pdf.pages; ++index)
        texts.append(document.getAllText(index).text());
    pdf.text = normalizedText(texts.join(QLatin1Char('\n')));
    pdf.loaded = pdf.pages > 0;
    return pdf;
}

QStringList relativePaths(const SubPrepPackageService::Result& result)
{
    QStringList paths;
    for (const QString& path : result.documentPaths)
        paths.append(QDir::fromNativeSeparators(QDir(result.outputDirectory).relativeFilePath(path)));
    return paths;
}

QStringList folders(const SubPrepPackageService::Result& result)
{
    return QDir(result.outputDirectory).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
}

QJsonObject transcript(const QString& name, const SubPrepPackageService::Result& result)
{
    QJsonArray documents;
    const QStringList paths = relativePaths(result);
    for (int index = 0; index < paths.size(); ++index)
    {
        const Pdf pdf = readPdf(result.documentPaths.at(index));
        documents.append(QJsonObject{{QStringLiteral("path"), paths.at(index)},
            {QStringLiteral("pages"), pdf.pages}, {QStringLiteral("text"), pdf.text}});
    }
    return {{QStringLiteral("case"), name}, {QStringLiteral("status"), QStringLiteral("Completed")},
        {QStringLiteral("folder"), QFileInfo(result.outputDirectory).fileName()},
        {QStringLiteral("class-folders"), QJsonArray::fromStringList(folders(result))},
        {QStringLiteral("documents"), documents}};
}

void logTranscript(const QJsonObject& value)
{
    const QString json = QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Compact));
    QString ascii;
    for (const QChar character : json)
        if (character.unicode() > 0x7F)
            ascii += QStringLiteral("\\u%1").arg(static_cast<unsigned int>(character.unicode()), 4, 16, QLatin1Char('0'));
        else
            ascii += character;
    qInfo().noquote() << "F353_TRANSCRIPT" << ascii;
}
}

class SubPrepRosterOutputParityTests final : public QObject
{
    Q_OBJECT
private slots:
    void byDayRegularFiltersAndOrdersThePacket();
    void dailyUsesIntensiveMeetings();
    void perClassProjectsOrderedCustomColumns();
};

void SubPrepRosterOutputParityTests::byDayRegularFiltersAndOrdersThePacket()
{
    Fixture fixture;
    QVERIFY(fixture.open());
    const int alice = fixture.teacher(QStringLiteral("Alice"), QStringLiteral("501"));
    const int bob = fixture.teacher(QStringLiteral("Bob"), QStringLiteral("502"));
    QVERIFY(alice > 0 && bob > 0);
    const int first = fixture.classroom(QStringLiteral("First"), alice, QStringLiteral("E4"),
        QStringLiteral("Theseus"), {{QStringLiteral("Monday"), QStringLiteral("4:00 PM"), QStringLiteral("4:50 PM")}}, {}, QStringLiteral("Alex"));
    const int second = fixture.classroom(QStringLiteral("Second"), bob, QStringLiteral("E5"),
        QStringLiteral("Hermes"), {{QStringLiteral("Wednesday"), QStringLiteral("5:00 PM"), QStringLiteral("5:50 PM")}}, {}, QStringLiteral("Casey"));
    const int excluded = fixture.classroom(QStringLiteral("Excluded"), alice, QStringLiteral("E4"),
        QStringLiteral("Hercules"), {{QStringLiteral("Friday"), QStringLiteral("6:00 PM"), QStringLiteral("6:50 PM")}}, {}, QStringLiteral("ExcludedStudent"));
    QVERIFY(first > 0 && second > 0 && excluded > 0);
    QTemporaryDir output;
    QVERIFY(output.isValid());
    const auto request = requestFor(output.path(), {QDate(2026, 10, 5), QDate(2026, 10, 7)},
        RosterTemplatePrintService::TemplateId::ByDay);
    const auto result = generate(fixture, request, {excluded, second, first});
    QCOMPARE(result.status, SubPrepPackageService::Status::Completed);
    QVERIFY(result.folderCreated);
    QCOMPARE(QFileInfo(result.outputDirectory).fileName(), QStringLiteral("Parity (05 - 07 Oct 2026)"));
    QCOMPARE(relativePaths(result), (QStringList{QStringLiteral("Sub Prep.pdf"), QStringLiteral("Rosters - By Day.pdf")}));
    const auto classFolders = folders(result);
    QCOMPARE(classFolders, (QStringList{QStringLiteral("E4 Theseus - Alice - Mon (4.00)"),
        QStringLiteral("E5 Hermes - Bob - Wed (5.00)")}));
    QVERIFY(classFolders.at(0).contains(QStringLiteral("E4 Theseus")) && classFolders.at(0).contains(QStringLiteral("Alice")));
    QVERIFY(classFolders.at(0).contains(QStringLiteral("Mon (4.00)")));
    QVERIFY(classFolders.at(1).contains(QStringLiteral("E5 Hermes")) && classFolders.at(1).contains(QStringLiteral("Bob")));
    QVERIFY(classFolders.at(1).contains(QStringLiteral("Wed (5.00)")));
    const Pdf pdf = readPdf(result.documentPaths.at(1));
    QVERIFY(readPdf(result.documentPaths.first()).loaded);
    QVERIFY(pdf.loaded);
    QCOMPARE(pdf.pages, 2);
    const QString compact = compactText(pdf.text);
    logTranscript(transcript(QStringLiteral("by-day-regular"), result));
    for (const auto& token : {QStringLiteral("Monday"), QStringLiteral("Wednesday"), QStringLiteral("E4 Theseus"),
             QStringLiteral("E5 Hermes"),
             QStringLiteral("Alex"), QStringLiteral("Casey")})
        QVERIFY2(compact.contains(compactText(token)), qPrintable(token + QStringLiteral(" absent from ") + pdf.text));
    // Qt PDF text extraction can map the By Day parentheses to these private-use code points.
    QVERIFY(compact.contains(QStringLiteral("Alice(501)")) || compact.contains(QStringLiteral("Alice\uE081501\uE082")));
    QVERIFY(compact.contains(QStringLiteral("Bob(502)")) || compact.contains(QStringLiteral("Bob\uE081502\uE082")));
    QCOMPARE(compact.count(compactText(QStringLiteral("Alex"))), 1);
    QCOMPARE(compact.count(compactText(QStringLiteral("Casey"))), 1);
    QVERIFY(!compact.contains(compactText(QStringLiteral("ExcludedStudent"))));
    QVERIFY(!compact.contains(compactText(QStringLiteral("Excludedstudent"))));
    QVERIFY(!compact.contains(compactText(QStringLiteral("E4 Hercules"))));
    QVERIFY(!compact.contains(compactText(QStringLiteral("Friday"))));
}

void SubPrepRosterOutputParityTests::dailyUsesIntensiveMeetings()
{
    Fixture fixture;
    QVERIFY(fixture.open());
    const int alice = fixture.teacher(QStringLiteral("Alice"), QStringLiteral("501"));
    const int bob = fixture.teacher(QStringLiteral("Bob"), QStringLiteral("502"));
    QVERIFY(alice > 0 && bob > 0);
    const int included = fixture.classroom(QStringLiteral("Intensive"), alice, QStringLiteral("E4"), QStringLiteral("Theseus"),
        {{QStringLiteral("Monday"), QStringLiteral("5:00 PM"), QStringLiteral("5:50 PM")}},
        {{QStringLiteral("Tuesday"), QStringLiteral("4:00 PM"), QStringLiteral("4:50 PM")}}, QStringLiteral("Alex"));
    const int excluded = fixture.classroom(QStringLiteral("Regular only"), bob, QStringLiteral("E5"), QStringLiteral("Hermes"),
        {{QStringLiteral("Tuesday"), QStringLiteral("5:00 PM"), QStringLiteral("5:50 PM")}}, {}, QStringLiteral("RegularStudent"));
    QVERIFY(included > 0 && excluded > 0);
    QTemporaryDir output;
    QVERIFY(output.isValid());
    auto request = requestFor(output.path(), {QDate(2026, 10, 6)}, RosterTemplatePrintService::TemplateId::Daily);
    request.useIntensiveSchedule = true;
    const auto result = generate(fixture, request, {excluded, included});
    QCOMPARE(result.status, SubPrepPackageService::Status::Completed);
    QCOMPARE(relativePaths(result), (QStringList{QStringLiteral("Sub Prep.pdf"), QStringLiteral("Rosters - Daily.pdf")}));
    const auto classFolders = folders(result);
    QCOMPARE(classFolders, (QStringList{QStringLiteral("E4 Theseus - Alice - Tues (4.00)")}));
    QVERIFY(classFolders.first().contains(QStringLiteral("E4 Theseus")));
    QVERIFY(classFolders.first().contains(QStringLiteral("Tues (4.00)")));
    const Pdf pdf = readPdf(result.documentPaths.at(1));
    QVERIFY(readPdf(result.documentPaths.first()).loaded);
    QVERIFY(pdf.loaded);
    QCOMPARE(pdf.pages, 1);
    const QString compact = compactText(pdf.text);
    for (const auto& token : {QStringLiteral("TUESDAY"), QStringLiteral("E4 Theseus"), QStringLiteral("4 p.m."),
             QStringLiteral("Alice"), QStringLiteral("Room 501"), QStringLiteral("alice.zoom"), QStringLiteral("zoom-key"), QStringLiteral("Alex")})
        QVERIFY2(compact.contains(compactText(token)), qPrintable(token + QStringLiteral(" absent from ") + pdf.text));
    QVERIFY(!compact.contains(compactText(QStringLiteral("Monday"))));
    QVERIFY(!compact.contains(compactText(QStringLiteral("MONDAY"))));
    QVERIFY(!compact.contains(compactText(QStringLiteral("5 p.m."))));
    QVERIFY(!compact.contains(compactText(QStringLiteral("RegularStudent"))));
    QVERIFY(!compact.contains(compactText(QStringLiteral("Regularstudent"))));
    QVERIFY(!compact.contains(compactText(QStringLiteral("E5 Hermes"))));
    logTranscript(transcript(QStringLiteral("daily-intensive"), result));
}

void SubPrepRosterOutputParityTests::perClassProjectsOrderedCustomColumns()
{
    Fixture fixture;
    QVERIFY(fixture.open());
    const int alice = fixture.teacher(QStringLiteral("Alice"), QStringLiteral("501"));
    const int bob = fixture.teacher(QStringLiteral("Bob"), QStringLiteral("502"));
    QVERIFY(alice > 0 && bob > 0);
    const int first = fixture.classroom(QStringLiteral("First"), alice, QStringLiteral("E4"), QStringLiteral("Theseus"),
        {{QStringLiteral("Monday"), QStringLiteral("4:00 PM"), QStringLiteral("4:50 PM")}}, {}, QStringLiteral("Alex"));
    const int second = fixture.classroom(QStringLiteral("Second"), bob, QStringLiteral("E5"), QStringLiteral("Hermes"),
        {{QStringLiteral("Monday"), QStringLiteral("5:00 PM"), QStringLiteral("5:50 PM")}}, {}, QStringLiteral("Casey"));
    QVERIFY(first > 0 && second > 0);
    QTemporaryDir output;
    QVERIFY(output.isValid());
    auto request = requestFor(output.path(), {QDate(2026, 10, 5)}, RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo);
    request.selectedExtraColumns = {QStringLiteral("Allergies"), QStringLiteral("Notes")};
    const auto result = generate(fixture, request, {second, first});
    QCOMPARE(result.status, SubPrepPackageService::Status::Completed);
    const auto classFolders = folders(result);
    QCOMPARE(classFolders, (QStringList{QStringLiteral("E4 Theseus - Alice - Mon (4.00)"),
        QStringLiteral("E5 Hermes - Bob - Mon (5.00)")}));
    const QStringList expected{QStringLiteral("Sub Prep.pdf"), classFolders.at(0) + QStringLiteral("/Roster.pdf"),
        classFolders.at(1) + QStringLiteral("/Roster.pdf")};
    QCOMPARE(relativePaths(result), expected);
    QVERIFY(readPdf(result.documentPaths.first()).loaded);
    for (int index = 0; index < 2; ++index)
    {
        const QString student = index == 0 ? QStringLiteral("Alex") : QStringLiteral("Casey");
        const QString other = index == 0 ? QStringLiteral("Casey") : QStringLiteral("Alex");
        const QString label = index == 0 ? QStringLiteral("E4 Theseus") : QStringLiteral("E5 Hermes");
        QVERIFY(classFolders.at(index).contains(label));
        const Pdf pdf = readPdf(result.documentPaths.at(index + 1));
        QVERIFY(pdf.loaded);
        QCOMPARE(pdf.pages, 1);
        const QString compact = compactText(pdf.text);
        for (const auto& token : {label, student, QStringLiteral("Allergies"), QStringLiteral("Notes"),
                 student + QStringLiteral(" allergy"), student + QStringLiteral(" note")})
            QVERIFY2(compact.contains(compactText(token)), qPrintable(token + QStringLiteral(" absent from ") + pdf.text));
        const qsizetype allergies = compact.indexOf(QStringLiteral("Allergies"));
        QVERIFY(allergies >= 0);
        QVERIFY(compact.indexOf(QStringLiteral("Notes"), allergies + QStringLiteral("Allergies").size()) > allergies);
        QVERIFY(!compact.contains(compactText(QStringLiteral("Hidden"))));
        QVERIFY(!compact.contains(compactText(QStringLiteral("secret"))));
        QVERIFY(!compact.contains(compactText(other)));
    }
    logTranscript(transcript(QStringLiteral("per-class-columns"), result));
}

QTEST_MAIN(SubPrepRosterOutputParityTests)
#include "sub_prep_roster_output_parity_tests.moc"
