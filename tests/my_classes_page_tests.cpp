#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "domain/models/class_info.h"
#include "features/my_info/ui/my_classes_page.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
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
};

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
