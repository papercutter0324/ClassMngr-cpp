#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "features/calendar/academic_calendar_schedule.h"
#include "features/classes/evaluation_default_selection.h"
#include "next/platform/application_services_academic_calendar_schedule_preferences_port.h"
#include "next/platform/application_services_evaluation_default_policy_port.h"

#include <QJsonDocument>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("evaluation-default-selection-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createClass(
    ApplicationServices& services,
    const QString& name,
    const QString& grade
    )
{
    const Result<int> created = services.classService()->create(name);
    if (!created)
    {
        qWarning().noquote() << "Could not create integration-test class:"
                             << created.error();
        return created.value_or(-1);
    }

    ClassInfo info;
    info.classId = *created;
    info.classGrade = grade;
    info.classLevel = grade.startsWith(QLatin1Char('M'))
        ? QStringLiteral("Ursa")
        : QStringLiteral("Theseus");
    if (grade.startsWith(QLatin1Char('M')))
    {
        info.essayBook = QStringLiteral("N/A");
    }
    const Status saved = services.classService()->saveClassInfo(info);
    if (!saved)
    {
        qWarning().noquote() << "Could not save integration-test class info:"
                             << saved.error();
        return -1;
    }
    return *created;
}

bool saveSchedule(ApplicationServices& services)
{
    AcademicCalendarSchedule schedule;
    schedule.setYearSchedules(
        2026,
        schedule.yearSchedule(SchoolLevel::Elementary, 2026),
        schedule.yearSchedule(SchoolLevel::Middle, 2026)
        );

    const QByteArray serialized =
        QJsonDocument(schedule.toJson()).toJson(QJsonDocument::Compact);
    ClassMngr::Next::Platform::
        ApplicationServicesAcademicCalendarSchedulePreferencesPort port(
            services
            );
    port.write(serialized.toStdString());
    return schedule.hasSavedSchedules();
}

bool saveCurrentEvaluation(
    ApplicationServices& services,
    int classId,
    const QString& evaluationName
    )
{
    SpeakingEvalRows rows = SpeakingEval::emptyRows();
    rows[0][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
        QStringLiteral("Alice");
    rows[0][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
        QString::fromUtf8("\xEA\xB9\x80\xEB\xAF\xBC\xEC\x88\x98");
    return services.speakingEvaluationService()->saveEvaluation(
        classId,
        evaluationName,
        rows
        ).has_value();
}

void setSelectionPolicy(
    ApplicationServices& services,
    ClassMngr::Next::Application::EvaluationDefaultPolicy policy
    )
{
    ClassMngr::Next::Platform::
        ApplicationServicesEvaluationDefaultPolicyPort port(services);
    port.save(policy);
}

} // namespace

class EvaluationDefaultSelectionIntegrationTests final : public QObject
{
    Q_OBJECT

private slots:
    void productionPathUsesSavedPolicyScheduleAndEvaluations();
    void failedCurrentEvaluationReadReturnsNoDefault();
    void missingScheduleOrRequiredClassDataReturnsNoDefault();

private:
    QTemporaryDir m_directory;
};

void EvaluationDefaultSelectionIntegrationTests::
productionPathUsesSavedPolicyScheduleAndEvaluations()
{
    QVERIFY(m_directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(m_directory)));

    const int middleClass = createClass(
        services,
        QStringLiteral("Middle Fall Class"),
        QStringLiteral("M2")
        );
    const int elementaryClass = createClass(
        services,
        QStringLiteral("Elementary Summer Class"),
        QStringLiteral("E4")
        );
    QVERIFY(middleClass > 0);
    QVERIFY(elementaryClass > 0);
    QVERIFY(saveSchedule(services));
    setSelectionPolicy(
        services,
        Application::EvaluationDefaultPolicy::CurrentOrPreviousTerm
        );

    const QDate fixedDate(2026, 9, 7);
    QCOMPARE(
        EvaluationDefaultSelection::forClass(
            &services,
            middleClass,
            fixedDate
            ),
        QStringLiteral("Summer")
        );
    QCOMPARE(
        EvaluationDefaultSelection::forClass(
            &services,
            elementaryClass,
            fixedDate
            ),
        QStringLiteral("Speech Contest")
        );

    QVERIFY(saveCurrentEvaluation(
        services,
        middleClass,
        QStringLiteral("Fall")
        ));
    QVERIFY(saveCurrentEvaluation(
        services,
        elementaryClass,
        QStringLiteral("Summer")
        ));
    QCOMPARE(
        EvaluationDefaultSelection::forClass(
            &services,
            middleClass,
            fixedDate
            ),
        QStringLiteral("Fall")
        );
    QCOMPARE(
        EvaluationDefaultSelection::forClass(
            &services,
            elementaryClass,
            fixedDate
            ),
        QStringLiteral("Summer")
        );

    setSelectionPolicy(services, Application::EvaluationDefaultPolicy::All);
    QVERIFY(
        EvaluationDefaultSelection::forClass(
            &services,
            middleClass,
            fixedDate
            ).isEmpty()
        );
}

void EvaluationDefaultSelectionIntegrationTests::
failedCurrentEvaluationReadReturnsNoDefault()
{
    QVERIFY(m_directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(m_directory)));

    const int middleClass = createClass(
        services,
        QStringLiteral("Middle Class With Unavailable Evaluation Data"),
        QStringLiteral("M2")
        );
    QVERIFY(middleClass > 0);
    QVERIFY(saveSchedule(services));
    setSelectionPolicy(
        services,
        Application::EvaluationDefaultPolicy::CurrentOrPreviousTerm
        );

    const QDate fixedDate(2026, 9, 7);
    QCOMPARE(
        EvaluationDefaultSelection::forClass(&services, middleClass, fixedDate),
        QStringLiteral("Summer")
        );

    QVERIFY(services.dataService());
    QVERIFY(services.dataService()->databaseSession());
    QSqlQuery query(
        services.dataService()->databaseSession()->database()
        );
    QVERIFY(query.exec(QStringLiteral("DROP TABLE speaking_evaluations")));

    const Result<SpeakingEvalRows> failedEvaluation =
        services.speakingEvaluationService()->evaluation(
            middleClass,
            QStringLiteral("Fall")
            );
    QVERIFY(!failedEvaluation);
    QVERIFY(
        EvaluationDefaultSelection::forClass(
            &services,
            middleClass,
            fixedDate
            ).isEmpty()
        );
}

void EvaluationDefaultSelectionIntegrationTests::
missingScheduleOrRequiredClassDataReturnsNoDefault()
{
    QVERIFY(m_directory.isValid());
    ApplicationServices noScheduleServices;
    QVERIFY(noScheduleServices.openDatabase(databasePath(m_directory)));
    setSelectionPolicy(
        noScheduleServices,
        Application::EvaluationDefaultPolicy::CurrentOrPreviousTerm
        );
    const int noScheduleClass = createClass(
        noScheduleServices,
        QStringLiteral("No Saved Schedule"),
        QStringLiteral("M2")
        );
    QVERIFY(noScheduleClass > 0);
    QVERIFY(
        EvaluationDefaultSelection::forClass(
            &noScheduleServices,
            noScheduleClass,
            QDate(2026, 9, 7)
            ).isEmpty()
        );

    ApplicationServices unavailableClassInfoServices;
    QVERIFY(unavailableClassInfoServices.openDatabase(databasePath(m_directory)));
    setSelectionPolicy(
        unavailableClassInfoServices,
        Application::EvaluationDefaultPolicy::CurrentOrPreviousTerm
        );
    QVERIFY(saveSchedule(unavailableClassInfoServices));
    const int classWithUnavailableInfo = createClass(
        unavailableClassInfoServices,
        QStringLiteral("Unavailable Required Class Information"),
        QStringLiteral("M2")
    );
    QVERIFY(classWithUnavailableInfo > 0);
    QVERIFY(unavailableClassInfoServices.dataService());
    QVERIFY(unavailableClassInfoServices.dataService()->databaseSession());
    QSqlQuery query(
        unavailableClassInfoServices.dataService()
            ->databaseSession()->database()
        );
    QVERIFY(query.exec(QStringLiteral("DROP TABLE class_info")));
    QVERIFY(
        EvaluationDefaultSelection::forClass(
            &unavailableClassInfoServices,
            classWithUnavailableInfo,
            QDate(2026, 9, 7)
            ).isEmpty()
        );
}

QTEST_MAIN(EvaluationDefaultSelectionIntegrationTests)

#include "evaluation_default_selection_integration_tests.moc"
