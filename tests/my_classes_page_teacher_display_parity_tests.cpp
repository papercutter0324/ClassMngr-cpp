#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "features/my_info/ui/my_classes_page.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <QCoreApplication>
#include <QEvent>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QtTest/QtTest>

namespace
{
struct Fixture
{
    QTemporaryDir directory;
    ApplicationServices services;

    bool open()
    {
        return directory.isValid()
            && services.openDatabase(directory.filePath(QStringLiteral("teacher-display.tps")));
    }

    TeacherRepository* teachers()
    {
        return services.dataService()->databaseSession()->teacherRepository();
    }

    int createClass(const QString& name, const int teacherId)
    {
        const auto created = services.classService()->create(name);
        if (!created)
            return -1;
        const auto loaded = services.classService()->classInfo(*created);
        if (!loaded)
            return -1;
        ClassInfo info = *loaded;
        info.classGrade = QStringLiteral("E4");
        info.classLevel = QStringLiteral("Theseus");
        info.teacherId = teacherId;
        return services.classService()->saveClassInfo(info) ? *created : -1;
    }
};

NavigationTabWidget* tabsFor(MyClassesPage& page)
{
    const auto tabs = page.findChildren<NavigationTabWidget*>(QStringLiteral("myInfoClassTabs"));
    return tabs.size() == 1 ? tabs.first() : nullptr;
}

int indexFor(NavigationTabWidget& tabs, const int classId)
{
    for (int index = 0; index < tabs.count(); ++index)
        if (tabs.widget(index)->property("class_id").toInt() == classId)
            return index;
    return -1;
}

QLineEdit* profileEdit(QWidget& page, const QString& labelText)
{
    for (auto* grid : page.findChildren<QGridLayout*>())
        for (int row = 0; row < grid->rowCount(); ++row)
            for (int column = 0; column < grid->columnCount(); ++column)
            {
                auto* item = grid->itemAtPosition(row, column);
                auto* label = item ? qobject_cast<QLabel*>(item->widget()) : nullptr;
                if (!label || label->text() != labelText)
                    continue;
                auto* value = grid->itemAtPosition(row + 1, column);
                return value ? qobject_cast<QLineEdit*>(value->widget()) : nullptr;
            }
    return nullptr;
}

struct Profile
{
    QString heading;
    QStringList fields;
    QString notes;
    bool complete = false;
    bool readOnly = false;

    QJsonObject transcript(const QString& role) const
    {
        return {{QStringLiteral("role"), role}, {QStringLiteral("heading"), heading},
            {QStringLiteral("fields"), QJsonArray::fromStringList(fields)},
            {QStringLiteral("notes"), notes}, {QStringLiteral("read-only"), readOnly}};
    }
};

Profile profileFor(QWidget& page)
{
    Profile profile;
    const auto headings = page.findChildren<QLabel*>(QStringLiteral("sectionTitle"), Qt::FindDirectChildrenOnly);
    if (headings.size() != 1 || page.findChildren<QLineEdit*>().size() != 6)
        return profile;
    profile.heading = headings.first()->text();
    profile.readOnly = true;
    QWidget* teacherCard = nullptr;
    for (const auto& label : {QStringLiteral("Internet Type"), QStringLiteral("WiFi Name"),
             QStringLiteral("WiFi Password"), QStringLiteral("Projection Type"),
             QStringLiteral("Zoom ID"), QStringLiteral("Zoom Password")})
    {
        auto* edit = profileEdit(page, label);
        if (!edit)
            return profile;
        profile.fields.append(edit->text());
        profile.readOnly = profile.readOnly && edit->isReadOnly();
        teacherCard = edit->parentWidget();
    }
    const auto notes = teacherCard->findChildren<QTextEdit*>();
    if (notes.size() != 1)
        return profile;
    profile.notes = notes.first()->toPlainText();
    profile.readOnly = profile.readOnly && notes.first()->isReadOnly();
    profile.complete = true;
    return profile;
}

Teacher fullTeacher(const QString& englishName)
{
    Teacher teacher;
    teacher.teacherEn = englishName;
    teacher.internetType = QStringLiteral("WiFi");
    teacher.wifiName = QStringLiteral("Class WiFi");
    teacher.wifiPassword = QStringLiteral("WiFi key");
    teacher.projectionType = QStringLiteral("HDMI");
    teacher.zoomId = QStringLiteral("class.zoom");
    teacher.zoomPassword = QStringLiteral("Zoom key");
    teacher.notes = QStringLiteral("Teacher notes");
    return teacher;
}

QString baseTab()
{
    return QStringLiteral("E4 Theseus \u2022 No time");
}

QString normalizedTab(QString label, const int classId, const QString& role)
{
    return label.replace(QStringLiteral("#%1").arg(classId), QStringLiteral("#%1").arg(role));
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
    qInfo().noquote() << "F351_TRANSCRIPT" << ascii;
}

void refresh(MyClassesPage& page)
{
    page.refresh();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}
}

class MyClassesPageTeacherDisplayParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void assignedProfilePreservesHeadingFieldsNotesAndReadOnlyState();
    void sharedTeacherAndKoreanFallbackKeepProfilesAndDisambiguateTabs();
    void refreshedProfileKeepsSelectedClassAndClassCount();
};

void MyClassesPageTeacherDisplayParityTests::assignedProfilePreservesHeadingFieldsNotesAndReadOnlyState()
{
    Fixture fixture;
    QVERIFY(fixture.open());
    Teacher teacher = fullTeacher(QStringLiteral("English Teacher"));
    teacher.preferredRomanization = QStringLiteral("Romanized Teacher");
    teacher.preferredName = QStringLiteral("  Romanized Teacher  ");
    teacher.roomNumber = QStringLiteral("  405 \uAC15\uB0A8  ");
    teacher.internetType = QStringLiteral("Both");
    teacher.wifiName = QStringLiteral("  \uD559\uAE09 WiFi \U0001F4F6  ");
    teacher.wifiPassword = QStringLiteral("   ");
    teacher.projectionType = QStringLiteral("Zoom");
    teacher.zoomId = QStringLiteral("  zoom.\uD68C\uC758  ");
    teacher.zoomPassword = QString();
    teacher.notes = QStringLiteral("  \uAD50\uC0AC \uBA54\uBAA8\nUTF-16 \U0001F9ED\nLast line  ");
    const auto teacherId = fixture.teachers()->createTeacher(teacher);
    QVERIFY(teacherId);
    const int classId = fixture.createClass(QStringLiteral("Assigned class"), *teacherId);
    QVERIFY(classId > 0);
    MyClassesPage page(&fixture.services);
    refresh(page);
    auto* tabs = tabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);
    QCOMPARE(tabs->currentWidget()->property("class_id").toInt(), classId);
    const auto profile = profileFor(*tabs->widget(0));
    QVERIFY(profile.complete);
    QVERIFY(profile.readOnly);
    QCOMPARE(profile.heading, QStringLiteral("Romanized Teacher - Room 405 \uAC15\uB0A8"));
    QCOMPARE(profile.fields, QStringList({QStringLiteral("Both"), QStringLiteral("\uD559\uAE09 WiFi \U0001F4F6"),
        QStringLiteral("N/A"), QStringLiteral("Zoom"), QStringLiteral("zoom.\uD68C\uC758"), QStringLiteral("N/A")}));
    QCOMPARE(profile.notes, teacher.notes);
    QCOMPARE(tabs->tabText(0), baseTab());
    logTranscript({{QStringLiteral("case"), QStringLiteral("assigned-profile")},
        {QStringLiteral("classes"), tabs->count()}, {QStringLiteral("tab"), tabs->tabText(0)},
        {QStringLiteral("profile"), profile.transcript(QStringLiteral("assigned"))}});
}

void MyClassesPageTeacherDisplayParityTests::sharedTeacherAndKoreanFallbackKeepProfilesAndDisambiguateTabs()
{
    Fixture fixture;
    QVERIFY(fixture.open());
    const Teacher english = fullTeacher(QStringLiteral("English Teacher"));
    Teacher korean = fullTeacher(QString());
    korean.teacherKr = QStringLiteral("\uD55C\uAD6D\uC5B4 \uC774\uB984");
    korean.preferredRomanization = QStringLiteral("Romanized \uC774\uB984");
    korean.internetType = QStringLiteral("LAN");
    korean.wifiName = QStringLiteral("Korean WiFi");
    korean.wifiPassword = QStringLiteral("Korean key");
    korean.projectionType = QStringLiteral("Zoom");
    korean.zoomId = QStringLiteral("korean.zoom");
    korean.zoomPassword = QStringLiteral("Korean Zoom key");
    korean.notes = QStringLiteral("\uAD50\uC0AC \uBA54\uBAA8\nKorean profile");
    const auto englishId = fixture.teachers()->createTeacher(english);
    const auto koreanId = fixture.teachers()->createTeacher(korean);
    QVERIFY(englishId && koreanId);
    const int firstId = fixture.createClass(QStringLiteral("Alpha class"), *englishId);
    const int koreanClassId = fixture.createClass(QStringLiteral("Beta class"), *koreanId);
    const int sharedId = fixture.createClass(QStringLiteral("Gamma class"), *englishId);
    QVERIFY(firstId > 0 && koreanClassId > 0 && sharedId > 0);
    MyClassesPage page(&fixture.services);
    refresh(page);
    auto* tabs = tabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 3);
    QCOMPARE(indexFor(*tabs, firstId), 0);
    QCOMPARE(indexFor(*tabs, koreanClassId), 1);
    QCOMPARE(indexFor(*tabs, sharedId), 2);
    const QStringList tabLabels{
        normalizedTab(tabs->tabText(0), firstId, QStringLiteral("first")),
        normalizedTab(tabs->tabText(1), koreanClassId, QStringLiteral("korean")),
        normalizedTab(tabs->tabText(2), sharedId, QStringLiteral("shared"))};
    QCOMPARE(tabLabels, QStringList({baseTab() + QStringLiteral(" \u2022 English Teacher #first"),
        baseTab() + QStringLiteral(" \u2022 ") + korean.teacherKr,
        baseTab() + QStringLiteral(" \u2022 English Teacher #shared")}));
    QJsonArray profiles;
    const QStringList roles{QStringLiteral("first"), QStringLiteral("korean"), QStringLiteral("shared")};
    for (int index = 0; index < tabs->count(); ++index)
    {
        tabs->setCurrentIndex(index);
        const auto profile = profileFor(*tabs->widget(index));
        const Teacher& expected = index == 1 ? korean : english;
        QVERIFY(profile.complete);
        QVERIFY(profile.readOnly);
        QCOMPARE(profile.heading, index == 1 ? korean.preferredRomanization : english.teacherEn);
        QCOMPARE(profile.fields, QStringList({expected.internetType, expected.wifiName,
            expected.wifiPassword, expected.projectionType, expected.zoomId, expected.zoomPassword}));
        QCOMPARE(profile.notes, expected.notes);
        profiles.append(profile.transcript(roles.at(index)));
    }
    logTranscript({{QStringLiteral("case"), QStringLiteral("shared-and-fallback")},
        {QStringLiteral("classes"), tabs->count()}, {QStringLiteral("tabs"), QJsonArray::fromStringList(tabLabels)},
        {QStringLiteral("profiles"), profiles}});
}

void MyClassesPageTeacherDisplayParityTests::refreshedProfileKeepsSelectedClassAndClassCount()
{
    Fixture fixture;
    QVERIFY(fixture.open());
    const Teacher siblingTeacher = fullTeacher(QStringLiteral("Sibling Teacher"));
    Teacher targetTeacher = fullTeacher(QStringLiteral("Target Teacher"));
    const auto siblingTeacherId = fixture.teachers()->createTeacher(siblingTeacher);
    const auto targetTeacherId = fixture.teachers()->createTeacher(targetTeacher);
    QVERIFY(siblingTeacherId && targetTeacherId);
    const int siblingId = fixture.createClass(QStringLiteral("Sibling class"), *siblingTeacherId);
    const int targetId = fixture.createClass(QStringLiteral("Target class"), *targetTeacherId);
    QVERIFY(siblingId > 0 && targetId > 0);
    MyClassesPage page(&fixture.services);
    refresh(page);
    auto* tabs = tabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 2);
    const int targetIndex = indexFor(*tabs, targetId);
    QVERIFY(targetIndex >= 0);
    tabs->setCurrentIndex(targetIndex);
    QCOMPARE(tabs->currentWidget()->property("class_id").toInt(), targetId);
    const auto initial = profileFor(*tabs->currentWidget());
    QVERIFY(initial.complete && initial.readOnly);
    QCOMPARE(initial.heading, targetTeacher.teacherEn);
    QCOMPARE(initial.notes, targetTeacher.notes);

    targetTeacher.id = *targetTeacherId;
    targetTeacher.teacherEn = QStringLiteral("Updated Teacher");
    targetTeacher.preferredRomanization = QStringLiteral("Updated Romanization");
    targetTeacher.preferredName = QStringLiteral("  Updated Romanization  ");
    targetTeacher.roomNumber = QStringLiteral("  507  ");
    targetTeacher.internetType = QStringLiteral("LAN");
    targetTeacher.wifiName = QStringLiteral("Updated WiFi");
    targetTeacher.wifiPassword = QStringLiteral("Updated key");
    targetTeacher.projectionType = QStringLiteral("Any");
    targetTeacher.zoomId = QStringLiteral("updated.zoom");
    targetTeacher.zoomPassword = QStringLiteral("Updated Zoom key");
    targetTeacher.notes = QStringLiteral("Updated \uBA54\uBAA8\nUTF-16 \U0001F393");
    QVERIFY(fixture.teachers()->updateTeacher(targetTeacher));
    refresh(page);
    tabs = tabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 2);
    QCOMPARE(tabs->currentWidget()->property("class_id").toInt(), targetId);
    const auto updated = profileFor(*tabs->currentWidget());
    QVERIFY(updated.complete && updated.readOnly);
    QCOMPARE(updated.heading, QStringLiteral("Updated Romanization - Room 507"));
    QCOMPARE(updated.fields, QStringList({targetTeacher.internetType, targetTeacher.wifiName,
        targetTeacher.wifiPassword, targetTeacher.projectionType, targetTeacher.zoomId, targetTeacher.zoomPassword}));
    QCOMPARE(updated.notes, targetTeacher.notes);
    QVERIFY(updated.heading != initial.heading);
    const int siblingIndex = indexFor(*tabs, siblingId);
    QVERIFY(siblingIndex >= 0);
    tabs->setCurrentIndex(siblingIndex);
    const auto sibling = profileFor(*tabs->widget(siblingIndex));
    QVERIFY(sibling.complete && sibling.readOnly);
    QCOMPARE(sibling.heading, siblingTeacher.teacherEn);
    QCOMPARE(sibling.fields, QStringList({siblingTeacher.internetType, siblingTeacher.wifiName,
        siblingTeacher.wifiPassword, siblingTeacher.projectionType, siblingTeacher.zoomId, siblingTeacher.zoomPassword}));
    QCOMPARE(sibling.notes, siblingTeacher.notes);
    tabs->setCurrentIndex(targetIndex);
    const auto classes = fixture.services.classService()->classes();
    QVERIFY(classes);
    QCOMPARE(classes->size(), 2);
    const QString selectedTab = normalizedTab(tabs->tabText(tabs->currentIndex()), targetId, QStringLiteral("target"));
    QCOMPARE(selectedTab, baseTab() + QStringLiteral(" \u2022 Updated Teacher"));
    logTranscript({{QStringLiteral("case"), QStringLiteral("refresh-selected-profile")},
        {QStringLiteral("classes"), classes->size()}, {QStringLiteral("selected-role"), QStringLiteral("target")},
        {QStringLiteral("selected-tab"), selectedTab}, {QStringLiteral("profile"), updated.transcript(QStringLiteral("target"))},
        {QStringLiteral("sibling"), sibling.transcript(QStringLiteral("sibling"))}});
}

QTEST_MAIN(MyClassesPageTeacherDisplayParityTests)

#include "my_classes_page_teacher_display_parity_tests.moc"
