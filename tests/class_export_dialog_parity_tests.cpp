#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/utils/sidebar_node_naming.h"
#include "data/database/database_session.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "features/classes/ui/class_export_dialog.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <cstdio>
#include <optional>

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("class-export-dialog-parity-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

class RecordingPromptService final : public IUserPromptService
{
public:
    void showMessage(const PromptRequest& request) override
    {
        messages.append(request);
    }

    void showMessageAsync(const PromptRequest& request) override
    {
        asynchronousMessages.append(request);
    }

    PromptChoice confirm(const PromptRequest&) override
    {
        return PromptChoice::Rejected;
    }

    UnsavedChangesChoice confirmUnsavedChanges(
        const UnsavedChangesRequest&
        ) override
    {
        return UnsavedChangesChoice::Cancel;
    }

    QString chooseAction(const ActionPromptRequest&) override
    {
        return {};
    }

    QVector<PromptRequest> messages;
    QVector<PromptRequest> asynchronousMessages;
};

class ScopedPromptService final
{
public:
    explicit ScopedPromptService(IUserPromptService& service)
    {
        DialogServices::setUserPromptServiceForTesting(&service);
    }

    ~ScopedPromptService()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }

    ScopedPromptService(const ScopedPromptService&) = delete;
    ScopedPromptService& operator=(const ScopedPromptService&) = delete;
};

struct SeededClass final
{
    int classId = -1;
    ClassInfo info;
};

int createTeacher(ApplicationServices& services, QString* error)
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD");
    teacher.teacherEn = QStringLiteral("Alex Kim");
    teacher.preferredRomanization = QStringLiteral("Alex Kim");
    teacher.preferredName = QStringLiteral("Alex Kim");
    teacher.roomNumber = QStringLiteral("Room 504");
    teacher.wifiName = QStringLiteral("Parity WiFi");
    teacher.wifiPassword = QStringLiteral("class-export-parity-password");
    teacher.internetType = QStringLiteral("Both");
    teacher.zoomId = QStringLiteral("123 456 7890");
    teacher.zoomPassword = QStringLiteral("class-export-parity-zoom");
    teacher.projectionType = QStringLiteral("Any");

    const auto created = services.teacherService()->create(teacher);
    if (!created)
    {
        if (error)
        {
            *error = created.error();
        }
        return -1;
    }
    return *created;
}

std::optional<SeededClass> createClassWithDetails(
    ApplicationServices& services,
    const QString& className,
    const QString& grade,
    const QString& level,
    const int teacherId,
    const QList<ClassTime>& regularTimes,
    QString* error
    )
{
    const auto created = services.classService()->create(className);
    if (!created)
    {
        if (error)
        {
            *error = created.error();
        }
        return std::nullopt;
    }

    auto loaded = services.classService()->classInfo(*created);
    if (!loaded)
    {
        if (error)
        {
            *error = loaded.error();
        }
        return std::nullopt;
    }

    ClassInfo info = *loaded;
    info.teacherId = teacherId;
    info.classGrade = grade;
    info.classLevel = level;
    info.classTimes = regularTimes;
    const auto saved = services.classService()->saveClassInfo(info);
    if (!saved)
    {
        if (error)
        {
            *error = saved.error();
        }
        return std::nullopt;
    }

    loaded = services.classService()->classInfo(*created);
    if (!loaded)
    {
        if (error)
        {
            *error = loaded.error();
        }
        return std::nullopt;
    }

    return SeededClass{
        .classId = *created,
        .info = *loaded
    };
}

QListWidget* classList(ClassExportDialog& dialog)
{
    return dialog.findChild<QListWidget*>(QStringLiteral("classExportList"));
}

QPushButton* exportButton(ClassExportDialog& dialog)
{
    return dialog.findChild<QPushButton*>(
        QStringLiteral("exportClassesButton")
        );
}

bool clickNamedButton(QWidget& widget, const QString& text)
{
    for (QPushButton* button : widget.findChildren<QPushButton*>())
    {
        if (button->text() == text)
        {
            button->click();
            return true;
        }
    }
    return false;
}

QJsonArray integerArray(const QList<int>& values)
{
    QJsonArray result;
    for (const int value : values)
    {
        result.append(value);
    }
    return result;
}

QJsonObject listObservation(
    const QString& scenario,
    ClassExportDialog& dialog
    )
{
    QJsonObject result;
    result.insert(QStringLiteral("case"), scenario);
    QListWidget* const list = classList(dialog);
    QPushButton* const button = exportButton(dialog);
    Q_ASSERT(list && button);

    QJsonArray labels;
    QJsonArray ids;
    QJsonArray checked;
    for (int row = 0; row < list->count(); ++row)
    {
        const QListWidgetItem* const item = list->item(row);
        labels.append(item->text());
        ids.append(item->data(Qt::UserRole).toInt());
        checked.append(item->checkState() == Qt::Checked);
    }

    result.insert(QStringLiteral("count"), list->count());
    result.insert(QStringLiteral("labels"), labels);
    result.insert(QStringLiteral("ids"), ids);
    result.insert(QStringLiteral("checked"), checked);
    result.insert(
        QStringLiteral("selectedIds"),
        integerArray(dialog.selectedClassIds())
        );
    result.insert(QStringLiteral("currentRow"), list->currentRow());
    result.insert(
        QStringLiteral("selectedRows"),
        list->selectedItems().size()
        );
    result.insert(QStringLiteral("exportEnabled"), button->isEnabled());
    return result;
}

void emitTranscript(const QJsonObject& row)
{
    QByteArray bytes = QJsonDocument(row).toJson(QJsonDocument::Compact);
    bytes.append('\n');
    std::fwrite(
        bytes.constData(),
        1,
        static_cast<std::size_t>(bytes.size()),
        stdout
        );
    std::fflush(stdout);
}

bool allUnchecked(const QListWidget& list)
{
    for (int row = 0; row < list.count(); ++row)
    {
        if (list.item(row)->checkState() != Qt::Unchecked)
        {
            return false;
        }
    }
    return true;
}

QString displayName(const ClassInfo& info, const Teacher& teacher)
{
    return SidebarNodeNaming::formatClassDisplayName(info, teacher).trimmed();
}

}

class ClassExportDialogParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void populatedListAndSelectionControls();
    void emptyClassList();
    void classListFailureWarnsAndLeavesExportDisabled();
    void classInfoReadFailureUsesDisplayFallback();
    void missingTeacherFallbackRetainsClassFields();
};

void ClassExportDialogParityTests::populatedListAndSelectionControls()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QString error;
    const int teacherId = createTeacher(services, &error);
    QVERIFY2(teacherId > 0, qPrintable(error));
    const QList<ClassTime> noTimes;

    const auto lowerDuplicateId = createClassWithDetails(
        services,
        QStringLiteral("Zulu duplicate"),
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        teacherId,
        noTimes,
        &error
        );
    QVERIFY2(lowerDuplicateId.has_value(), qPrintable(error));
    const auto distinctLabel = createClassWithDetails(
        services,
        QStringLiteral("Middle label"),
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        teacherId,
        noTimes,
        &error
        );
    QVERIFY2(distinctLabel.has_value(), qPrintable(error));
    const auto higherDuplicateId = createClassWithDetails(
        services,
        QStringLiteral("Alpha duplicate"),
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        teacherId,
        noTimes,
        &error
        );
    QVERIFY2(higherDuplicateId.has_value(), qPrintable(error));
    QCOMPARE(lowerDuplicateId->classId, 1);
    QCOMPARE(distinctLabel->classId, 2);
    QCOMPARE(higherDuplicateId->classId, 3);

    auto loadedTeacher = services.teacherService()->teacher(teacherId);
    QVERIFY(loadedTeacher.has_value());

    ClassExportDialog dialog(&services);
    QListWidget* const list = classList(dialog);
    QPushButton* const button = exportButton(dialog);
    QVERIFY(list && button);
    QCOMPARE(list->count(), 3);

    const QList<int> expectedIds{2, 1, 3};
    const QString perseusName = displayName(
        distinctLabel->info,
        *loadedTeacher
        );
    const QString theseusName = displayName(
        lowerDuplicateId->info,
        *loadedTeacher
        );
    QCOMPARE(list->item(0)->text(), perseusName);
    QCOMPARE(list->item(1)->text(), theseusName);
    QCOMPARE(list->item(2)->text(), theseusName);
    QCOMPARE(list->item(0)->data(Qt::UserRole).toInt(), expectedIds.at(0));
    QCOMPARE(list->item(1)->data(Qt::UserRole).toInt(), expectedIds.at(1));
    QCOMPARE(list->item(2)->data(Qt::UserRole).toInt(), expectedIds.at(2));
    QVERIFY(list->item(0)->text().startsWith(QStringLiteral("E4 Perseus")));
    QVERIFY(list->item(1)->text().startsWith(QStringLiteral("E4 Theseus")));
    QVERIFY(allUnchecked(*list));
    QVERIFY(list->selectedItems().isEmpty());
    QCOMPARE(list->currentRow(), -1);
    QVERIFY(dialog.selectedClassIds().isEmpty());
    QVERIFY(!button->isEnabled());
    emitTranscript(listObservation(QStringLiteral("populated-initial"), dialog));

    list->item(1)->setCheckState(Qt::Checked);
    const QList<int> oneExpected{1};
    QCOMPARE(dialog.selectedClassIds(), oneExpected);
    QVERIFY(button->isEnabled());
    emitTranscript(listObservation(QStringLiteral("one-checked"), dialog));

    QVERIFY(clickNamedButton(dialog, QStringLiteral("Select All")));
    QCOMPARE(dialog.selectedClassIds(), expectedIds);
    QVERIFY(button->isEnabled());
    for (int row = 0; row < list->count(); ++row)
    {
        QCOMPARE(list->item(row)->checkState(), Qt::Checked);
    }
    emitTranscript(listObservation(QStringLiteral("select-all"), dialog));

    QVERIFY(clickNamedButton(dialog, QStringLiteral("Clear")));
    QVERIFY(dialog.selectedClassIds().isEmpty());
    QVERIFY(allUnchecked(*list));
    QVERIFY(!button->isEnabled());
    emitTranscript(listObservation(QStringLiteral("clear"), dialog));
}

void ClassExportDialogParityTests::emptyClassList()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    ClassExportDialog dialog(&services);
    QListWidget* const list = classList(dialog);
    QPushButton* const button = exportButton(dialog);
    QVERIFY(list && button);
    QCOMPARE(list->count(), 0);
    QVERIFY(dialog.selectedClassIds().isEmpty());
    QCOMPARE(list->currentRow(), -1);
    QVERIFY(!button->isEnabled());
    emitTranscript(listObservation(QStringLiteral("empty"), dialog));
}

void ClassExportDialogParityTests::
classListFailureWarnsAndLeavesExportDisabled()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QSqlQuery drop(services.databaseSession()->database());
    QVERIFY2(
        drop.exec(QStringLiteral("DROP TABLE testing_classes")),
        qPrintable(drop.lastError().text())
        );

    RecordingPromptService promptService;
    ScopedPromptService promptOverride(promptService);
    ClassExportDialog dialog(&services);
    QListWidget* const list = classList(dialog);
    QPushButton* const button = exportButton(dialog);
    QVERIFY(list && button);
    QCOMPARE(list->count(), 0);
    QVERIFY(dialog.selectedClassIds().isEmpty());
    QCOMPARE(list->currentRow(), -1);
    QVERIFY(!button->isEnabled());
    QCOMPARE(promptService.messages.size(), 1);
    const PromptRequest& warning = promptService.messages.first();
    QCOMPARE(warning.severity, PromptSeverity::Warning);
    QCOMPARE(warning.title, QStringLiteral("Export Classes"));
    QCOMPARE(warning.message, QStringLiteral("Classes could not be loaded."));
    QVERIFY(!warning.details.isEmpty());

    QJsonObject row = listObservation(QStringLiteral("class-list-failure"), dialog);
    row.insert(QStringLiteral("warningTitle"), warning.title);
    row.insert(QStringLiteral("warningText"), warning.message);
    row.insert(QStringLiteral("detailsPresent"), !warning.details.isEmpty());
    emitTranscript(row);
}

void ClassExportDialogParityTests::classInfoReadFailureUsesDisplayFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QString error;
    const int teacherId = createTeacher(services, &error);
    QVERIFY2(teacherId > 0, qPrintable(error));
    const auto seeded = createClassWithDetails(
        services,
        QStringLiteral("Fallback class name"),
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        teacherId,
        {},
        &error
        );
    QVERIFY2(seeded.has_value(), qPrintable(error));

    QSqlQuery drop(services.databaseSession()->database());
    QVERIFY2(
        drop.exec(QStringLiteral("DROP TABLE class_info")),
        qPrintable(drop.lastError().text())
        );

    RecordingPromptService promptService;
    ScopedPromptService promptOverride(promptService);
    ClassExportDialog dialog(&services);
    QListWidget* const list = classList(dialog);
    QVERIFY(list);
    QCOMPARE(list->count(), 1);
    QCOMPARE(list->item(0)->data(Qt::UserRole).toInt(), seeded->classId);

    const QString fallback = displayName(ClassInfo{}, Teacher{});
    QCOMPARE(list->item(0)->text(), fallback);
    QVERIFY(list->item(0)->text().contains(QStringLiteral("Unknown Class")));
    QVERIFY(list->item(0)->text().contains(QStringLiteral("No Teacher")));
    QVERIFY(promptService.messages.isEmpty());

    QJsonObject row = listObservation(QStringLiteral("subtitle-read-failure"), dialog);
    row.insert(QStringLiteral("fallbackLabel"), fallback);
    emitTranscript(row);
}

void ClassExportDialogParityTests::
missingTeacherFallbackRetainsClassFields()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QString error;
    const auto seeded = createClassWithDetails(
        services,
        QStringLiteral("Dangling teacher class"),
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        -1,
        {
            {
                QStringLiteral("Monday"),
                QStringLiteral("9:00 AM"),
                QStringLiteral("9:55 AM")
            }
        },
        &error
        );
    QVERIFY2(seeded.has_value(), qPrintable(error));

    QSqlDatabase database = services.databaseSession()->database();
    QSqlQuery disableForeignKeys(database);
    QVERIFY2(
        disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys = OFF")),
        qPrintable(disableForeignKeys.lastError().text())
        );

    QSqlQuery assignMissingTeacher(database);
    assignMissingTeacher.prepare(
        QStringLiteral(
            "UPDATE class_info SET teacher_id=? WHERE class_id=?"
            )
        );
    assignMissingTeacher.addBindValue(999999);
    assignMissingTeacher.addBindValue(seeded->classId);
    QVERIFY2(
        assignMissingTeacher.exec(),
        qPrintable(assignMissingTeacher.lastError().text())
        );

    QSqlQuery enableForeignKeys(database);
    QVERIFY2(
        enableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys = ON")),
        qPrintable(enableForeignKeys.lastError().text())
        );

    ClassExportDialog dialog(&services);
    QListWidget* const list = classList(dialog);
    QVERIFY(list);
    QCOMPARE(list->count(), 1);
    QCOMPARE(list->item(0)->data(Qt::UserRole).toInt(), seeded->classId);
    const QString expected = displayName(seeded->info, Teacher{});
    QCOMPARE(list->item(0)->text(), expected);
    QVERIFY(list->item(0)->text().contains(QStringLiteral("E4 Theseus")));
    QVERIFY(list->item(0)->text().contains(QStringLiteral("No Teacher")));
    QVERIFY(list->item(0)->text().contains(QStringLiteral("Mon")));

    QJsonObject row = listObservation(QStringLiteral("missing-teacher"), dialog);
    row.insert(QStringLiteral("expectedLabel"), expected);
    emitTranscript(row);
}

QTEST_MAIN(ClassExportDialogParityTests)

#include "class_export_dialog_parity_tests.moc"
