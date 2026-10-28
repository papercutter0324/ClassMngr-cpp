#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "next/application/classes_list_read_snapshot.h"
#include "next/application/classes_navigation_snapshot.h"
#include "next/platform/workspace_store_classes_navigator_read_actions.h"

#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <algorithm>
#include <limits>
#include <string>

using namespace ClassMngr::Next;

namespace
{

QString fixturePath(QTemporaryDir& directory, const QString& filename)
{
    const QString destination = directory.filePath(filename);
    QFile::remove(destination);
    return QFile::copy(
        QStringLiteral(CLASSMNGR_CLASSES_NAVIGATOR_FIXTURE), destination)
        ? destination
        : QString{};
}

Application::ClassesNavigationSnapshotQuery queryFor(
    const Application::ClassesListSnapshot& classes
    )
{
    Application::ClassesNavigationSnapshotQuery query;
    query.classes.reserve(classes.classes.size());
    for (const auto& classroom : classes.classes)
    {
        query.classes.push_back({
            .classId = classroom.classId,
            .className = classroom.className
        });
    }
    return query;
}

QList<ClassNavigationBoundedReadRequestEntry> repositoryRequestFor(
    const QList<Classroom>& classes
    )
{
    QList<ClassNavigationBoundedReadRequestEntry> request;
    request.reserve(classes.size());
    for (const Classroom& classroom : classes)
    {
        request.append({
            .classId = classroom.id,
            .className = classroom.name
        });
    }
    return request;
}

ClassNavigationBoundedReadLimits repositoryReadLimits()
{
    return {
        .maximumClassCount = static_cast<int>(
            Application::kClassesNavigationMaximumClasses),
        .maximumTextLength = static_cast<int>(
            Application::kClassesNavigationMaximumTextLength),
        .maximumScheduleRowsPerSourceAndClass = static_cast<int>(
            Application::kClassesNavigationMaximumScheduleRows),
        .maximumScheduleTokenLength = static_cast<int>(
            Application::kClassesNavigationMaximumScheduleTokenLength)
    };
}

} // namespace

class ClassesNavigatorWorkspaceStoreTests final : public QObject
{
    Q_OBJECT

private slots:
    void readsBoundedListAndOrderedNavigationSummaries();
    void rejectsChangedIdentityAndOverLimitRequests();
    void rejectsMoreThanFourThousandNinetySixClasses();
    void rejectsMoreThanFourteenScheduleRowsPerSourceAndClass();
    void rejectsOverlongClassNamesAndScheduleTokens();
    void readsAndPersistsNavigatorPreferences();
    void repositoryClassListPreservesOrderAndExcludesTestingClasses();
    void repositoryClassListAcceptsExactCountBoundAndRejectsOneOver();
    void repositoryTextFieldsAcceptExactBoundAndRejectOneOver();
    void repositoryNavigationAcceptsExactScheduleAndTokenBounds();
    void repositoryNavigationRejectsScheduleOverflowAndIdentityChanges();
    void repositoryNavigationKeepsSchedulesUnderClassInfoParents();
    void repositoryReadFailuresRemainTyped();
};

void ClassesNavigatorWorkspaceStoreTests::
readsBoundedListAndOrderedNavigationSummaries()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("navigator.tps"));
    QVERIFY(!path.isEmpty());

    const auto list = Platform::readClassesNavigatorListAtPath(path);
    QVERIFY(list);
    QCOMPARE(list.value().classes.size(), std::size_t(96));
    QVERIFY(std::is_sorted(list.value().classes.cbegin(), list.value().classes.cend(),
        [](const auto& left, const auto& right) {
            return QString::compare(
                QString::fromStdU16String(left.className),
                QString::fromStdU16String(right.className),
                Qt::CaseSensitive) < 0;
        }));

    const auto query = queryFor(list.value());
    const auto navigation =
        Platform::readClassesNavigatorNavigationAtPath(path, query);
    QVERIFY(navigation);
    QCOMPARE(navigation.value().classes.size(), query.classes.size());
    int totalScheduleRows = 0;
    for (std::size_t index = 0; index < query.classes.size(); ++index)
    {
        const auto& requested = query.classes[index];
        const auto& result = navigation.value().classes[index];
        QCOMPARE(result.classId, requested.classId);
        QCOMPARE(result.className, requested.className);
        QVERIFY(result.details);
        QVERIFY(result.details.value().regularSchedule.size()
            <= Application::kClassesNavigationMaximumScheduleRows);
        QVERIFY(result.details.value().intensiveSchedule.size()
            <= Application::kClassesNavigationMaximumScheduleRows);
        totalScheduleRows += static_cast<int>(
            result.details.value().regularSchedule.size()
            + result.details.value().intensiveSchedule.size());
    }
    QVERIFY(totalScheduleRows <= 2 * 96
        * static_cast<int>(Application::kClassesNavigationMaximumScheduleRows));
}

void ClassesNavigatorWorkspaceStoreTests::
rejectsChangedIdentityAndOverLimitRequests()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("identity.tps"));
    QVERIFY(!path.isEmpty());
    const auto list = Platform::readClassesNavigatorListAtPath(path);
    QVERIFY(list);

    auto reordered = queryFor(list.value());
    std::reverse(reordered.classes.begin(), reordered.classes.end());
    const auto changed =
        Platform::readClassesNavigatorNavigationAtPath(path, reordered);
    QVERIFY(!changed);
    QCOMPARE(changed.error().code, Domain::ErrorCode::Validation);

    Application::ClassesNavigationSnapshotQuery tooMany;
    tooMany.classes.reserve(Application::kClassesNavigationMaximumClasses + 1);
    for (std::size_t index = 0;
         index <= Application::kClassesNavigationMaximumClasses; ++index)
    {
        const auto id = Domain::ClassId::fromString(
            std::to_string(index + 1));
        QVERIFY(id.has_value());
        tooMany.classes.push_back({.classId = *id, .className = u"Class"});
    }
    const auto overLimit =
        Platform::readClassesNavigatorNavigationAtPath(path, tooMany);
    QVERIFY(!overLimit);
    QCOMPARE(overLimit.error().code, Domain::ErrorCode::Validation);
}

void ClassesNavigatorWorkspaceStoreTests::
rejectsMoreThanFourThousandNinetySixClasses()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("class-limit.tps"));
    QVERIFY(!path.isEmpty());
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery insert(store.database());
        QVERIFY(insert.prepare(QStringLiteral(
            "INSERT INTO classes(name) VALUES(?)")));
        for (std::size_t index = 0;
             index < Application::kClassesNavigationMaximumClasses; ++index)
        {
            insert.bindValue(0, QStringLiteral("Overflow %1")
                .arg(static_cast<qulonglong>(index)));
            QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
        }
    }

    const auto overLimit = Platform::readClassesNavigatorListAtPath(path);
    QVERIFY(!overLimit);
    QCOMPARE(overLimit.error().code, Domain::ErrorCode::Validation);
}

void ClassesNavigatorWorkspaceStoreTests::
rejectsMoreThanFourteenScheduleRowsPerSourceAndClass()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("schedule-bound.tps"));
    QVERIFY(!path.isEmpty());
    int targetId = 0;
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery classQuery(store.database());
        QVERIFY(classQuery.exec(QStringLiteral(
            "SELECT id, name FROM classes ORDER BY name LIMIT 1")));
        QVERIFY(classQuery.next());
        bool idOk = false;
        targetId = classQuery.value(0).toInt(&idOk);
        QVERIFY(idOk && targetId > 0);
        QSqlQuery insert(store.database());
        QVERIFY(insert.prepare(QStringLiteral(
            "INSERT INTO class_times(class_id, day, start_time, end_time) "
            "VALUES(?, ?, ?, ?)")));
        for (int row = 0; row < 15; ++row)
        {
            insert.bindValue(0, targetId);
            insert.bindValue(1, QStringLiteral("Monday"));
            insert.bindValue(2, QStringLiteral("%1:00").arg(row + 1));
            insert.bindValue(3, QStringLiteral("%1:50").arg(row + 1));
            QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
        }
    }

    const auto list = Platform::readClassesNavigatorListAtPath(path);
    QVERIFY(list);
    const auto navigation =
        Platform::readClassesNavigatorNavigationAtPath(path, queryFor(list.value()));
    QVERIFY(!navigation);
    QCOMPARE(navigation.error().code, Domain::ErrorCode::Validation);
}

void ClassesNavigatorWorkspaceStoreTests::
rejectsOverlongClassNamesAndScheduleTokens()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString namePath = fixturePath(directory, QStringLiteral("name-bound.tps"));
    QVERIFY(!namePath.isEmpty());
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(namePath));
        QSqlQuery update(store.database());
        QVERIFY(update.prepare(QStringLiteral(
            "UPDATE classes SET name = ? WHERE id = "
            "(SELECT id FROM classes ORDER BY name LIMIT 1)")));
        update.addBindValue(QString(
            static_cast<qsizetype>(Application::kClassesNavigationMaximumTextLength + 1),
            QLatin1Char('x')));
        QVERIFY2(update.exec(), qPrintable(update.lastError().text()));
    }
    const auto longName = Platform::readClassesNavigatorListAtPath(namePath);
    QVERIFY(!longName);
    QCOMPARE(longName.error().code, Domain::ErrorCode::Validation);

    const QString tokenPath = fixturePath(directory, QStringLiteral("token-bound.tps"));
    QVERIFY(!tokenPath.isEmpty());
    int targetId = 0;
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(tokenPath));
        QSqlQuery classQuery(store.database());
        QVERIFY(classQuery.exec(QStringLiteral(
            "SELECT id FROM classes ORDER BY name LIMIT 1")));
        QVERIFY(classQuery.next());
        bool idOk = false;
        targetId = classQuery.value(0).toInt(&idOk);
        QVERIFY(idOk && targetId > 0);

        QSqlQuery removeSchedule(store.database());
        QVERIFY(removeSchedule.prepare(QStringLiteral(
            "DELETE FROM class_times WHERE class_id = ?")));
        removeSchedule.addBindValue(targetId);
        QVERIFY2(removeSchedule.exec(),
            qPrintable(removeSchedule.lastError().text()));

        QSqlQuery insert(store.database());
        QVERIFY(insert.prepare(QStringLiteral(
            "INSERT INTO class_times(class_id, day, start_time, end_time) "
            "VALUES(?, ?, ?, ?)")));
        insert.addBindValue(targetId);
        insert.addBindValue(QString(
            static_cast<qsizetype>(
                Application::kClassesNavigationMaximumScheduleTokenLength + 1),
            QLatin1Char('x')));
        insert.addBindValue(QStringLiteral("09:00"));
        insert.addBindValue(QStringLiteral("09:50"));
        QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
    }
    const auto list = Platform::readClassesNavigatorListAtPath(tokenPath);
    QVERIFY(list);
    const auto navigation = Platform::readClassesNavigatorNavigationAtPath(
        tokenPath, queryFor(list.value()));
    QVERIFY(!navigation);
    QCOMPARE(navigation.error().code, Domain::ErrorCode::Validation);
}

void ClassesNavigatorWorkspaceStoreTests::
readsAndPersistsNavigatorPreferences()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("preferences.tps"));
    QVERIFY(!path.isEmpty());

    const auto defaults = Platform::readClassesNavigatorPreferencesAtPath(path);
    QVERIFY(defaults.scheduleDisplayMode.has_value());
    QCOMPARE(*defaults.scheduleDisplayMode, Application::ScheduleDisplayMode::Regular);
    QVERIFY(defaults.visibilityScope.has_value());
    QCOMPARE(*defaults.visibilityScope, Application::ClassVisibilityScope::ActiveSchedule);
    QVERIFY(defaults.dayFilterResetPolicy.has_value());
    QCOMPARE(*defaults.dayFilterResetPolicy,
        Application::ClassDayFilterResetPolicy::OnApplicationClose);
    QVERIFY(defaults.classSelectionResetPolicy.has_value());
    QCOMPARE(*defaults.classSelectionResetPolicy,
        Application::ClassSelectionResetPolicy::OnApplicationClose);

    QVERIFY(Platform::saveClassesNavigatorScheduleModeAtPath(
        path, Application::ScheduleDisplayMode::Intensive));
    QVERIFY(Platform::saveClassesNavigatorVisibilityAtPath(
        path, Application::ClassVisibilityScope::AllClasses));
    const auto saved = Platform::readClassesNavigatorPreferencesAtPath(path);
    QCOMPARE(*saved.scheduleDisplayMode, Application::ScheduleDisplayMode::Intensive);
    QCOMPARE(*saved.visibilityScope, Application::ClassVisibilityScope::AllClasses);
}

void ClassesNavigatorWorkspaceStoreTests::
repositoryClassListPreservesOrderAndExcludesTestingClasses()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("repository-order.tps"));
    QVERIFY(!path.isEmpty());

    WorkspaceStore store;
    QVERIFY(store.openExisting(path));
    QSqlQuery insert(store.database());
    QVERIFY(insert.prepare(QStringLiteral("INSERT INTO classes(name) VALUES(?)")));
    insert.bindValue(0, QStringLiteral("Navigator duplicate"));
    QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
    const int firstDuplicateId = insert.lastInsertId().toInt();
    insert.bindValue(0, QStringLiteral("Navigator duplicate"));
    QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
    const int secondDuplicateId = insert.lastInsertId().toInt();
    insert.bindValue(0, QStringLiteral("Navigator duplicate"));
    QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
    const int thirdDuplicateId = insert.lastInsertId().toInt();

    QSqlQuery markTesting(store.database());
    QVERIFY(markTesting.prepare(QStringLiteral(
        "INSERT INTO testing_classes(class_id, room) VALUES(?, ?)")));
    markTesting.addBindValue(thirdDuplicateId);
    markTesting.addBindValue(QStringLiteral("Test room"));
    QVERIFY2(markTesting.exec(), qPrintable(markTesting.lastError().text()));

    ClassRepository* const repository = store.classRepository();
    QVERIFY(repository);
    const auto classes = repository->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(classes);

    const auto firstPosition = std::find_if(
        classes->cbegin(), classes->cend(), [firstDuplicateId](const Classroom& item) {
            return item.id == firstDuplicateId;
        });
    const auto secondPosition = std::find_if(
        classes->cbegin(), classes->cend(), [secondDuplicateId](const Classroom& item) {
            return item.id == secondDuplicateId;
        });
    QVERIFY(firstPosition != classes->cend());
    QVERIFY(secondPosition != classes->cend());
    QVERIFY(firstPosition < secondPosition);
    QCOMPARE(secondPosition->name, QStringLiteral("Navigator duplicate"));

    QSqlQuery actualIds(store.database());
    QVERIFY(actualIds.exec(QStringLiteral(
        "SELECT id FROM classes WHERE name = 'Navigator duplicate' ORDER BY id")));
    QVERIFY(actualIds.next());
    QCOMPARE(firstPosition->id, actualIds.value(0).toInt());
    QVERIFY(actualIds.next());
    QCOMPARE(secondPosition->id, actualIds.value(0).toInt());
    const auto testingPosition = std::find_if(
        classes->cbegin(), classes->cend(), [thirdDuplicateId](const Classroom& item) {
            return item.id == thirdDuplicateId;
        });
    QVERIFY(testingPosition == classes->cend());
}

void ClassesNavigatorWorkspaceStoreTests::
repositoryClassListAcceptsExactCountBoundAndRejectsOneOver()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("repository-count.tps"));
    QVERIFY(!path.isEmpty());

    WorkspaceStore store;
    QVERIFY(store.openExisting(path));
    ClassRepository* const repository = store.classRepository();
    QVERIFY(repository);
    const auto initial = repository->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(initial);
    QCOMPARE(initial->size(), qsizetype(96));

    QSqlQuery insert(store.database());
    QVERIFY(insert.prepare(QStringLiteral("INSERT INTO classes(name) VALUES(?)")));
    for (int index = 0; index < 4000; ++index)
    {
        insert.bindValue(0, QStringLiteral("Repository bound %1").arg(index));
        QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
    }
    const auto exact = repository->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(exact);
    QCOMPARE(exact->size(), qsizetype(4096));
    ClassInfoRepository* const infoRepository = store.classInfoRepository();
    QVERIFY(infoRepository);
    const auto exactNavigation =
        infoRepository->loadBoundedClassesNavigationRecords(
            repositoryRequestFor(*exact), repositoryReadLimits());
    QVERIFY(exactNavigation);
    QCOMPARE(exactNavigation->size(), qsizetype(4096));

    insert.bindValue(0, QStringLiteral("Repository bound overflow"));
    QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
    const auto over = repository->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(!over);
    QCOMPARE(over.error().code, RepositoryErrorCode::Validation);
    const auto navigationOver =
        infoRepository->loadBoundedClassesNavigationRecords(
            repositoryRequestFor(*exact), repositoryReadLimits());
    QVERIFY(!navigationOver);
    QCOMPARE(navigationOver.error().code, RepositoryErrorCode::Validation);
}

void ClassesNavigatorWorkspaceStoreTests::
repositoryTextFieldsAcceptExactBoundAndRejectOneOver()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("repository-text.tps"));
    QVERIFY(!path.isEmpty());

    WorkspaceStore store;
    QVERIFY(store.openExisting(path));
    QSqlQuery targetQuery(store.database());
    QVERIFY(targetQuery.exec(QStringLiteral(
        "SELECT c.id FROM classes c INNER JOIN class_info ci "
        "ON ci.class_id = c.id ORDER BY c.id LIMIT 1")));
    QVERIFY(targetQuery.next());
    const int targetId = targetQuery.value(0).toInt();
    QVERIFY(targetId > 0);

    const QString exactText(
        static_cast<qsizetype>(Application::kClassesNavigationMaximumTextLength),
        QLatin1Char('x'));
    const QString overText(
        static_cast<qsizetype>(Application::kClassesNavigationMaximumTextLength + 1),
        QLatin1Char('x'));
    QSqlQuery updateName(store.database());
    QVERIFY(updateName.prepare(QStringLiteral(
        "UPDATE classes SET name = ? WHERE id = ?")));
    updateName.addBindValue(exactText);
    updateName.addBindValue(targetId);
    QVERIFY2(updateName.exec(), qPrintable(updateName.lastError().text()));

    ClassRepository* const classRepository = store.classRepository();
    QVERIFY(classRepository);
    auto classes = classRepository->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(classes);
    const auto target = std::find_if(
        classes->cbegin(), classes->cend(), [targetId](const Classroom& item) {
            return item.id == targetId;
        });
    QVERIFY(target != classes->cend());
    QCOMPARE(target->name.size(),
        static_cast<qsizetype>(Application::kClassesNavigationMaximumTextLength));

    QSqlQuery updateGrade(store.database());
    QVERIFY(updateGrade.prepare(QStringLiteral(
        "UPDATE class_info SET class_grade = ? WHERE class_id = ?")));
    updateGrade.addBindValue(exactText);
    updateGrade.addBindValue(targetId);
    QVERIFY2(updateGrade.exec(), qPrintable(updateGrade.lastError().text()));
    ClassInfoRepository* const infoRepository = store.classInfoRepository();
    QVERIFY(infoRepository);
    auto request = repositoryRequestFor(*classes);
    auto summaries = infoRepository->loadBoundedClassesNavigationRecords(
        request, repositoryReadLimits());
    QVERIFY(summaries);
    const auto summary = std::find_if(
        summaries->cbegin(), summaries->cend(), [targetId](const auto& item) {
            return item.details.classId == targetId;
        });
    QVERIFY(summary != summaries->cend());
    QCOMPARE(summary->details.grade.size(), exactText.size());

    updateGrade.bindValue(0, overText);
    updateGrade.bindValue(1, targetId);
    QVERIFY2(updateGrade.exec(), qPrintable(updateGrade.lastError().text()));
    summaries = infoRepository->loadBoundedClassesNavigationRecords(
        request, repositoryReadLimits());
    QVERIFY(!summaries);
    QCOMPARE(summaries.error().code, RepositoryErrorCode::Validation);

    updateName.bindValue(0, overText);
    updateName.bindValue(1, targetId);
    QVERIFY2(updateName.exec(), qPrintable(updateName.lastError().text()));
    classes = classRepository->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(!classes);
    QCOMPARE(classes.error().code, RepositoryErrorCode::Validation);
}

void ClassesNavigatorWorkspaceStoreTests::
repositoryNavigationAcceptsExactScheduleAndTokenBounds()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("repository-schedule-exact.tps"));
    QVERIFY(!path.isEmpty());

    WorkspaceStore store;
    QVERIFY(store.openExisting(path));
    ClassRepository* const classRepository = store.classRepository();
    ClassInfoRepository* const infoRepository = store.classInfoRepository();
    QVERIFY(classRepository);
    QVERIFY(infoRepository);
    const auto classes = classRepository->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(classes);
    QVERIFY(!classes->isEmpty());

    QSqlQuery targetQuery(store.database());
    QVERIFY(targetQuery.exec(QStringLiteral(
        "SELECT class_id FROM class_info ORDER BY class_id LIMIT 1")));
    QVERIFY(targetQuery.next());
    const int targetId = targetQuery.value(0).toInt();
    QVERIFY(targetId > 0);
    QSqlQuery removeRegular(store.database());
    QVERIFY(removeRegular.prepare(QStringLiteral(
        "DELETE FROM class_times WHERE class_id = ?")));
    removeRegular.addBindValue(targetId);
    QVERIFY2(removeRegular.exec(), qPrintable(removeRegular.lastError().text()));
    QSqlQuery removeIntensive(store.database());
    QVERIFY(removeIntensive.prepare(QStringLiteral(
        "DELETE FROM class_intensive_times WHERE class_id = ?")));
    removeIntensive.addBindValue(targetId);
    QVERIFY2(removeIntensive.exec(), qPrintable(removeIntensive.lastError().text()));

    const QString exactToken(
        static_cast<qsizetype>(
            Application::kClassesNavigationMaximumScheduleTokenLength),
        QLatin1Char('d'));
    for (const QString table : {
             QStringLiteral("class_times"),
             QStringLiteral("class_intensive_times")})
    {
        QSqlQuery insert(store.database());
        QVERIFY(insert.prepare(QStringLiteral(
            "INSERT INTO %1(class_id, day, start_time, end_time) "
            "VALUES(?, ?, ?, ?)" ).arg(table)));
        for (int row = 0; row < 14; ++row)
        {
            insert.bindValue(0, targetId);
            insert.bindValue(1, exactToken);
            insert.bindValue(2, QStringLiteral("09:%1").arg(row, 2, 10, QLatin1Char('0')));
            insert.bindValue(3, QStringLiteral("10:%1").arg(row, 2, 10, QLatin1Char('0')));
            QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
        }
    }

    const auto summaries = infoRepository->loadBoundedClassesNavigationRecords(
        repositoryRequestFor(*classes), repositoryReadLimits());
    QVERIFY(summaries);
    const auto summary = std::find_if(
        summaries->cbegin(), summaries->cend(), [targetId](const auto& item) {
            return item.details.classId == targetId;
        });
    QVERIFY(summary != summaries->cend());
    QCOMPARE(summary->details.regularTimes.size(), qsizetype(14));
    QCOMPARE(summary->details.intensiveTimes.size(), qsizetype(14));
    QCOMPARE(summary->details.regularTimes.front().day.size(), exactToken.size());
}

void ClassesNavigatorWorkspaceStoreTests::
repositoryNavigationRejectsScheduleOverflowAndIdentityChanges()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("repository-schedule-overflow.tps"));
    QVERIFY(!path.isEmpty());

    WorkspaceStore store;
    QVERIFY(store.openExisting(path));
    ClassRepository* const classRepository = store.classRepository();
    ClassInfoRepository* const infoRepository = store.classInfoRepository();
    QVERIFY(classRepository);
    QVERIFY(infoRepository);
    const auto classes = classRepository->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(classes);
    const auto request = repositoryRequestFor(*classes);
    auto reversed = request;
    std::reverse(reversed.begin(), reversed.end());
    auto changed = infoRepository->loadBoundedClassesNavigationRecords(
        reversed, repositoryReadLimits());
    QVERIFY(!changed);
    QCOMPARE(changed.error().code, RepositoryErrorCode::Validation);

    QList<ClassNavigationBoundedReadRequestEntry> tooMany;
    tooMany.reserve(4097);
    for (int index = 1; index <= 4097; ++index)
    {
        tooMany.append({
            .classId = index,
            .className = QStringLiteral("Class %1").arg(index)
        });
    }
    changed = infoRepository->loadBoundedClassesNavigationRecords(
        tooMany, repositoryReadLimits());
    QVERIFY(!changed);
    QCOMPARE(changed.error().code, RepositoryErrorCode::Validation);

    auto invalidId = request;
    invalidId.front().classId = 0;
    changed = infoRepository->loadBoundedClassesNavigationRecords(
        invalidId, repositoryReadLimits());
    QVERIFY(!changed);
    QCOMPARE(changed.error().code, RepositoryErrorCode::InvalidInput);

    const int targetId = classes->front().id;
    QSqlQuery remove(store.database());
    QVERIFY(remove.prepare(QStringLiteral(
        "DELETE FROM class_times WHERE class_id = ?")));
    remove.addBindValue(targetId);
    QVERIFY2(remove.exec(), qPrintable(remove.lastError().text()));
    QSqlQuery insert(store.database());
    QVERIFY(insert.prepare(QStringLiteral(
        "INSERT INTO class_times(class_id, day, start_time, end_time) "
        "VALUES(?, ?, ?, ?)")));
    for (int row = 0; row < 15; ++row)
    {
        insert.bindValue(0, targetId);
        insert.bindValue(1, QStringLiteral("Monday"));
        insert.bindValue(2, QStringLiteral("09:%1").arg(row, 2, 10, QLatin1Char('0')));
        insert.bindValue(3, QStringLiteral("10:%1").arg(row, 2, 10, QLatin1Char('0')));
        QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
    }
    changed = infoRepository->loadBoundedClassesNavigationRecords(
        request, repositoryReadLimits());
    QVERIFY(!changed);
    QCOMPARE(changed.error().code, RepositoryErrorCode::Validation);

    QSqlQuery removeOverflow(store.database());
    QVERIFY(removeOverflow.prepare(QStringLiteral(
        "DELETE FROM class_times WHERE class_id = ?")));
    removeOverflow.addBindValue(targetId);
    QVERIFY2(removeOverflow.exec(), qPrintable(removeOverflow.lastError().text()));
    QSqlQuery insertLongToken(store.database());
    QVERIFY(insertLongToken.prepare(QStringLiteral(
        "INSERT INTO class_times(class_id, day, start_time, end_time) "
        "VALUES(?, ?, ?, ?)")));
    insertLongToken.addBindValue(targetId);
    insertLongToken.addBindValue(QString(
        static_cast<qsizetype>(
            Application::kClassesNavigationMaximumScheduleTokenLength + 1),
        QLatin1Char('d')));
    insertLongToken.addBindValue(QStringLiteral("09:00"));
    insertLongToken.addBindValue(QStringLiteral("09:50"));
    QVERIFY2(insertLongToken.exec(), qPrintable(insertLongToken.lastError().text()));
    changed = infoRepository->loadBoundedClassesNavigationRecords(
        request, repositoryReadLimits());
    QVERIFY(!changed);
    QCOMPARE(changed.error().code, RepositoryErrorCode::Validation);
}

void ClassesNavigatorWorkspaceStoreTests::
repositoryNavigationKeepsSchedulesUnderClassInfoParents()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("repository-parent.tps"));
    QVERIFY(!path.isEmpty());

    WorkspaceStore store;
    QVERIFY(store.openExisting(path));
    QSqlQuery addClass(store.database());
    QVERIFY(addClass.prepare(QStringLiteral("INSERT INTO classes(name) VALUES(?)")));
    addClass.addBindValue(QStringLiteral("Class without information"));
    QVERIFY2(addClass.exec(), qPrintable(addClass.lastError().text()));
    const int classId = addClass.lastInsertId().toInt();
    QVERIFY(classId > 0);

    QSqlQuery addSchedule(store.database());
    QVERIFY(addSchedule.prepare(QStringLiteral(
        "INSERT INTO class_times(class_id, day, start_time, end_time) "
        "VALUES(?, ?, ?, ?)")));
    addSchedule.addBindValue(classId);
    addSchedule.addBindValue(QStringLiteral("Monday"));
    addSchedule.addBindValue(QStringLiteral("09:00"));
    addSchedule.addBindValue(QStringLiteral("09:50"));
    QVERIFY2(addSchedule.exec(), qPrintable(addSchedule.lastError().text()));

    ClassRepository* const classRepository = store.classRepository();
    ClassInfoRepository* const infoRepository = store.classInfoRepository();
    QVERIFY(classRepository);
    QVERIFY(infoRepository);
    const auto classes = classRepository->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(classes);
    const auto summaries = infoRepository->loadBoundedClassesNavigationRecords(
        repositoryRequestFor(*classes), repositoryReadLimits());
    QVERIFY(summaries);
    const auto summary = std::find_if(
        summaries->cbegin(), summaries->cend(), [classId](const auto& item) {
            return item.details.classId == classId;
        });
    QVERIFY(summary != summaries->cend());
    QVERIFY(!summary->details.hasClassInfo);
    QVERIFY(summary->details.regularTimes.isEmpty());
    QVERIFY(summary->details.intensiveTimes.isEmpty());
}

void ClassesNavigatorWorkspaceStoreTests::repositoryReadFailuresRemainTyped()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString listPath = fixturePath(directory, QStringLiteral("repository-list-failure.tps"));
    QVERIFY(!listPath.isEmpty());
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(listPath));
        QSqlQuery drop(store.database());
        QVERIFY2(drop.exec(QStringLiteral("DROP TABLE testing_classes")),
            qPrintable(drop.lastError().text()));
        ClassRepository* const repository = store.classRepository();
        QVERIFY(repository);
        const auto result = repository->getBoundedClassesForNavigation(4096, 4096);
        QVERIFY(!result);
        QCOMPARE(result.error().code, RepositoryErrorCode::Technical);
    }

    const QString navigationPath = fixturePath(
        directory, QStringLiteral("repository-navigation-failure.tps"));
    QVERIFY(!navigationPath.isEmpty());
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(navigationPath));
        ClassRepository* const classRepository = store.classRepository();
        ClassInfoRepository* const infoRepository = store.classInfoRepository();
        QVERIFY(classRepository);
        QVERIFY(infoRepository);
        const auto classes = classRepository->getBoundedClassesForNavigation(
            4096, 4096);
        QVERIFY(classes);
        QSqlQuery drop(store.database());
        QVERIFY2(drop.exec(QStringLiteral("DROP TABLE class_times")),
            qPrintable(drop.lastError().text()));
        const auto result = infoRepository->loadBoundedClassesNavigationRecords(
            repositoryRequestFor(*classes), repositoryReadLimits());
        QVERIFY(!result);
        QCOMPARE(result.error().code, RepositoryErrorCode::Technical);
    }
}

QTEST_GUILESS_MAIN(ClassesNavigatorWorkspaceStoreTests)
#include "classes_navigator_workspace_store_tests.moc"
