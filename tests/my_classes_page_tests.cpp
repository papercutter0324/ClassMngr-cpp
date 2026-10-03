#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/class_info.h"
#include "features/my_info/ui/my_classes_page.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
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

}

class MyClassesPageTests final : public QObject
{
    Q_OBJECT

private slots:
    void classesRenderInOrderWithTitlesAndSelectionRestoredById();
    void successfulEmptyListShowsEmptyState();
    void failedClassListReadClearsRenderedContentAndShowsWarning();
    void assignedTeacherProfileProjectsAllConsumedUtf16Fields();
    void failedTeacherProfileKeepsClassAndUsesSilentUnassignedFallback();
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

    MyClassesPage page(&fixture.services);
    page.resize(900, 700);
    page.refresh();
    page.show();
    QApplication::processEvents();

    NavigationTabWidget* const tabs = classTabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 2);
    const int englishTabIndex = tabIndexForClass(*tabs, englishClassId);
    const int koreanTabIndex = tabIndexForClass(*tabs, koreanClassId);
    QVERIFY(englishTabIndex >= 0);
    QVERIFY(koreanTabIndex >= 0);

    const QString baseLabel = classTabLabel(
        QStringLiteral("E4"), QStringLiteral("Theseus"));
    QCOMPARE(
        tabs->tabText(englishTabIndex),
        baseLabel + QLatin1Char(' ') + QChar(0x2022)
            + QLatin1Char(' ') + englishTeacher.teacherEn
        );
    QCOMPARE(
        tabs->tabText(koreanTabIndex),
        baseLabel + QLatin1Char(' ') + QChar(0x2022)
            + QLatin1Char(' ') + koreanTeacher.teacherKr
        );

    QWidget* const englishClassPage = tabs->widget(englishTabIndex);
    QVERIFY(englishClassPage);
    const QString expectedEnglishHeading =
        englishTeacher.preferredName + QStringLiteral(" - Room ")
        + englishTeacher.roomNumber;
    QCOMPARE(
        labelsWithText(*englishClassPage, expectedEnglishHeading).size(),
        1
        );
    QCOMPARE(
        labelsWithText(
            *tabs->widget(koreanTabIndex),
            koreanTeacher.preferredRomanization
            ).size(),
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

    MyClassesPage page(&fixture.services);
    page.resize(900, 700);
    page.refresh();
    page.show();
    QApplication::processEvents();

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

    page.refresh();
    QApplication::processEvents();

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

QTEST_MAIN(MyClassesPageTests)

#include "my_classes_page_tests.moc"
