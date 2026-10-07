#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "domain/models/class_info.h"
#include "domain/models/roster.h"
#include "features/my_info/ui/my_classes_page.h"
#include "ui/shared/widgets/navigation_tab_widget.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"

#include <QApplication>
#include <QCoreApplication>
#include <QGridLayout>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>
#include <QtTest/QtTest>

namespace
{
struct Fixture final
{
    QTemporaryDir directory;
    ApplicationServices services;

    bool open()
    {
        return directory.isValid()
            && services.openDatabase(
                directory.filePath(QStringLiteral("my-classes-summary.tps"))
                );
    }

    int createClass(
        const QString& name,
        const QString& grade,
        const QString& level,
        const QList<ClassTime>& regular = {},
        const QList<ClassTime>& intensive = {},
        const QString& notes = {},
        const QString& filler = {}
        )
    {
        const auto created = services.classService()->create(name);
        if (!created)
        {
            return -1;
        }

        const auto loaded = services.classService()->classInfo(*created);
        if (!loaded)
        {
            return -1;
        }

        ClassInfo info = *loaded;
        info.classGrade = grade;
        info.classLevel = level;
        info.classTimes = regular;
        info.intensiveTimes = intensive;
        info.notes = notes;
        info.timeFillerActivities = filler;

        auto* const repository = services.databaseSession()
            ? services.databaseSession()->classInfoRepository()
            : nullptr;
        return repository && repository->saveClassInfo(info)
            ? *created
            : -1;
    }

    bool saveRoster(const int classId, const QStringList& names)
    {
        Roster roster;
        roster.columns = Roster::BaseColumns;
        roster.columnWidths.fill(100, roster.columns.size());
        for (const QString& name : names)
        {
            roster.rows.append({name, QStringLiteral("\uAE40\uBBFC\uC9C0")});
        }

        const auto saved = services.rosterService()->saveRoster(classId, roster);
        return saved.has_value();
    }
};

NavigationTabWidget* tabsFor(MyClassesPage& page)
{
    const auto tabs = page.findChildren<NavigationTabWidget*>(
        QStringLiteral("myInfoClassTabs")
        );
    return tabs.size() == 1 ? tabs.first() : nullptr;
}

QLabel* rowValueFor(QWidget& root, const QString& labelText)
{
    for (QGridLayout* const grid : root.findChildren<QGridLayout*>())
    {
        for (int row = 0; row < grid->rowCount(); ++row)
        {
            const auto* const labelItem = grid->itemAtPosition(row, 0);
            auto* const label = labelItem
                ? qobject_cast<QLabel*>(labelItem->widget())
                : nullptr;
            if (!label || label->text() != labelText)
            {
                continue;
            }

            const auto* const valueItem = grid->itemAtPosition(row, 1);
            return valueItem
                ? qobject_cast<QLabel*>(valueItem->widget())
                : nullptr;
        }
    }
    return nullptr;
}

QTextEdit* textEditAfterLabel(
    SectionCard& card,
    const QString& labelText
    )
{
    for (QLabel* const label : card.findChildren<QLabel*>(
             QString(), Qt::FindDirectChildrenOnly))
    {
        if (label->text() != labelText)
        {
            continue;
        }

        QVBoxLayout* const layout = card.contentLayout();
        const int index = layout ? layout->indexOf(label) : -1;
        const QLayoutItem* const next = layout && index >= 0
            ? layout->itemAt(index + 1)
            : nullptr;
        return next ? qobject_cast<QTextEdit*>(next->widget()) : nullptr;
    }
    return nullptr;
}

QStringList rolesInTabOrder(
    NavigationTabWidget& tabs,
    const QHash<int, QString>& classRoles
    )
{
    QStringList roles;
    for (int index = 0; index < tabs.count(); ++index)
    {
        QWidget* const tabPage = tabs.widget(index);
        const int classId = tabPage
            ? tabPage->property("class_id").toInt()
            : -1;
        roles.append(classRoles.value(classId, QStringLiteral("unknown")));
    }
    return roles;
}

struct VisibleSummary final
{
    QString role;
    QString tab;
    QString title;
    QString students;
    QJsonValue regular = QJsonValue::Null;
    QJsonValue intensive = QJsonValue::Null;
    QJsonValue schedule = QJsonValue::Null;
    QString classNotes;
    QString timeFiller;
    bool complete = false;

    QJsonObject toJson() const
    {
        return {
            {QStringLiteral("class-notes"), classNotes},
            {QStringLiteral("class-title"), title},
            {QStringLiteral("intensive"), intensive},
            {QStringLiteral("regular"), regular},
            {QStringLiteral("role"), role},
            {QStringLiteral("schedule"), schedule},
            {QStringLiteral("students"), students},
            {QStringLiteral("tab"), tab},
            {QStringLiteral("time-filler"), timeFiller}
        };
    }
};

VisibleSummary summaryFor(
    NavigationTabWidget& tabs,
    const int tabIndex,
    const QHash<int, QString>& classRoles
    )
{
    VisibleSummary summary;
    if (tabIndex < 0 || tabIndex >= tabs.count())
    {
        return summary;
    }

    QWidget* const tabPage = tabs.widget(tabIndex);
    if (!tabPage)
    {
        return summary;
    }

    summary.role = classRoles.value(
        tabPage->property("class_id").toInt(),
        QStringLiteral("unknown")
        );
    summary.tab = tabs.tabText(tabIndex);

    const auto cards = tabPage->findChildren<SectionCard*>();
    if (cards.size() != 1)
    {
        return summary;
    }
    SectionCard* const classCard = cards.first();

    const auto titles = classCard->findChildren<QLabel*>(
        QStringLiteral("sectionTitle"), Qt::FindDirectChildrenOnly
        );
    if (titles.size() != 1)
    {
        return summary;
    }
    summary.title = titles.first()->text();

    const auto* const students = rowValueFor(
        *classCard, QStringLiteral("# of Students")
        );
    const auto* const regular = rowValueFor(
        *classCard, QStringLiteral("Regular")
        );
    const auto* const intensive = rowValueFor(
        *classCard, QStringLiteral("Intensive")
        );
    const auto* const schedule = rowValueFor(
        *classCard, QStringLiteral("Schedule")
        );
    QTextEdit* const classNotes = textEditAfterLabel(
        *classCard, QStringLiteral("Class Notes")
        );
    QTextEdit* const timeFiller = textEditAfterLabel(
        *classCard, QStringLiteral("Time Filler Activities")
        );

    if (!students || !classNotes || !timeFiller)
    {
        return summary;
    }

    summary.students = students->text();
    if (regular)
    {
        summary.regular = regular->text();
    }
    if (intensive)
    {
        summary.intensive = intensive->text();
    }
    if (schedule)
    {
        summary.schedule = schedule->text();
    }
    summary.classNotes = classNotes->toPlainText();
    summary.timeFiller = timeFiller->toPlainText();
    summary.complete = true;
    return summary;
}

QJsonArray summariesInTabOrder(
    NavigationTabWidget& tabs,
    const QHash<int, QString>& classRoles,
    bool* complete
    )
{
    QJsonArray summaries;
    *complete = true;
    for (int index = 0; index < tabs.count(); ++index)
    {
        const VisibleSummary summary = summaryFor(tabs, index, classRoles);
        *complete = *complete && summary.complete;
        summaries.append(summary.toJson());
    }
    return summaries;
}

void logTranscript(const QJsonObject& transcript)
{
    const QString json = QString::fromUtf8(
        QJsonDocument(transcript).toJson(QJsonDocument::Compact)
        );
    QString ascii;
    for (const QChar character : json)
    {
        if (character.unicode() > 0x7F)
        {
            ascii += QStringLiteral("\\u%1").arg(
                static_cast<unsigned int>(character.unicode()),
                4,
                16,
                QLatin1Char('0')
                );
        }
        else
        {
            ascii += character;
        }
    }
    qInfo().noquote() << "F374_TRANSCRIPT" << ascii;
}

QJsonObject expectedSummary(
    const QString& role,
    const QString& tab,
    const QString& title,
    const QString& students,
    const QJsonValue& regular,
    const QJsonValue& intensive,
    const QJsonValue& schedule,
    const QString& notes,
    const QString& filler
    )
{
    return {
        {QStringLiteral("class-notes"), notes},
        {QStringLiteral("class-title"), title},
        {QStringLiteral("intensive"), intensive},
        {QStringLiteral("regular"), regular},
        {QStringLiteral("role"), role},
        {QStringLiteral("schedule"), schedule},
        {QStringLiteral("students"), students},
        {QStringLiteral("tab"), tab},
        {QStringLiteral("time-filler"), filler}
    };
}
}

class MyClassesPageSummaryParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void orderedTabsMapVisibleClassSummaries();
    void emptyStateAndClassInformationFailureKeepTheirVisibleProjection();
};

void MyClassesPageSummaryParityTests::orderedTabsMapVisibleClassSummaries()
{
    Fixture fixture;
    QVERIFY(fixture.open());

    const int artemisId = fixture.createClass(
        QStringLiteral("Artemis class"),
        QStringLiteral("E5"),
        QStringLiteral("Artemis")
        );
    const int perseusId = fixture.createClass(
        QStringLiteral("Perseus class"),
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        {},
        {{QStringLiteral("Wednesday"), QStringLiteral("1:00 PM"), QStringLiteral("1:55 PM")}}
        );
    const int theseusId = fixture.createClass(
        QStringLiteral("Theseus class"),
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        {
            {QStringLiteral("Tuesday"), QStringLiteral("11:05 am"), QStringLiteral("12:05 pm")},
            {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
        },
        {{QStringLiteral("Friday"), QStringLiteral("02:00 PM"), QStringLiteral("04:00 PM")}},
        QStringLiteral("Theseus notes\nSecond line"),
        QStringLiteral("Quiet reading\nWord games")
        );
    QVERIFY(artemisId > 0);
    QVERIFY(perseusId > 0);
    QVERIFY(theseusId > 0);

    QVERIFY(fixture.saveRoster(artemisId, {}));
    QVERIFY(fixture.saveRoster(perseusId, {QStringLiteral("Perseus student")}));
    QVERIFY(fixture.saveRoster(theseusId, {
        QStringLiteral("Theseus student one"),
        QStringLiteral("Theseus student two")
    }));

    MyClassesPage page(&fixture.services);
    page.resize(900, 700);
    page.refresh();
    page.show();
    QApplication::processEvents();

    NavigationTabWidget* const tabs = tabsFor(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 3);
    const QHash<int, QString> classRoles{
        {theseusId, QStringLiteral("theseus")},
        {perseusId, QStringLiteral("perseus")},
        {artemisId, QStringLiteral("artemis")}
    };
    QCOMPARE(rolesInTabOrder(*tabs, classRoles), QStringList({
        QStringLiteral("theseus"),
        QStringLiteral("perseus"),
        QStringLiteral("artemis")
    }));
    QCOMPARE(QStringList({tabs->tabText(0), tabs->tabText(1), tabs->tabText(2)}),
        QStringList({
            QStringLiteral("E4 Theseus • T 11:05; M 9:00"),
            QStringLiteral("E4 Perseus • Int W 1:00"),
            QStringLiteral("E5 Artemis • No time")
        }));

    bool summariesComplete = false;
    const QJsonArray summaries = summariesInTabOrder(
        *tabs, classRoles, &summariesComplete
        );
    QVERIFY(summariesComplete);
    const QJsonArray expected{
        expectedSummary(
            QStringLiteral("theseus"),
            QStringLiteral("E4 Theseus • T 11:05; M 9:00"),
            QStringLiteral("E4 - Theseus"),
            QStringLiteral("2"),
            QStringLiteral("Tues 11:05 am-12:05 pm; Mon 9:00 AM-9:45 AM"),
            QStringLiteral("Fri 02:00 PM-04:00 PM"),
            QJsonValue(QJsonValue::Null),
            QStringLiteral("Theseus notes\nSecond line"),
            QStringLiteral("Quiet reading\nWord games")
            ),
        expectedSummary(
            QStringLiteral("perseus"),
            QStringLiteral("E4 Perseus • Int W 1:00"),
            QStringLiteral("E4 - Perseus"),
            QStringLiteral("1"),
            QJsonValue(QJsonValue::Null),
            QStringLiteral("Wed 1:00 PM-1:55 PM"),
            QJsonValue(QJsonValue::Null),
            QString(),
            QStringLiteral("N/A")
            ),
        expectedSummary(
            QStringLiteral("artemis"),
            QStringLiteral("E5 Artemis • No time"),
            QStringLiteral("E5 - Artemis"),
            QStringLiteral("0"),
            QJsonValue(QJsonValue::Null),
            QJsonValue(QJsonValue::Null),
            QStringLiteral("N/A"),
            QString(),
            QStringLiteral("N/A")
            )
    };
    QCOMPARE(summaries, expected);
    logTranscript({
        {QStringLiteral("case"), QStringLiteral("ordered-summary")},
        {QStringLiteral("summaries"), summaries}
    });
}

void MyClassesPageSummaryParityTests::
emptyStateAndClassInformationFailureKeepTheirVisibleProjection()
{
    Fixture emptyFixture;
    QVERIFY(emptyFixture.open());
    MyClassesPage emptyPage(&emptyFixture.services);
    emptyPage.resize(900, 700);
    emptyPage.refresh();
    emptyPage.show();
    QApplication::processEvents();

    const auto emptyLabels = emptyPage.findChildren<QLabel*>();
    bool emptyStateVisible = false;
    for (QLabel* const label : emptyLabels)
    {
        if (label->text() == QStringLiteral("No classes available."))
        {
            emptyStateVisible = label->isVisible();
            break;
        }
    }
    QVERIFY(emptyStateVisible);
    QVERIFY(!tabsFor(emptyPage));
    logTranscript({
        {QStringLiteral("case"), QStringLiteral("empty")},
        {QStringLiteral("empty-state"), QStringLiteral("No classes available.")},
        {QStringLiteral("tabs"), 0}
    });

    Fixture failureFixture;
    QVERIFY(failureFixture.open());
    const int classId = failureFixture.createClass(
        QStringLiteral("Fallback Visible Class"), QString(), QString()
        );
    QVERIFY(classId > 0);
    QVERIFY(failureFixture.saveRoster(classId, {
        QStringLiteral("Fallback student one"),
        QStringLiteral("Fallback student two")
    }));

    QSqlQuery dropClassInfo(
        failureFixture.services.databaseSession()->database()
        );
    QVERIFY2(
        dropClassInfo.exec(QStringLiteral("DROP TABLE class_info")),
        qPrintable(dropClassInfo.lastError().text())
        );

    MyClassesPage failurePage(&failureFixture.services);
    failurePage.resize(900, 700);
    bool unexpectedWarning = false;
    QTimer::singleShot(0, &failurePage, [&unexpectedWarning] {
        if (QWidget* const active = QApplication::activeModalWidget())
        {
            unexpectedWarning = true;
            active->close();
        }
    });
    failurePage.refresh();
    failurePage.show();
    QApplication::processEvents();
    QVERIFY(!unexpectedWarning);

    NavigationTabWidget* const failureTabs = tabsFor(failurePage);
    QVERIFY(failureTabs);
    QCOMPARE(failureTabs->count(), 1);
    const QHash<int, QString> classRoles{
        {classId, QStringLiteral("class-info-failure")}
    };
    bool summaryComplete = false;
    const QJsonArray summaries = summariesInTabOrder(
        *failureTabs, classRoles, &summaryComplete
        );
    QVERIFY(summaryComplete);
    QCOMPARE(rolesInTabOrder(*failureTabs, classRoles),
        QStringList({QStringLiteral("class-info-failure")}));
    const QJsonArray expected{
        expectedSummary(
            QStringLiteral("class-info-failure"),
            QStringLiteral("Fallback Visible Class • No time"),
            QStringLiteral("Fallback Visible Class"),
            QStringLiteral("2"),
            QJsonValue(QJsonValue::Null),
            QJsonValue(QJsonValue::Null),
            QStringLiteral("N/A"),
            QString(),
            QStringLiteral("N/A")
            )
    };
    QCOMPARE(summaries, expected);
    logTranscript({
        {QStringLiteral("case"), QStringLiteral("class-info-failure")},
        {QStringLiteral("schema-drop"), QStringLiteral("class_info")},
        {QStringLiteral("summaries"), summaries}
    });
}

QTEST_MAIN(MyClassesPageSummaryParityTests)

#include "my_classes_page_summary_parity_tests.moc"
