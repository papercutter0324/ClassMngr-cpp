#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/class_info.h"
#include "domain/models/roster.h"
#include "features/my_info/ui/my_classes_page.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>
#include <QUuid>
#include <QtTest/QtTest>

#include <utility>

namespace
{

class MyClassesPageFixture final
{
public:
    QTemporaryDir directory;
    ApplicationServices services;

    bool initialize(QString* error)
    {
        if (!directory.isValid())
        {
            *error = QStringLiteral("Temporary directory is invalid.");
            return false;
        }

        const QString path = directory.filePath(
            QStringLiteral("my-classes-page-%1.tps").arg(
                QUuid::createUuid().toString(QUuid::WithoutBraces)
                )
            );
        const Status opened = services.openDatabase(path);
        if (!opened)
        {
            *error = opened.error();
            return false;
        }

        return true;
    }

    bool createClass(
        const QString& name,
        const QString& grade,
        const QString& level,
        int* classId,
        QString* error
        )
    {
        const auto created = services.classService()->create(name);
        if (!created)
        {
            *error = created.error();
            return false;
        }

        ClassInfo info;
        info.classId = *created;
        info.classGrade = grade;
        info.classLevel = level;
        const Status saved = services.classService()->saveClassInfo(info);
        if (!saved)
        {
            *error = saved.error();
            return false;
        }

        *classId = *created;
        return true;
    }

    bool createTeacher(
        const Teacher& teacher,
        int* teacherId,
        QString* error
        )
    {
        TeacherRepository* const repository =
            services.databaseSession()
                ? services.databaseSession()->teacherRepository()
                : nullptr;
        if (!repository)
        {
            *error = QStringLiteral("Teacher repository is unavailable.");
            return false;
        }

        const auto created = repository->createTeacher(teacher);
        if (!created)
        {
            *error = created.error();
            return false;
        }

        *teacherId = *created;
        return true;
    }

    bool assignTeacher(
        const int classId,
        const int teacherId,
        QString* error
        )
    {
        const auto loaded = services.classService()->classInfo(classId);
        if (!loaded)
        {
            *error = loaded.error();
            return false;
        }

        ClassInfo info = loaded.value();
        info.teacherId = teacherId;
        const Status saved = services.classService()->saveClassInfo(info);
        if (!saved)
        {
            *error = saved.error();
            return false;
        }
        return true;
    }
};

NavigationTabWidget* classTabsFor(MyClassesPage& page)
{
    const QList<NavigationTabWidget*> tabs =
        page.findChildren<NavigationTabWidget*>(
            QStringLiteral("myInfoClassTabs")
            );
    return tabs.size() == 1 ? tabs.constFirst() : nullptr;
}

QList<QLabel*> labelsWithText(
    QWidget& root,
    const QString& text
    )
{
    QList<QLabel*> matches;
    for (QLabel* label : root.findChildren<QLabel*>())
    {
        if (label->text() == text)
        {
            matches.append(label);
        }
    }
    return matches;
}

QList<QLineEdit*> lineEditsWithText(
    QWidget& root,
    const QString& text
    )
{
    QList<QLineEdit*> matches;
    for (QLineEdit* edit : root.findChildren<QLineEdit*>())
    {
        if (edit->text() == text)
        {
            matches.append(edit);
        }
    }
    return matches;
}

QList<QTextEdit*> textEditsWithContent(
    QWidget& root,
    const QString& text
    )
{
    QList<QTextEdit*> matches;
    for (QTextEdit* edit : root.findChildren<QTextEdit*>())
    {
        if (edit->toPlainText() == text)
        {
            matches.append(edit);
        }
    }
    return matches;
}

int tabIndexForClass(
    NavigationTabWidget& tabs,
    const int classId
    )
{
    for (int index = 0; index < tabs.count(); ++index)
    {
        QWidget* const page = tabs.widget(index);
        if (page && page->property("class_id").toInt() == classId)
        {
            return index;
        }
    }
    return -1;
}

QString classTabLabel(
    const QString& grade,
    const QString& level
    )
{
    return grade + QLatin1Char(' ') + level + QLatin1Char(' ')
        + QChar(0x2022) + QStringLiteral(" No time");
}

QLabel* infoRowValueLabelFor(
    QWidget& root,
    const QString& labelText
    )
{
    for (QGridLayout* const grid : root.findChildren<QGridLayout*>())
    {
        for (int row = 0; row < grid->rowCount(); ++row)
        {
            QLayoutItem* const labelItem = grid->itemAtPosition(row, 0);
            auto* const label = labelItem
                ? qobject_cast<QLabel*>(labelItem->widget())
                : nullptr;
            if (!label || label->text() != labelText)
            {
                continue;
            }

            QLayoutItem* const valueItem = grid->itemAtPosition(row, 1);
            return valueItem
                ? qobject_cast<QLabel*>(valueItem->widget())
                : nullptr;
        }
    }

    return nullptr;
}

Roster rosterWithNamedRows()
{
    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.columnWidths = {140, 140, 80, 100, 80, 80};
    roster.rows = {
        {QStringLiteral("Alice"), QStringLiteral("\uAE40\uBBFC\uC9C0")},
        {QStringLiteral("Charlie"), QStringLiteral("\uBC15\uC9C0\uC6D0")},
        {QStringLiteral("David"), QStringLiteral("\uC774\uC218\uC9C4")},
        {QStringLiteral("Eve"), QStringLiteral("\uD64D\uAE38\uB3D9")}
    };
    return roster;
}

bool saveRoster(
    MyClassesPageFixture& fixture,
    const int classId,
    const Roster& roster,
    QString* error
    )
{
    const Status saved = fixture.services.rosterService()->saveRoster(
        classId,
        roster
        );
    if (!saved)
    {
        *error = saved.error();
        return false;
    }
    return true;
}

bool updateRosterCell(
    MyClassesPageFixture& fixture,
    const int classId,
    const int rowIndex,
    const int columnIndex,
    const QString& value,
    QString* error
    )
{
    QSqlQuery update(fixture.services.databaseSession()->database());
    if (!update.prepare(QStringLiteral(
            "UPDATE roster_data SET value=? "
            "WHERE class_id=? AND row_index=? AND col_index=?")))
    {
        *error = update.lastError().text();
        return false;
    }

    update.addBindValue(value);
    update.addBindValue(classId);
    update.addBindValue(rowIndex);
    update.addBindValue(columnIndex);
    if (!update.exec())
    {
        *error = update.lastError().text();
        return false;
    }
    if (update.numRowsAffected() != 1)
    {
        *error = QStringLiteral("Expected to update one roster cell.");
        return false;
    }
    return true;
}

}

class MyClassesPageTests final : public QObject
{
    Q_OBJECT

private slots:
    void classesRenderInOrderWithTitlesAndSelectionRestoredById();
    void successfulEmptyListShowsEmptyState();
    void closedSessionRefreshIsQuietAndKeepsRenderedContent();
    void failedClassListReadClearsRenderedContentAndShowsWarning();
    void assignedTeacherProfileProjectsAllConsumedUtf16Fields();
    void classInformationFieldsAndTeacherAssociationUseTypedReads();
    void classInformationBatchFailureKeepsEachClassInListOrder();
    void failedTeacherProfileKeepsClassAndUsesSilentUnassignedFallback();
    void rosterCountUsesNonblankEnglishOrKoreanCells();
    void studentCountBatchRunsOnceAndKeepsClassOrder();
    void flatPageReentryRestoresDetailsWithoutSummaryRead();
    void groupedGradeSelectionMaterializesOnlySelectedDetails();
    void rosterReadFailureKeepsClassAndShowsZeroStudentCount();
    void rosterWithoutNameColumnsShowsZeroStudentCount();
};

void MyClassesPageTests::
assignedTeacherProfileProjectsAllConsumedUtf16Fields()
{
    MyClassesPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    Teacher englishTeacher;
    englishTeacher.teacherKr = QStringLiteral("\uD55C\uAD6D\uC5B4 \uAD50\uC0AC");
    englishTeacher.teacherEn = QStringLiteral("English Teacher");
    englishTeacher.preferredRomanization = QStringLiteral("Romanized Teacher");
    englishTeacher.preferredName = QStringLiteral("\uC120\uD638 \U0001F393");
    englishTeacher.roomNumber = QStringLiteral("Room 5 \uAC15\uB0A8");
    englishTeacher.internetType = QStringLiteral("Both");
    englishTeacher.wifiName = QStringLiteral("\uD559\uAE09 WiFi \U0001F4F6");
    englishTeacher.wifiPassword = QStringLiteral("\uBE44\uBC00 \U0001F511");
    englishTeacher.projectionType = QStringLiteral("Zoom");
    englishTeacher.zoomId = QStringLiteral("zoom.\uD68C\uC758");
    englishTeacher.zoomPassword = QStringLiteral("\uC554\uD638 \U0001F510");
    englishTeacher.notes = QStringLiteral(
        "\uAD50\uC0AC \uBA54\uBAA8\nUTF-16 \U0001F9ED");

    int englishTeacherId = 0;
    QVERIFY2(fixture.createTeacher(
                 englishTeacher,
                 &englishTeacherId,
                 &error
                 ), qPrintable(error));

    Teacher koreanTeacher;
    koreanTeacher.teacherKr = QStringLiteral("\uD55C\uAD6D\uC5B4 \uC774\uB984");
    koreanTeacher.preferredRomanization = QStringLiteral("Romanized \uC774\uB984");

    int koreanTeacherId = 0;
    QVERIFY2(fixture.createTeacher(
                 koreanTeacher,
                 &koreanTeacherId,
                 &error
                 ), qPrintable(error));

    int englishClassId = 0;
    int koreanClassId = 0;
    int secondEnglishClassId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Alpha class"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 &englishClassId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Beta class"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 &koreanClassId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Gamma class"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 &secondEnglishClassId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.assignTeacher(
                 englishClassId,
                 englishTeacherId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.assignTeacher(
                 koreanClassId,
                 koreanTeacherId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.assignTeacher(
                 secondEnglishClassId,
                 englishTeacherId,
                 &error
                 ), qPrintable(error));

    TeacherRepository* const teacherRepository =
        fixture.services.databaseSession()->teacherRepository();
    QVERIFY(teacherRepository);
    const MyClassesTeacherProfileBatchReadMetrics beforeProfiles =
        teacherRepository->myClassesTeacherProfileBatchReadMetrics();
    MyClassesPage page(&fixture.services);
    page.resize(900, 700);
    page.refresh();
    page.show();
    QApplication::processEvents();

    const MyClassesTeacherProfileBatchReadMetrics afterProfiles =
        teacherRepository->myClassesTeacherProfileBatchReadMetrics();
    QCOMPARE(afterProfiles.callCount, beforeProfiles.callCount + 1);
    QCOMPARE(afterProfiles.requestedTeacherCount,
             beforeProfiles.requestedTeacherCount + 2);
    QCOMPARE(afterProfiles.statementCount, beforeProfiles.statementCount + 1);

    NavigationTabWidget* const tabs = classTabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 3);
    const int englishTabIndex = tabIndexForClass(*tabs, englishClassId);
    const int koreanTabIndex = tabIndexForClass(*tabs, koreanClassId);
    const int secondEnglishTabIndex =
        tabIndexForClass(*tabs, secondEnglishClassId);
    QVERIFY(englishTabIndex >= 0);
    QVERIFY(koreanTabIndex >= 0);
    QVERIFY(secondEnglishTabIndex >= 0);
    QVERIFY(englishTabIndex < koreanTabIndex);
    QVERIFY(koreanTabIndex < secondEnglishTabIndex);

    const QString baseLabel = classTabLabel(
        QStringLiteral("E4"), QStringLiteral("Theseus"));
    QCOMPARE(
        tabs->tabText(englishTabIndex),
        baseLabel + QLatin1Char(' ') + QChar(0x2022)
            + QLatin1Char(' ') + englishTeacher.teacherEn
            + QStringLiteral(" #%1").arg(englishClassId)
        );
    QCOMPARE(
        tabs->tabText(koreanTabIndex),
        baseLabel + QLatin1Char(' ') + QChar(0x2022)
            + QLatin1Char(' ') + koreanTeacher.teacherKr
        );
    QCOMPARE(
        tabs->tabText(secondEnglishTabIndex),
        baseLabel + QLatin1Char(' ') + QChar(0x2022)
            + QLatin1Char(' ') + englishTeacher.teacherEn
            + QStringLiteral(" #%1").arg(secondEnglishClassId)
        );

    tabs->setCurrentIndex(englishTabIndex);
    QWidget* const englishClassPage = tabs->widget(englishTabIndex);
    QVERIFY(englishClassPage);
    const QString expectedEnglishHeading =
        englishTeacher.preferredName + QStringLiteral(" - Room ")
        + englishTeacher.roomNumber;
    QCOMPARE(
        labelsWithText(*englishClassPage, expectedEnglishHeading).size(),
        1
        );
    for (const QString& expectedValue : {
             englishTeacher.internetType,
             englishTeacher.wifiName,
             englishTeacher.wifiPassword,
             englishTeacher.projectionType,
             englishTeacher.zoomId,
             englishTeacher.zoomPassword
         })
    {
        const QList<QLineEdit*> matchingEdits = lineEditsWithText(
            *englishClassPage, expectedValue);
        QCOMPARE(matchingEdits.size(), 1);
        QVERIFY(matchingEdits.constFirst()->isReadOnly());
    }
    const QList<QTextEdit*> notes = textEditsWithContent(
        *englishClassPage, englishTeacher.notes);
    QCOMPARE(notes.size(), 1);
    QVERIFY(notes.constFirst()->isReadOnly());

    tabs->setCurrentIndex(koreanTabIndex);
    QCOMPARE(
        labelsWithText(
            *tabs->widget(koreanTabIndex),
            koreanTeacher.preferredRomanization
            ).size(),
        1
        );

    tabs->setCurrentIndex(secondEnglishTabIndex);
    QCOMPARE(
        labelsWithText(
            *tabs->widget(secondEnglishTabIndex),
            expectedEnglishHeading
            ).size(),
        1
        );
    const QList<QTextEdit*> repeatedTeacherNotes = textEditsWithContent(
        *tabs->widget(secondEnglishTabIndex),
        englishTeacher.notes
        );
    QCOMPARE(repeatedTeacherNotes.size(), 1);
    QVERIFY(repeatedTeacherNotes.constFirst()->isReadOnly());
}

void MyClassesPageTests::
classInformationFieldsAndTeacherAssociationUseTypedReads()
{
    MyClassesPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Assigned My Classes Teacher");
    teacher.roomNumber = QStringLiteral("Room 12");
    int teacherId = 0;
    QVERIFY2(fixture.createTeacher(teacher, &teacherId, &error),
             qPrintable(error));

    int classId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Displayed details class"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 &classId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.assignTeacher(classId, teacherId, &error),
             qPrintable(error));

    auto loadedInfo = fixture.services.classService()->classInfo(classId);
    QVERIFY(loadedInfo);
    ClassInfo info = loadedInfo.value();
    info.classTimes = {
        {
            .day = QStringLiteral("Tuesday"),
            .startTime = QStringLiteral("11:05 am"),
            .endTime = QStringLiteral("12:05 pm")
        },
        {
            .day = QStringLiteral("Monday"),
            .startTime = QStringLiteral("9:00 AM"),
            .endTime = QStringLiteral("9:45 AM")
        }
    };
    info.intensiveTimes = {
        {
            .day = QStringLiteral("Friday"),
            .startTime = QStringLiteral("02:00 PM"),
            .endTime = QStringLiteral("04:00 PM")
        }
    };
    info.notes = QStringLiteral("Class notes\nUTF-16 \U0001F9ED");
    info.timeFillerActivities = QStringLiteral("Filler \U0001F4DA");
    QVERIFY(fixture.services.databaseSession()->classInfoRepository()
        ->saveClassInfo(info));

    MyClassesPage page(&fixture.services);
    page.resize(900, 700);
    page.refresh();
    page.show();
    QApplication::processEvents();

    NavigationTabWidget* tabs = classTabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);
    QWidget* classPage = tabs->widget(0);
    QVERIFY(classPage);
    QCOMPARE(classPage->property("class_id").toInt(), classId);
    QCOMPARE(labelsWithText(*classPage, QStringLiteral("Assigned My Classes Teacher - Room Room 12"))
                 .size(), 1);
    QCOMPARE(labelsWithText(*classPage, QStringLiteral("E4 - Theseus")).size(), 1);

    QLabel* const regularSchedule = infoRowValueLabelFor(
        *classPage,
        QStringLiteral("Regular")
        );
    QVERIFY(regularSchedule);
    QCOMPARE(regularSchedule->text(), QStringLiteral(
        "Tues 11:05 am-12:05 pm; Mon 9:00 AM-9:45 AM"));
    QLabel* const intensiveSchedule = infoRowValueLabelFor(
        *classPage,
        QStringLiteral("Intensive")
        );
    QVERIFY(intensiveSchedule);
    QCOMPARE(intensiveSchedule->text(), QStringLiteral("Fri 02:00 PM-04:00 PM"));
    const QList<QTextEdit*> classNotes = textEditsWithContent(
        *classPage,
        QStringLiteral("Class notes\nUTF-16 \U0001F9ED")
        );
    QCOMPARE(classNotes.size(), 1);
    QCOMPARE(textEditsWithContent(
                 *classPage,
                 QStringLiteral("Filler \U0001F4DA")
                 ).size(), 1);

    QSqlQuery dropClassInfo(
        fixture.services.databaseSession()->database());
    QVERIFY2(dropClassInfo.exec(QStringLiteral("DROP TABLE class_info")),
             qPrintable(dropClassInfo.lastError().text()));

    bool warningCaptured = false;
    QTimer::singleShot(
        0,
        &page,
        [&warningCaptured]()
        {
            auto* const warning = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget());
            if (warning)
            {
                warningCaptured = true;
                warning->accept();
            }
        }
        );
    page.refresh();
    QApplication::processEvents();

    QVERIFY(!warningCaptured);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    tabs = classTabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);
    classPage = tabs->widget(0);
    QVERIFY(classPage);
    QCOMPARE(classPage->property("class_id").toInt(), classId);
    QCOMPARE(labelsWithText(*classPage, QStringLiteral("Unassigned")).size(), 1);
    QLabel* const scheduleFallback = infoRowValueLabelFor(
        *classPage,
        QStringLiteral("Schedule")
        );
    QVERIFY(scheduleFallback);
    QCOMPARE(scheduleFallback->text(), QStringLiteral("N/A"));
    QCOMPARE(textEditsWithContent(*classPage, QString()).size(), 2);
    QCOMPARE(textEditsWithContent(*classPage, QStringLiteral("N/A")).size(), 1);
}

void MyClassesPageTests::
classInformationBatchFailureKeepsEachClassInListOrder()
{
    MyClassesPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int betaId = 0;
    int alphaId = 0;
    int missingInfoId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Beta information failure class"),
                 QStringLiteral("E5"),
                 QStringLiteral("Artemis"),
                 &betaId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Alpha information failure class"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 &alphaId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Gamma information default class"),
                 QStringLiteral("E6"),
                 QStringLiteral("Gaia"),
                 &missingInfoId,
                 &error
                 ), qPrintable(error));

    QSqlQuery deleteMissingInfo(
        fixture.services.databaseSession()->database());
    deleteMissingInfo.prepare(QStringLiteral(
        "DELETE FROM class_info WHERE class_id=?"));
    deleteMissingInfo.addBindValue(missingInfoId);
    QVERIFY2(deleteMissingInfo.exec(),
             qPrintable(deleteMissingInfo.lastError().text()));

    ClassInfoRepository* const repository = fixture.services.databaseSession()
        ->classInfoRepository();
    QVERIFY(repository);
    const MyClassesClassInformationBatchReadMetrics before =
        repository->myClassesClassInformationBatchReadMetrics();

    MyClassesPage page(&fixture.services);
    page.resize(900, 700);
    page.show();
    page.refresh();
    QApplication::processEvents();

    const MyClassesClassInformationBatchReadMetrics afterSuccess =
        repository->myClassesClassInformationBatchReadMetrics();
    QCOMPARE(afterSuccess.callCount, before.callCount + 1);
    QCOMPARE(afterSuccess.requestedClassCount, before.requestedClassCount + 3);
    QCOMPARE(afterSuccess.metadataStatementCount,
             before.metadataStatementCount + 1);
    QCOMPARE(afterSuccess.regularScheduleStatementCount,
             before.regularScheduleStatementCount + 1);
    QCOMPARE(afterSuccess.intensiveScheduleStatementCount,
             before.intensiveScheduleStatementCount + 1);
    QCOMPARE(afterSuccess.fallbackClassReadCount,
             before.fallbackClassReadCount);
    NavigationTabWidget* tabs = classTabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 3);
    QCOMPARE(tabs->widget(0)->property("class_id").toInt(), alphaId);
    QCOMPARE(tabs->widget(1)->property("class_id").toInt(), betaId);
    QCOMPARE(tabs->widget(2)->property("class_id").toInt(), missingInfoId);
    QCOMPARE(tabs->tabText(2),
             QStringLiteral("Gamma information default class ")
                 + QChar(0x2022) + QStringLiteral(" No time"));
    tabs->setCurrentIndex(2);
    QCOMPARE(labelsWithText(*tabs->widget(2), QStringLiteral("Unassigned")).size(), 1);

    QSqlQuery dropClassInfo(
        fixture.services.databaseSession()->database());
    QVERIFY2(dropClassInfo.exec(QStringLiteral("DROP TABLE class_info")),
             qPrintable(dropClassInfo.lastError().text()));

    TeacherRepository* const teacherRepository =
        fixture.services.databaseSession()->teacherRepository();
    QVERIFY(teacherRepository);
    const MyClassesTeacherProfileBatchReadMetrics
        beforeInformationFailureRefresh =
            teacherRepository->myClassesTeacherProfileBatchReadMetrics();
    RosterRepository* const rosterRepository = fixture.services
        .databaseSession()->rosterRepository();
    QVERIFY(rosterRepository);
    const auto beforeInformationFailureCounts =
        rosterRepository->myClassesStudentCountBatchReadMetrics();
    page.refresh();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();

    const MyClassesTeacherProfileBatchReadMetrics
        afterInformationFailureRefresh =
            teacherRepository->myClassesTeacherProfileBatchReadMetrics();
    QCOMPARE(afterInformationFailureRefresh.callCount,
             beforeInformationFailureRefresh.callCount);
    QCOMPARE(afterInformationFailureRefresh.requestedTeacherCount,
             beforeInformationFailureRefresh.requestedTeacherCount);
    QCOMPARE(afterInformationFailureRefresh.statementCount,
             beforeInformationFailureRefresh.statementCount);

    const MyClassesClassInformationBatchReadMetrics afterFailure =
        repository->myClassesClassInformationBatchReadMetrics();
    QCOMPARE(afterFailure.callCount, afterSuccess.callCount + 1);
    QCOMPARE(afterFailure.requestedClassCount,
             afterSuccess.requestedClassCount + 3);
    QCOMPARE(afterFailure.metadataStatementCount,
             afterSuccess.metadataStatementCount + 1);
    QCOMPARE(afterFailure.regularScheduleStatementCount,
             afterSuccess.regularScheduleStatementCount);
    QCOMPARE(afterFailure.intensiveScheduleStatementCount,
             afterSuccess.intensiveScheduleStatementCount);
    QCOMPARE(afterFailure.fallbackClassReadCount,
             afterSuccess.fallbackClassReadCount + 3);
    const auto afterInformationFailureCounts =
        rosterRepository->myClassesStudentCountBatchReadMetrics();
    QCOMPARE(afterInformationFailureCounts.callCount,
             beforeInformationFailureCounts.callCount + 1);
    QCOMPARE(afterInformationFailureCounts.requestedClassCount,
             beforeInformationFailureCounts.requestedClassCount + 3);
    QCOMPARE(afterInformationFailureCounts.columnStatementCount,
             beforeInformationFailureCounts.columnStatementCount + 1);
    QCOMPARE(afterInformationFailureCounts.dataStatementCount,
             beforeInformationFailureCounts.dataStatementCount + 1);
    QCOMPARE(afterInformationFailureCounts.fallbackClassReadCount,
             beforeInformationFailureCounts.fallbackClassReadCount);
    tabs = classTabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 3);
    QCOMPARE(tabs->widget(0)->property("class_id").toInt(), alphaId);
    QCOMPARE(tabs->widget(1)->property("class_id").toInt(), betaId);
    QCOMPARE(tabs->widget(2)->property("class_id").toInt(), missingInfoId);
    for (int index = 0; index < tabs->count(); ++index)
    {
        QWidget* const classPage = tabs->widget(index);
        QVERIFY(classPage);
        tabs->setCurrentIndex(index);
        QCOMPARE(labelsWithText(*classPage, QStringLiteral("Unassigned")).size(), 1);
        QLabel* const schedule = infoRowValueLabelFor(
            *classPage,
            QStringLiteral("Schedule")
            );
        QVERIFY(schedule);
        QCOMPARE(schedule->text(), QStringLiteral("N/A"));
    }
}

void MyClassesPageTests::
failedTeacherProfileKeepsClassAndUsesSilentUnassignedFallback()
{
    MyClassesPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Assigned Teacher");
    teacher.teacherKr = QStringLiteral("\uC120\uC0DD\uB2D8");
    int teacherId = 0;
    QVERIFY2(fixture.createTeacher(teacher, &teacherId, &error),
             qPrintable(error));

    int classId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Assigned class"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 &classId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.assignTeacher(classId, teacherId, &error),
             qPrintable(error));

    TeacherRepository* const teacherRepository =
        fixture.services.databaseSession()->teacherRepository();
    QVERIFY(teacherRepository);
    const MyClassesTeacherProfileBatchReadMetrics beforeFirstRefresh =
        teacherRepository->myClassesTeacherProfileBatchReadMetrics();
    MyClassesPage page(&fixture.services);
    page.resize(900, 700);
    page.refresh();
    page.show();
    QApplication::processEvents();

    const MyClassesTeacherProfileBatchReadMetrics afterFirstRefresh =
        teacherRepository->myClassesTeacherProfileBatchReadMetrics();
    QCOMPARE(afterFirstRefresh.callCount,
             beforeFirstRefresh.callCount + 1);
    QCOMPARE(afterFirstRefresh.requestedTeacherCount,
             beforeFirstRefresh.requestedTeacherCount + 1);
    QCOMPARE(afterFirstRefresh.statementCount,
             beforeFirstRefresh.statementCount + 1);

    NavigationTabWidget* tabs = classTabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);
    QCOMPARE(tabs->widget(0)->property("class_id").toInt(), classId);
    const QString singleClassTabLabel = classTabLabel(
        QStringLiteral("E4"), QStringLiteral("Theseus"));
    QCOMPARE(tabs->tabText(0), singleClassTabLabel);
    QCOMPARE(labelsWithText(page, QStringLiteral("Assigned Teacher")).size(),
             1);

    QSqlQuery disableForeignKeys(fixture.services.databaseSession()->database());
    QVERIFY2(disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")),
             qPrintable(disableForeignKeys.lastError().text()));
    QSqlQuery deleteTeacher(fixture.services.databaseSession()->database());
    QVERIFY(deleteTeacher.prepare(
        QStringLiteral("DELETE FROM teachers WHERE id=?")));
    deleteTeacher.addBindValue(teacherId);
    QVERIFY2(deleteTeacher.exec(), qPrintable(deleteTeacher.lastError().text()));

    bool warningCaptured = false;
    QTimer::singleShot(
        0,
        &page,
        [&warningCaptured]()
        {
            auto* warning = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget());
            if (!warning)
            {
                return;
            }

            warningCaptured = true;
            warning->accept();
        }
        );

    const MyClassesTeacherProfileBatchReadMetrics beforeFailedProfileRefresh =
        teacherRepository->myClassesTeacherProfileBatchReadMetrics();
    page.refresh();
    QApplication::processEvents();

    const MyClassesTeacherProfileBatchReadMetrics afterFailedProfileRefresh =
        teacherRepository->myClassesTeacherProfileBatchReadMetrics();
    QCOMPARE(afterFailedProfileRefresh.callCount,
             beforeFailedProfileRefresh.callCount + 1);
    QCOMPARE(afterFailedProfileRefresh.requestedTeacherCount,
             beforeFailedProfileRefresh.requestedTeacherCount + 1);
    QCOMPARE(afterFailedProfileRefresh.statementCount,
             beforeFailedProfileRefresh.statementCount + 1);

    QVERIFY(!warningCaptured);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    tabs = classTabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);
    QCOMPARE(tabs->widget(0)->property("class_id").toInt(), classId);
    QCOMPARE(tabs->tabText(0), singleClassTabLabel);

    QWidget* const classPage = tabs->widget(0);
    QVERIFY(classPage);
    QCOMPARE(labelsWithText(*classPage, QStringLiteral("E4 - Theseus")).size(),
             1);
    QCOMPARE(labelsWithText(*classPage, QStringLiteral("Unassigned")).size(),
             1);

    const QList<QLineEdit*> fallbackFields =
        classPage->findChildren<QLineEdit*>();
    QCOMPARE(fallbackFields.size(), 6);
    for (QLineEdit* field : fallbackFields)
    {
        QVERIFY(field->isReadOnly());
        QCOMPARE(field->text(), QStringLiteral("N/A"));
    }
}

void MyClassesPageTests::
classesRenderInOrderWithTitlesAndSelectionRestoredById()
{
    MyClassesPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int artemisId = 0;
    int perseusId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Alpha stored class"),
                 QStringLiteral("E5"),
                 QStringLiteral("Artemis"),
                 &artemisId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Zulu stored class"),
                 QStringLiteral("E4"),
                 QStringLiteral("Perseus"),
                 &perseusId,
                 &error
                 ), qPrintable(error));

    MyClassesPage page(&fixture.services);
    page.resize(900, 700);
    page.refresh();
    page.show();
    QApplication::processEvents();

    NavigationTabWidget* tabs = classTabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 2);
    QCOMPARE(tabs->tabText(0),
             classTabLabel(QStringLiteral("E4"), QStringLiteral("Perseus")));
    QCOMPARE(tabs->tabText(1),
             classTabLabel(QStringLiteral("E5"), QStringLiteral("Artemis")));
    QCOMPARE(tabs->widget(0)->property("class_id").toInt(), perseusId);
    QCOMPARE(tabs->widget(1)->property("class_id").toInt(), artemisId);
    QCOMPARE(tabs->currentWidget()->property("class_id").toInt(), perseusId);

    const QList<QLabel*> initialTitle = labelsWithText(
        page, QStringLiteral("E4 - Perseus"));
    QCOMPARE(initialTitle.size(), 1);
    QVERIFY(initialTitle.constFirst()->isVisible());

    int theseusId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Beta stored class"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 &theseusId,
                 &error
                 ), qPrintable(error));

    tabs->setCurrentIndex(0);
    QCOMPARE(tabs->currentWidget()->property("class_id").toInt(), perseusId);
    page.refresh();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();

    tabs = classTabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 3);
    QCOMPARE(tabs->tabText(0),
             classTabLabel(QStringLiteral("E4"), QStringLiteral("Theseus")));
    QCOMPARE(tabs->tabText(1),
             classTabLabel(QStringLiteral("E4"), QStringLiteral("Perseus")));
    QCOMPARE(tabs->tabText(2),
             classTabLabel(QStringLiteral("E5"), QStringLiteral("Artemis")));
    QCOMPARE(tabs->widget(0)->property("class_id").toInt(), theseusId);
    QCOMPARE(tabs->widget(1)->property("class_id").toInt(), perseusId);
    QCOMPARE(tabs->widget(2)->property("class_id").toInt(), artemisId);
    QCOMPARE(tabs->currentIndex(), 1);
    QCOMPARE(tabs->currentWidget()->property("class_id").toInt(), perseusId);

    const QList<QLabel*> restoredTitle = labelsWithText(
        page, QStringLiteral("E4 - Perseus"));
    QCOMPARE(restoredTitle.size(), 1);
    QVERIFY(restoredTitle.constFirst()->isVisible());
    QVERIFY(theseusId > 0);
    QVERIFY(artemisId > 0);
}

void MyClassesPageTests::successfulEmptyListShowsEmptyState()
{
    MyClassesPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    MyClassesPage page(&fixture.services);
    page.resize(900, 700);
    page.refresh();
    page.show();
    QApplication::processEvents();

    const QList<QLabel*> emptyLabels = labelsWithText(
        page, QStringLiteral("No classes available."));
    QCOMPARE(emptyLabels.size(), 1);
    QVERIFY(emptyLabels.constFirst()->isVisible());
    QVERIFY(classTabsFor(page) == nullptr);

    QLabel* const title = page.findChild<QLabel*>(QStringLiteral("pageTitle"));
    QVERIFY(title);
    QCOMPARE(title->text(), QStringLiteral("Class Information"));
}

void MyClassesPageTests::closedSessionRefreshIsQuietAndKeepsRenderedContent()
{
    MyClassesPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int classId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Current class"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 &classId,
                 &error
                 ), qPrintable(error));

    MyClassesPage page(&fixture.services);
    page.resize(900, 700);
    page.refresh();

    NavigationTabWidget* const tabs = classTabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);
    const QString initialTabText = classTabLabel(
        QStringLiteral("E4"),
        QStringLiteral("Theseus")
        );
    QCOMPARE(tabs->tabText(0), initialTabText);
    QPointer<NavigationTabWidget> tabsGuard(tabs);

    fixture.services.closeDatabase();
    QVERIFY(!fixture.services.hasOpenDatabase());

    bool warningShown = false;
    QTimer::singleShot(0, &page, [&warningShown]
    {
        auto* const warning = qobject_cast<QMessageBox*>(
            QApplication::activeModalWidget()
            );
        if (warning)
        {
            warningShown = true;
            warning->accept();
        }
    });
    page.refresh();
    QApplication::processEvents();

    QVERIFY(!warningShown);
    QVERIFY(tabsGuard);
    QCOMPARE(classTabsFor(page), tabsGuard.data());
    QCOMPARE(tabsGuard->count(), 1);
    QCOMPARE(tabsGuard->tabText(0), initialTabText);
    QWidget* const retainedClassPage = tabsGuard->widget(0);
    QVERIFY(retainedClassPage);
    QCOMPARE(labelsWithText(
        *retainedClassPage,
        QStringLiteral("E4 - Theseus")
        ).size(), 1);
}

void MyClassesPageTests::
failedClassListReadClearsRenderedContentAndShowsWarning()
{
    MyClassesPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int currentId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Current class"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 &currentId,
                 &error
                 ), qPrintable(error));

    MyClassesPage page(&fixture.services);
    page.resize(900, 700);
    page.refresh();
    page.show();
    QApplication::processEvents();

    NavigationTabWidget* const originalTabs = classTabsFor(page);
    QVERIFY(originalTabs);
    QCOMPARE(originalTabs->count(), 1);
    QCOMPARE(originalTabs->widget(0)->property("class_id").toInt(), currentId);
    const QList<QLabel*> originalTitle = labelsWithText(
        page, QStringLiteral("E4 - Theseus"));
    QCOMPARE(originalTitle.size(), 1);
    QVERIFY(originalTitle.constFirst()->isVisible());
    QPointer<NavigationTabWidget> previousTabs(originalTabs);

    QSqlQuery disableForeignKeys(fixture.services.databaseSession()->database());
    QVERIFY2(disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")),
             qPrintable(disableForeignKeys.lastError().text()));
    QSqlQuery dropClasses(fixture.services.databaseSession()->database());
    QVERIFY2(dropClasses.exec(QStringLiteral("DROP TABLE classes")),
             qPrintable(dropClasses.lastError().text()));

    QString warningTitle;
    QString warningText;
    QString warningDetails;
    bool warningCaptured = false;
    QTimer::singleShot(
        0,
        &page,
        [&]()
        {
            auto* warning = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget());
            if (!warning)
            {
                return;
            }

            warningTitle = warning->windowTitle();
            warningText = warning->text();
            warningDetails = warning->detailedText();
            warningCaptured = true;
            warning->accept();
        }
        );

    page.refresh();

    QVERIFY(warningCaptured);
    QCOMPARE(warningTitle, QStringLiteral("Load Classes"));
    QCOMPARE(warningText,
             QStringLiteral("Class information could not be loaded."));
    QVERIFY2(!warningDetails.isEmpty(),
             "The class-list read error details were not shown.");
    QVERIFY(warningDetails.contains(QStringLiteral("classes"),
                                    Qt::CaseInsensitive));

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(previousTabs.isNull());
    QVERIFY(page.findChildren<NavigationTabWidget*>().isEmpty());
    QVERIFY(labelsWithText(page, QStringLiteral("E4 - Theseus")).isEmpty());
    QVERIFY(labelsWithText(
        page, QStringLiteral("No classes available.")).isEmpty());
}

void MyClassesPageTests::
rosterCountUsesNonblankEnglishOrKoreanCells()
{
    MyClassesPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int classId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Roster count class"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 &classId,
                 &error
                 ), qPrintable(error));

    const Roster roster = rosterWithNamedRows();
    QVERIFY2(saveRoster(fixture, classId, roster, &error), qPrintable(error));
    QVERIFY2(updateRosterCell(
                 fixture,
                 classId,
                 1,
                 0,
                 QStringLiteral(" \t "),
                 &error
                 ), qPrintable(error));
    QVERIFY2(updateRosterCell(
                 fixture,
                 classId,
                 2,
                 1,
                 QStringLiteral("  "),
                 &error
                 ), qPrintable(error));
    QVERIFY2(updateRosterCell(
                 fixture,
                 classId,
                 3,
                 0,
                 QStringLiteral(" \n "),
                 &error
                 ), qPrintable(error));
    QVERIFY2(updateRosterCell(
                 fixture,
                 classId,
                 3,
                 1,
                 QStringLiteral(" \t "),
                 &error
                 ), qPrintable(error));

    MyClassesPage page(&fixture.services);
    page.resize(900, 700);
    page.refresh();
    page.show();
    QApplication::processEvents();

    NavigationTabWidget* const tabs = classTabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);
    QWidget* const classPage = tabs->widget(0);
    QVERIFY(classPage);
    QCOMPARE(classPage->property("class_id").toInt(), classId);
    QLabel* const count = infoRowValueLabelFor(
        *classPage,
        QStringLiteral("# of Students")
        );
    QVERIFY(count);
    QVERIFY(count->isVisible());
    QCOMPARE(count->text(), QStringLiteral("3"));
}

void MyClassesPageTests::
rosterReadFailureKeepsClassAndShowsZeroStudentCount()
{
    MyClassesPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int classId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Roster failure class"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 &classId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(saveRoster(
                 fixture,
                 classId,
                 rosterWithNamedRows(),
                 &error
                 ), qPrintable(error));

    MyClassesPage page(&fixture.services);
    page.resize(900, 700);
    page.refresh();
    page.show();
    QApplication::processEvents();

    NavigationTabWidget* tabs = classTabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);
    QCOMPARE(tabs->widget(0)->property("class_id").toInt(), classId);
    QLabel* const initialCount = infoRowValueLabelFor(
        *tabs->widget(0),
        QStringLiteral("# of Students")
        );
    QVERIFY(initialCount);
    QCOMPARE(initialCount->text(), QStringLiteral("4"));

    QSqlQuery dropRosterData(
        fixture.services.databaseSession()->database());
    QVERIFY2(dropRosterData.exec(QStringLiteral("DROP TABLE roster_data")),
             qPrintable(dropRosterData.lastError().text()));
    const Result<int> expectedRosterReadFailure =
        fixture.services.rosterService()->studentCount(classId);
    QVERIFY(!expectedRosterReadFailure);
    QVERIFY(!expectedRosterReadFailure.error().trimmed().isEmpty());
    RosterRepository* const rosterRepository = fixture.services
        .databaseSession()->rosterRepository();
    QVERIFY(rosterRepository);
    const auto metricsBeforeFailure =
        rosterRepository->myClassesStudentCountBatchReadMetrics();

    bool warningCaptured = false;
    QTimer::singleShot(
        0,
        &page,
        [&warningCaptured]()
        {
            auto* const warning = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget());
            if (!warning)
            {
                return;
            }

            warningCaptured = true;
            warning->accept();
        }
        );
    page.refresh();
    QApplication::processEvents();

    QVERIFY(!warningCaptured);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    tabs = classTabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);
    QWidget* const classPage = tabs->widget(0);
    QVERIFY(classPage);
    QCOMPARE(classPage->property("class_id").toInt(), classId);
    QLabel* const count = infoRowValueLabelFor(
        *classPage,
        QStringLiteral("# of Students")
        );
    QVERIFY(count);
    QVERIFY(count->isVisible());
    QCOMPARE(count->text(), QStringLiteral("0"));
    QCOMPARE(labelsWithText(page, QStringLiteral("E4 - Theseus")).size(), 1);
    const auto metricsAfterFailure =
        rosterRepository->myClassesStudentCountBatchReadMetrics();
    QCOMPARE(metricsAfterFailure.callCount, metricsBeforeFailure.callCount + 1);
    QCOMPARE(metricsAfterFailure.requestedClassCount,
             metricsBeforeFailure.requestedClassCount + 1);
    QCOMPARE(metricsAfterFailure.fallbackClassReadCount,
             metricsBeforeFailure.fallbackClassReadCount + 1);
}

void MyClassesPageTests::studentCountBatchRunsOnceAndKeepsClassOrder()
{
    MyClassesPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int firstClassId = 0;
    int secondClassId = 0;
    int thirdClassId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("First roster batch class"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 &firstClassId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Second roster batch class"),
                 QStringLiteral("E5"),
                 QStringLiteral("Artemis"),
                 &secondClassId,
                 &error
                 ), qPrintable(error));
    const bool thirdClassCreated = fixture.createClass(
        QStringLiteral("Third roster batch class"),
        QStringLiteral("E6"),
        QStringLiteral("Gaia"),
        &thirdClassId,
        &error
        );
    QVERIFY2(thirdClassCreated, qPrintable(error));

    const Roster validRoster = rosterWithNamedRows();
    QVERIFY2(saveRoster(fixture, firstClassId, validRoster, &error),
             qPrintable(error));
    QVERIFY2(saveRoster(fixture, secondClassId, validRoster, &error),
             qPrintable(error));
    QVERIFY2(saveRoster(fixture, thirdClassId, validRoster, &error),
             qPrintable(error));

    const auto clearNameCells =
        [&fixture, &error](const int targetClassId, const int rowIndex)
    {
        return updateRosterCell(
                   fixture,
                   targetClassId,
                   rowIndex,
                   0,
                   QStringLiteral(" \u2003 "),
                   &error
                   )
            && updateRosterCell(
                   fixture,
                   targetClassId,
                   rowIndex,
                   1,
                   QStringLiteral(" \t "),
                   &error
                   );
    };
    for (const int rowIndex : {2, 3})
    {
        QVERIFY2(clearNameCells(firstClassId, rowIndex), qPrintable(error));
    }
    for (const int rowIndex : {0, 1, 3})
    {
        QVERIFY2(clearNameCells(secondClassId, rowIndex), qPrintable(error));
    }
    for (const int rowIndex : {0, 1, 2, 3})
    {
        QVERIFY2(clearNameCells(thirdClassId, rowIndex), qPrintable(error));
    }

    RosterRepository* const repository = fixture.services.databaseSession()
        ->rosterRepository();
    QVERIFY(repository);
    const auto before = repository->myClassesStudentCountBatchReadMetrics();

    MyClassesPage page(&fixture.services);
    page.resize(900, 700);
    page.refresh();
    page.show();
    QApplication::processEvents();

    NavigationTabWidget* const tabs = classTabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 3);
    const int expectedClassIds[] = {
        firstClassId, secondClassId, thirdClassId
    };
    const QString expectedCounts[] = {
        QStringLiteral("2"), QStringLiteral("1"), QStringLiteral("0")
    };
    for (int index = 0; index < 3; ++index)
    {
        tabs->setCurrentIndex(index);
        QWidget* const classPage = tabs->widget(index);
        QVERIFY(classPage);
        QCOMPARE(classPage->property("class_id").toInt(), expectedClassIds[index]);
        QLabel* const count = infoRowValueLabelFor(
            *classPage,
            QStringLiteral("# of Students")
            );
        QVERIFY(count);
        QCOMPARE(count->text(), expectedCounts[index]);
        for (int otherIndex = 0; otherIndex < tabs->count(); ++otherIndex)
        {
            if (otherIndex != index)
            {
                QVERIFY(tabs->widget(otherIndex)->findChildren<QWidget*>().isEmpty());
            }
        }
    }

    const auto after = repository->myClassesStudentCountBatchReadMetrics();
    QCOMPARE(after.callCount, before.callCount + 1);
    QCOMPARE(after.requestedClassCount, before.requestedClassCount + 3);
    QCOMPARE(after.columnStatementCount, before.columnStatementCount + 1);
    QCOMPARE(after.dataStatementCount, before.dataStatementCount + 1);
    QCOMPARE(after.fallbackClassReadCount, before.fallbackClassReadCount);
}

void MyClassesPageTests::flatPageReentryRestoresDetailsWithoutSummaryRead()
{
    MyClassesPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int firstClassId = 0;
    int secondClassId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Flat first class"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 &firstClassId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Flat second class"),
                 QStringLiteral("E5"),
                 QStringLiteral("Artemis"),
                 &secondClassId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(saveRoster(
                 fixture,
                 secondClassId,
                 rosterWithNamedRows(),
                 &error
                 ), qPrintable(error));

    DatabaseSession* const session = fixture.services.databaseSession();
    QVERIFY(session);
    ClassInfoRepository* const classInfoRepository =
        session->classInfoRepository();
    TeacherRepository* const teacherRepository =
        session->teacherRepository();
    RosterRepository* const rosterRepository =
        session->rosterRepository();
    QVERIFY(classInfoRepository);
    QVERIFY(teacherRepository);
    QVERIFY(rosterRepository);

    MyClassesPage page(&fixture.services);
    page.resize(900, 700);
    page.refresh();
    page.show();
    QApplication::processEvents();

    NavigationTabWidget* const tabs = classTabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 2);
    const int selectedIndex = tabIndexForClass(*tabs, secondClassId);
    QVERIFY(selectedIndex >= 0);
    tabs->setCurrentIndex(selectedIndex);

    QWidget* const selectedPage = tabs->currentWidget();
    QVERIFY(selectedPage);
    QCOMPARE(selectedPage->property("class_id").toInt(), secondClassId);
    QLabel* const selectedStudentCount = infoRowValueLabelFor(
        *selectedPage,
        QStringLiteral("# of Students")
        );
    QVERIFY(selectedStudentCount);
    const QString selectedStudentCountBefore = selectedStudentCount->text();
    QCOMPARE(selectedStudentCountBefore, QStringLiteral("4"));

    const auto classInfoReadsBefore =
        classInfoRepository->myClassesClassInformationBatchReadMetrics();
    const auto teacherReadsBefore =
        teacherRepository->myClassesTeacherProfileBatchReadMetrics();
    const auto rosterReadsBefore =
        rosterRepository->myClassesStudentCountBatchReadMetrics();
    const MyClassesPageRuntimeMetrics pageMetricsBefore =
        page.runtimeMetrics();
    QCOMPARE(pageMetricsBefore.classSummaryListQueryCount, 1);

    page.deactivate();
    QCOMPARE(tabs->count(), 2);
    QCOMPARE(tabs->currentIndex(), selectedIndex);
    QCOMPARE(tabs->currentWidget(), selectedPage);
    QCOMPARE(selectedPage->property("class_id").toInt(), secondClassId);
    QVERIFY(selectedPage->findChildren<QWidget*>().isEmpty());
    QVERIFY(!infoRowValueLabelFor(
        *selectedPage,
        QStringLiteral("# of Students")
        ));

    page.activate();
    QCOMPARE(tabs->count(), 2);
    QCOMPARE(tabs->currentIndex(), selectedIndex);
    QCOMPARE(tabs->currentWidget(), selectedPage);
    QCOMPARE(selectedPage->property("class_id").toInt(), secondClassId);
    QLabel* const restoredStudentCount = infoRowValueLabelFor(
        *selectedPage,
        QStringLiteral("# of Students")
        );
    QVERIFY(restoredStudentCount);
    QCOMPARE(restoredStudentCount->text(), selectedStudentCountBefore);
    const int firstIndex = tabIndexForClass(*tabs, firstClassId);
    QVERIFY(firstIndex >= 0);
    QVERIFY(tabs->widget(firstIndex)->findChildren<QWidget*>().isEmpty());

    const auto classInfoReadsAfter =
        classInfoRepository->myClassesClassInformationBatchReadMetrics();
    const auto teacherReadsAfter =
        teacherRepository->myClassesTeacherProfileBatchReadMetrics();
    const auto rosterReadsAfter =
        rosterRepository->myClassesStudentCountBatchReadMetrics();
    const MyClassesPageRuntimeMetrics pageMetricsAfter = page.runtimeMetrics();
    QCOMPARE(
        pageMetricsAfter.classSummaryListQueryCount,
        pageMetricsBefore.classSummaryListQueryCount
        );
    QCOMPARE(classInfoReadsAfter.callCount, classInfoReadsBefore.callCount);
    QCOMPARE(teacherReadsAfter.callCount, teacherReadsBefore.callCount);
    QCOMPARE(rosterReadsAfter.callCount, rosterReadsBefore.callCount);
}

void MyClassesPageTests::groupedGradeSelectionMaterializesOnlySelectedDetails()
{
    MyClassesPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    for (int index = 0; index < 7; ++index)
    {
        const QString grade =
            index < 4
                ? QStringLiteral("E4")
                : QStringLiteral("E5");
        const QString level =
            grade == QStringLiteral("E4")
                ? index % 2 == 0
                    ? QStringLiteral("Theseus")
                    : QStringLiteral("Perseus")
                : QStringLiteral("Artemis");
        int classId = 0;
        QVERIFY2(fixture.createClass(
                     QStringLiteral("Grouped class %1").arg(index),
                     grade,
                     level,
                     &classId,
                     &error
                     ), qPrintable(error));
        QVERIFY(classId > 0);
    }

    MyClassesPage page(&fixture.services);
    page.resize(900, 700);
    page.refresh();
    page.show();
    QApplication::processEvents();

    NavigationTabWidget* const gradeTabs =
        page.findChild<NavigationTabWidget*>(
            QStringLiteral("myInfoGradeTabs")
            );
    QVERIFY(gradeTabs);
    QCOMPARE(gradeTabs->count(), 2);

    const auto classTabsForGrade =
        [gradeTabs](int gradeIndex)
        {
            QWidget* const gradePage =
                gradeTabs->widget(gradeIndex);
            return gradePage
                ? gradePage->findChild<NavigationTabWidget*>(
                    QStringLiteral("myInfoClassTabs"),
                    Qt::FindDirectChildrenOnly
                    )
                : nullptr;
        };

    NavigationTabWidget* const firstGradeClassTabs =
        classTabsForGrade(0);
    NavigationTabWidget* const secondGradeClassTabs =
        classTabsForGrade(1);
    QVERIFY(firstGradeClassTabs);
    QVERIFY(secondGradeClassTabs);
    QCOMPARE(firstGradeClassTabs->count(), 4);
    QCOMPARE(secondGradeClassTabs->count(), 3);

    QWidget* const firstGradeClassPage =
        firstGradeClassTabs->currentWidget();
    QWidget* const firstSecondGradeClassPage =
        secondGradeClassTabs->currentWidget();
    QVERIFY(firstGradeClassPage);
    QVERIFY(firstSecondGradeClassPage);
    QVERIFY(infoRowValueLabelFor(
        *firstGradeClassPage,
        QStringLiteral("# of Students")
        ));
    QVERIFY(firstSecondGradeClassPage->findChildren<QWidget*>().isEmpty());

    gradeTabs->setCurrentIndex(1);
    QVERIFY(firstGradeClassPage->findChildren<QWidget*>().isEmpty());
    QVERIFY(infoRowValueLabelFor(
        *firstSecondGradeClassPage,
        QStringLiteral("# of Students")
        ));

    secondGradeClassTabs->setCurrentIndex(1);
    QVERIFY(firstSecondGradeClassPage->findChildren<QWidget*>().isEmpty());
    QWidget* const secondSecondGradeClassPage =
        secondGradeClassTabs->currentWidget();
    QVERIFY(secondSecondGradeClassPage);
    const int secondSecondGradeClassId =
        secondSecondGradeClassPage->property("class_id").toInt();
    QLabel* const selectedStudentCount = infoRowValueLabelFor(
        *secondSecondGradeClassPage,
        QStringLiteral("# of Students")
        );
    QVERIFY(selectedStudentCount);
    const QString selectedStudentCountBefore = selectedStudentCount->text();
    QCOMPARE(selectedStudentCountBefore, QStringLiteral("0"));

    DatabaseSession* const session = fixture.services.databaseSession();
    QVERIFY(session);
    ClassInfoRepository* const classInfoRepository =
        session->classInfoRepository();
    TeacherRepository* const teacherRepository =
        session->teacherRepository();
    RosterRepository* const rosterRepository =
        session->rosterRepository();
    QVERIFY(classInfoRepository);
    QVERIFY(teacherRepository);
    QVERIFY(rosterRepository);
    const auto classInfoReadsBefore =
        classInfoRepository->myClassesClassInformationBatchReadMetrics();
    const auto teacherReadsBefore =
        teacherRepository->myClassesTeacherProfileBatchReadMetrics();
    const auto rosterReadsBefore =
        rosterRepository->myClassesStudentCountBatchReadMetrics();
    const MyClassesPageRuntimeMetrics pageMetricsBefore =
        page.runtimeMetrics();
    QCOMPARE(pageMetricsBefore.classSummaryListQueryCount, 1);

    page.deactivate();
    QCOMPARE(gradeTabs->currentIndex(), 1);
    QCOMPARE(secondGradeClassTabs->currentIndex(), 1);
    QVERIFY(firstGradeClassPage->findChildren<QWidget*>().isEmpty());
    QVERIFY(firstSecondGradeClassPage->findChildren<QWidget*>().isEmpty());
    QVERIFY(secondSecondGradeClassPage->findChildren<QWidget*>().isEmpty());

    page.activate();
    QCOMPARE(gradeTabs->currentIndex(), 1);
    QCOMPARE(secondGradeClassTabs->currentIndex(), 1);
    QCOMPARE(secondGradeClassTabs->currentWidget(), secondSecondGradeClassPage);
    QCOMPARE(
        secondSecondGradeClassPage->property("class_id").toInt(),
        secondSecondGradeClassId
        );
    QLabel* const restoredStudentCount = infoRowValueLabelFor(
        *secondSecondGradeClassPage,
        QStringLiteral("# of Students")
        );
    QVERIFY(restoredStudentCount);
    QCOMPARE(restoredStudentCount->text(), selectedStudentCountBefore);
    QVERIFY(firstGradeClassPage->findChildren<QWidget*>().isEmpty());
    QVERIFY(firstSecondGradeClassPage->findChildren<QWidget*>().isEmpty());

    const auto classInfoReadsAfter =
        classInfoRepository->myClassesClassInformationBatchReadMetrics();
    const auto teacherReadsAfter =
        teacherRepository->myClassesTeacherProfileBatchReadMetrics();
    const auto rosterReadsAfter =
        rosterRepository->myClassesStudentCountBatchReadMetrics();
    const MyClassesPageRuntimeMetrics pageMetricsAfter = page.runtimeMetrics();
    QCOMPARE(
        pageMetricsAfter.classSummaryListQueryCount,
        pageMetricsBefore.classSummaryListQueryCount
        );
    QCOMPARE(classInfoReadsAfter.callCount, classInfoReadsBefore.callCount);
    QCOMPARE(teacherReadsAfter.callCount, teacherReadsBefore.callCount);
    QCOMPARE(rosterReadsAfter.callCount, rosterReadsBefore.callCount);

    gradeTabs->setCurrentIndex(0);
    QVERIFY(secondSecondGradeClassPage->findChildren<QWidget*>().isEmpty());
    QVERIFY(infoRowValueLabelFor(
        *firstGradeClassPage,
        QStringLiteral("# of Students")
        ));
}

void MyClassesPageTests::rosterWithoutNameColumnsShowsZeroStudentCount()
{
    MyClassesPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int classId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Roster name columns class"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 &classId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(saveRoster(
                 fixture,
                 classId,
                 rosterWithNamedRows(),
                 &error
                 ), qPrintable(error));

    QSqlQuery renameNameColumn(
        fixture.services.databaseSession()->database());
    QVERIFY2(renameNameColumn.prepare(QStringLiteral(
                 "UPDATE roster_columns SET name=? "
                 "WHERE class_id=? AND name=?")),
             qPrintable(renameNameColumn.lastError().text()));
    for (const auto& [originalName, renamedName] : {
             std::pair{QStringLiteral("English"), QStringLiteral("English name")},
             std::pair{QStringLiteral("Korean"), QStringLiteral("Korean name")}
         })
    {
        renameNameColumn.bindValue(0, renamedName);
        renameNameColumn.bindValue(1, classId);
        renameNameColumn.bindValue(2, originalName);
        QVERIFY2(renameNameColumn.exec(),
                 qPrintable(renameNameColumn.lastError().text()));
        QCOMPARE(renameNameColumn.numRowsAffected(), 1);
    }

    MyClassesPage page(&fixture.services);
    page.resize(900, 700);
    page.refresh();
    page.show();
    QApplication::processEvents();

    NavigationTabWidget* const tabs = classTabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);
    QCOMPARE(tabs->widget(0)->property("class_id").toInt(), classId);
    QLabel* const count = infoRowValueLabelFor(
        *tabs->widget(0),
        QStringLiteral("# of Students")
        );
    QVERIFY(count);
    QVERIFY(count->isVisible());
    QCOMPARE(count->text(), QStringLiteral("0"));
}

QTEST_MAIN(MyClassesPageTests)

#include "my_classes_page_tests.moc"
