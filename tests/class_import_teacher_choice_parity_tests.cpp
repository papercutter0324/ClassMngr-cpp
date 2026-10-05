#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/class_info.h"
#include "domain/models/class_transfer.h"
#include "features/classes/ui/class_import_dialog.h"

#include <QComboBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <memory>
#include <type_traits>

namespace
{
Teacher incomingTeacher()
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("\uAE40\uC54C\uB809\uC2A4");
    teacher.teacherEn = QStringLiteral("Alex Kim");
    teacher.preferredRomanization = QStringLiteral("Incoming Teacher");
    teacher.preferredName = teacher.preferredRomanization;
    teacher.roomNumber = QStringLiteral("405");
    teacher.birthday = QStringLiteral("02-29");
    teacher.phoneNumber = QStringLiteral("010-1234-5678");
    teacher.internetType = QStringLiteral("Both");
    teacher.wifiName = QStringLiteral("Incoming WiFi");
    teacher.wifiPassword = QStringLiteral("Incoming key");
    teacher.projectionType = QStringLiteral("Zoom");
    teacher.zoomId = QStringLiteral("incoming.zoom");
    teacher.zoomPassword = QStringLiteral("Incoming Zoom key");
    teacher.notes = QStringLiteral("Incoming \uBA54\uBAA8\nUnicode \U0001F393");
    return teacher;
}

Teacher localTeacher()
{
    Teacher teacher = incomingTeacher();
    teacher.preferredRomanization = QStringLiteral("Local Teacher");
    teacher.preferredName = teacher.preferredRomanization;
    teacher.roomNumber = QStringLiteral("507");
    teacher.birthday = QStringLiteral("03-01");
    teacher.phoneNumber = QStringLiteral("010-9876-5432");
    teacher.internetType = QStringLiteral("LAN");
    teacher.wifiName = QStringLiteral("Local WiFi");
    teacher.wifiPassword = QStringLiteral("Local key");
    teacher.projectionType = QStringLiteral("HDMI");
    teacher.zoomId = QStringLiteral("local.zoom");
    teacher.zoomPassword = QStringLiteral("Local Zoom key");
    teacher.notes = QStringLiteral("Local \uBA54\uBAA8\nPreserve this profile");
    return teacher;
}

QStringList profileFields(const Teacher& teacher)
{
    return {teacher.teacherKr, teacher.teacherEn, teacher.preferredRomanization, teacher.preferredName,
        teacher.roomNumber, teacher.birthday, teacher.phoneNumber, teacher.internetType,
        teacher.wifiName, teacher.wifiPassword, teacher.projectionType, teacher.zoomId,
        teacher.zoomPassword, teacher.notes};
}

TeacherRepository* teachers(ApplicationServices& services)
{
    return services.dataService()->databaseSession()->teacherRepository();
}

Result<ClassTransferPackage> buildPackage(QTemporaryDir& directory, const Teacher& teacher)
{
    ApplicationServices source;
    const auto opened = source.openDatabase(directory.filePath(QStringLiteral("source.tps")));
    if (!opened)
        return std::unexpected(opened.error());
    const auto teacherId = teachers(source)->createTeacher(teacher);
    if (!teacherId)
        return std::unexpected(teacherId.error());
    const auto classId = source.classService()->create(QStringLiteral("Imported Class"));
    if (!classId)
        return std::unexpected(classId.error());
    const auto loaded = source.classService()->classInfo(*classId);
    if (!loaded)
        return std::unexpected(loaded.error());
    ClassInfo info = *loaded;
    info.teacherId = *teacherId;
    info.classGrade = QStringLiteral("E4");
    info.classLevel = QStringLiteral("Theseus");
    const auto saved = source.classService()->saveClassInfo(info);
    if (!saved)
        return std::unexpected(saved.error());
    auto package = source.classService()->buildTransferPackage({*classId});
    if (package)
        package->classes.first().info.classId = *classId;
    return package;
}

template<class Dialog>
std::unique_ptr<Dialog> reviewDialog(ApplicationServices& services,
    const ClassTransferPackage& package, const ClassImportPreview& preview)
{
    if constexpr (std::is_constructible_v<Dialog, ApplicationServices*,
                      const ClassTransferPackage&, const ClassImportPreview&>)
        return std::make_unique<Dialog>(&services, package, preview);
    else
    {
        static_assert(std::is_constructible_v<Dialog, ClassService*, TeacherService*,
            const ClassTransferPackage&, const ClassImportPreview&>);
        return std::make_unique<Dialog>(services.classService(), services.teacherService(), package, preview);
    }
}

QString actionName(const int action)
{
    if (action == static_cast<int>(TeacherImportAction::Create))
        return QStringLiteral("create");
    if (action == static_cast<int>(TeacherImportAction::KeepExisting))
        return QStringLiteral("keep");
    if (action == static_cast<int>(TeacherImportAction::ReplaceExisting))
        return QStringLiteral("replace");
    return QStringLiteral("unresolved");
}

QJsonArray options(QComboBox& choice, const QMap<int, QString>& roles)
{
    QJsonArray snapshot;
    for (int index = 0; index < choice.count(); ++index)
    {
        const int target = choice.itemData(index, Qt::UserRole + 1).toInt();
        snapshot.append(QJsonObject{{QStringLiteral("label"), choice.itemText(index)},
            {QStringLiteral("action"), actionName(choice.itemData(index, Qt::UserRole).toInt())},
            {QStringLiteral("target"), target > 0 ? roles.value(target, QStringLiteral("unexpected")) : QStringLiteral("none")}});
    }
    return snapshot;
}

void logTranscript(const QJsonObject& transcript)
{
    const QString json = QString::fromUtf8(QJsonDocument(transcript).toJson(QJsonDocument::Compact));
    QString ascii;
    for (const QChar character : json)
    {
        if (character.unicode() > 0x7F)
            ascii += QStringLiteral("\\u%1").arg(static_cast<unsigned int>(character.unicode()), 4, 16, QLatin1Char('0'));
        else
            ascii += character;
    }
    qInfo().noquote() << "F352_TRANSCRIPT" << ascii;
}
}

class ClassImportTeacherChoiceParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void unmatchedTeacherCreatesAndAssignsOneTeacher();
    void singleMatchKeepsOrReplacesWithoutDuplicateTeacher_data();
    void singleMatchKeepsOrReplacesWithoutDuplicateTeacher();
    void ambiguousMatchesDisplayInPreviewOrderAndApplySelectedTeacher();
};

void ClassImportTeacherChoiceParityTests::unmatchedTeacherCreatesAndAssignsOneTeacher()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto package = buildPackage(directory, incomingTeacher());
    QVERIFY(package);
    QCOMPARE(package->teachers.size(), 1);
    QCOMPARE(package->classes.size(), 1);
    ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("destination.tps"))));
    const auto preview = services.classService()->previewImport(*package);
    QVERIFY(preview);
    QCOMPARE(preview->teachers.size(), 1);
    QVERIFY(preview->teachers.first().matchingTeacherIds.isEmpty());
    auto dialog = reviewDialog<ClassImportDialog>(services, *package, *preview);
    auto* choice = dialog->findChild<QComboBox*>(QStringLiteral("teacherImportChoice_%1").arg(package->teachers.first().key));
    auto* button = dialog->findChild<QPushButton*>(QStringLiteral("importClassesButton"));
    auto* message = dialog->findChild<QLabel*>(QStringLiteral("importValidationLabel"));
    QVERIFY(choice && button && message);
    QCOMPARE(choice->count(), 1);
    QCOMPARE(choice->itemText(0), QStringLiteral("Create new teacher"));
    QCOMPARE(choice->itemData(0, Qt::UserRole).toInt(), static_cast<int>(TeacherImportAction::Create));
    QCOMPARE(choice->itemData(0, Qt::UserRole + 1).toInt(), -1);
    QVERIFY(button->isEnabled());
    QVERIFY(message->text().isEmpty());
    const auto plan = dialog->importPlan();
    QCOMPARE(plan.classes.size(), 1);
    QCOMPARE(plan.classes.first().action, ClassImportAction::Create);
    QCOMPARE(plan.teachers.size(), 1);
    QCOMPARE(plan.teachers.first().action, TeacherImportAction::Create);
    const auto imported = services.classService()->importClasses(*package, plan);
    QVERIFY(imported);
    QCOMPARE(imported->createdClassIds.size(), 1);
    QVERIFY(imported->replacedClassIds.isEmpty());
    QCOMPARE(imported->skippedClassCount, 0);
    const auto storedTeachers = teachers(services)->getAllTeachers();
    const auto storedClasses = services.classService()->classes();
    const auto storedInfo = services.classService()->classInfo(imported->createdClassIds.first());
    QVERIFY(storedTeachers && storedClasses && storedInfo);
    QCOMPARE(storedTeachers->size(), 1);
    QCOMPARE(storedClasses->size(), 1);
    QCOMPARE(storedInfo->teacherId, storedTeachers->first().id);
    QCOMPARE(profileFields(storedTeachers->first()), profileFields(package->teachers.first().teacher));
    logTranscript({{QStringLiteral("case"), QStringLiteral("no-match")},
        {QStringLiteral("choices"), options(*choice, {})}, {QStringLiteral("enabled"), button->isEnabled()},
        {QStringLiteral("message"), message->text()}, {QStringLiteral("classes"), storedClasses->size()},
        {QStringLiteral("teachers"), storedTeachers->size()}, {QStringLiteral("assigned-role"), QStringLiteral("created")},
        {QStringLiteral("profile"), QJsonArray::fromStringList(profileFields(storedTeachers->first()))}});
}

void ClassImportTeacherChoiceParityTests::singleMatchKeepsOrReplacesWithoutDuplicateTeacher_data()
{
    QTest::addColumn<bool>("replace");
    QTest::newRow("keep") << false;
    QTest::newRow("replace") << true;
}

void ClassImportTeacherChoiceParityTests::singleMatchKeepsOrReplacesWithoutDuplicateTeacher()
{
    QFETCH(bool, replace);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto package = buildPackage(directory, incomingTeacher());
    QVERIFY(package);
    ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("destination.tps"))));
    const Teacher local = localTeacher();
    const auto localId = teachers(services)->createTeacher(local);
    QVERIFY(localId);
    const auto preview = services.classService()->previewImport(*package);
    QVERIFY(preview);
    QCOMPARE(preview->teachers.first().matchingTeacherIds, QList<int>({*localId}));
    auto dialog = reviewDialog<ClassImportDialog>(services, *package, *preview);
    auto* choice = dialog->findChild<QComboBox*>(QStringLiteral("teacherImportChoice_%1").arg(package->teachers.first().key));
    auto* button = dialog->findChild<QPushButton*>(QStringLiteral("importClassesButton"));
    auto* message = dialog->findChild<QLabel*>(QStringLiteral("importValidationLabel"));
    QVERIFY(choice && button && message);
    QCOMPARE(choice->count(), 2);
    QCOMPARE(choice->itemText(0), QStringLiteral("Keep local: Local Teacher"));
    QCOMPARE(choice->itemText(1), QStringLiteral("Replace local: Local Teacher"));
    QCOMPARE(choice->itemData(0, Qt::UserRole).toInt(), static_cast<int>(TeacherImportAction::KeepExisting));
    QCOMPARE(choice->itemData(1, Qt::UserRole).toInt(), static_cast<int>(TeacherImportAction::ReplaceExisting));
    QCOMPARE(choice->itemData(0, Qt::UserRole + 1).toInt(), *localId);
    QCOMPARE(choice->itemData(1, Qt::UserRole + 1).toInt(), *localId);
    QCOMPARE(choice->currentIndex(), 0);
    QVERIFY(button->isEnabled());
    QVERIFY(message->text().isEmpty());
    const auto defaultPlan = dialog->importPlan();
    QCOMPARE(defaultPlan.teachers.first().action, TeacherImportAction::KeepExisting);
    QCOMPARE(defaultPlan.teachers.first().targetTeacherId, *localId);
    choice->setCurrentIndex(replace ? 1 : 0);
    const auto plan = dialog->importPlan();
    QCOMPARE(plan.classes.size(), 1);
    QCOMPARE(plan.classes.first().action, ClassImportAction::Create);
    QCOMPARE(plan.teachers.size(), 1);
    QCOMPARE(plan.teachers.first().action, replace ? TeacherImportAction::ReplaceExisting : TeacherImportAction::KeepExisting);
    QCOMPARE(plan.teachers.first().targetTeacherId, *localId);
    const auto imported = services.classService()->importClasses(*package, plan);
    QVERIFY(imported);
    QCOMPARE(imported->createdClassIds.size(), 1);
    QVERIFY(imported->replacedClassIds.isEmpty());
    QCOMPARE(imported->skippedClassCount, 0);
    const auto storedTeachers = teachers(services)->getAllTeachers();
    const auto storedClasses = services.classService()->classes();
    const auto stored = teachers(services)->getTeacher(*localId);
    const auto info = services.classService()->classInfo(imported->createdClassIds.first());
    QVERIFY(storedTeachers && storedClasses && stored && info);
    QCOMPARE(storedTeachers->size(), 1);
    QCOMPARE(storedClasses->size(), 1);
    QCOMPARE(stored->id, *localId);
    QCOMPARE(info->teacherId, *localId);
    QCOMPARE(profileFields(*stored), profileFields(replace ? package->teachers.first().teacher : local));
    logTranscript({{QStringLiteral("case"), replace ? QStringLiteral("single-match-replace") : QStringLiteral("single-match-keep")},
        {QStringLiteral("choices"), options(*choice, {{*localId, QStringLiteral("local")}})},
        {QStringLiteral("default-action"), actionName(static_cast<int>(defaultPlan.teachers.first().action))},
        {QStringLiteral("selected-action"), actionName(static_cast<int>(plan.teachers.first().action))},
        {QStringLiteral("enabled"), button->isEnabled()}, {QStringLiteral("message"), message->text()},
        {QStringLiteral("classes"), storedClasses->size()}, {QStringLiteral("teachers"), storedTeachers->size()},
        {QStringLiteral("assigned-role"), QStringLiteral("local")},
        {QStringLiteral("profile"), QJsonArray::fromStringList(profileFields(*stored))}});
}

void ClassImportTeacherChoiceParityTests::ambiguousMatchesDisplayInPreviewOrderAndApplySelectedTeacher()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Teacher incoming = incomingTeacher();
    incoming.teacherEn.clear();
    const auto package = buildPackage(directory, incoming);
    QVERIFY(package);
    ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("destination.tps"))));
    Teacher preferred = localTeacher();
    preferred.teacherEn = QStringLiteral("English Local");
    preferred.preferredRomanization = QStringLiteral("Preferred Local");
    preferred.preferredName = preferred.preferredRomanization;
    Teacher romanized = localTeacher();
    romanized.teacherEn.clear();
    romanized.preferredRomanization = QStringLiteral("Romanized Local");
    romanized.preferredName.clear();
    romanized.notes = QStringLiteral("Romanized local profile");
    const auto romanizedId = teachers(services)->createTeacher(romanized);
    const auto preferredId = teachers(services)->createTeacher(preferred);
    QVERIFY(preferredId && romanizedId);
    const auto preview = services.classService()->previewImport(*package);
    QVERIFY(preview);
    QCOMPARE(preview->teachers.size(), 1);
    QCOMPARE(preview->teachers.first().matchingTeacherIds, QList<int>({*romanizedId, *preferredId}));
    auto dialog = reviewDialog<ClassImportDialog>(services, *package, *preview);
    auto* choice = dialog->findChild<QComboBox*>(QStringLiteral("teacherImportChoice_%1").arg(package->teachers.first().key));
    auto* button = dialog->findChild<QPushButton*>(QStringLiteral("importClassesButton"));
    auto* message = dialog->findChild<QLabel*>(QStringLiteral("importValidationLabel"));
    QVERIFY(choice && button && message);
    QCOMPARE(choice->count(), 6);
    QCOMPARE(choice->itemText(0), QStringLiteral("Choose a teacher resolution\u2026"));
    QCOMPARE(choice->itemText(1), QStringLiteral("Create new teacher"));
    QCOMPARE(choice->itemText(2), QStringLiteral("Keep local: Romanized Local"));
    QCOMPARE(choice->itemText(3), QStringLiteral("Replace local: Romanized Local"));
    QCOMPARE(choice->itemText(4), QStringLiteral("Keep local: Preferred Local"));
    QCOMPARE(choice->itemText(5), QStringLiteral("Replace local: Preferred Local"));
    QCOMPARE(choice->itemData(0, Qt::UserRole).toInt(), -1);
    QCOMPARE(choice->itemData(0, Qt::UserRole + 1).toInt(), -1);
    QCOMPARE(choice->itemData(1, Qt::UserRole).toInt(), static_cast<int>(TeacherImportAction::Create));
    QCOMPARE(choice->itemData(1, Qt::UserRole + 1).toInt(), -1);
    for (int index = 2; index < 6; ++index)
    {
        QCOMPARE(choice->itemData(index, Qt::UserRole).toInt(),
            static_cast<int>(index % 2 == 0 ? TeacherImportAction::KeepExisting : TeacherImportAction::ReplaceExisting));
        QCOMPARE(choice->itemData(index, Qt::UserRole + 1).toInt(), index < 4 ? *romanizedId : *preferredId);
    }
    QCOMPARE(choice->currentIndex(), 0);
    QVERIFY(!button->isEnabled());
    const QString initialMessage = message->text();
    QCOMPARE(initialMessage, QStringLiteral("Choose a resolution for every ambiguous teacher match."));
    choice->setCurrentIndex(4);
    QVERIFY(button->isEnabled());
    QVERIFY(message->text().isEmpty());
    const auto plan = dialog->importPlan();
    QCOMPARE(plan.classes.size(), 1);
    QCOMPARE(plan.classes.first().action, ClassImportAction::Create);
    QCOMPARE(plan.teachers.size(), 1);
    QCOMPARE(plan.teachers.first().action, TeacherImportAction::KeepExisting);
    QCOMPARE(plan.teachers.first().targetTeacherId, *preferredId);
    const auto imported = services.classService()->importClasses(*package, plan);
    QVERIFY(imported);
    QCOMPARE(imported->createdClassIds.size(), 1);
    QVERIFY(imported->replacedClassIds.isEmpty());
    QCOMPARE(imported->skippedClassCount, 0);
    const auto storedTeachers = teachers(services)->getAllTeachers();
    const auto storedClasses = services.classService()->classes();
    const auto info = services.classService()->classInfo(imported->createdClassIds.first());
    const auto storedPreferred = teachers(services)->getTeacher(*preferredId);
    const auto storedRomanized = teachers(services)->getTeacher(*romanizedId);
    QVERIFY(storedTeachers && storedClasses && info && storedPreferred && storedRomanized);
    QCOMPARE(storedTeachers->size(), 2);
    QCOMPARE(storedClasses->size(), 1);
    QCOMPARE(info->teacherId, *preferredId);
    QCOMPARE(profileFields(*storedPreferred), profileFields(preferred));
    QCOMPARE(profileFields(*storedRomanized), profileFields(romanized));
    logTranscript({{QStringLiteral("case"), QStringLiteral("ambiguous-matches")},
        {QStringLiteral("choices"), options(*choice, {{*preferredId, QStringLiteral("preferred")}, {*romanizedId, QStringLiteral("romanized")}})},
        {QStringLiteral("initial-enabled"), false}, {QStringLiteral("initial-message"), initialMessage},
        {QStringLiteral("selected-action"), actionName(static_cast<int>(plan.teachers.first().action))},
        {QStringLiteral("enabled"), button->isEnabled()}, {QStringLiteral("message"), message->text()},
        {QStringLiteral("classes"), storedClasses->size()}, {QStringLiteral("teachers"), storedTeachers->size()},
        {QStringLiteral("assigned-role"), QStringLiteral("preferred")},
        {QStringLiteral("preferred-profile"), QJsonArray::fromStringList(profileFields(*storedPreferred))},
        {QStringLiteral("romanized-profile"), QJsonArray::fromStringList(profileFields(*storedRomanized))}});
}

QTEST_MAIN(ClassImportTeacherChoiceParityTests)

#include "class_import_teacher_choice_parity_tests.moc"
