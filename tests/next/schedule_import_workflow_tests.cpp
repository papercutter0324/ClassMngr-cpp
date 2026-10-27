#include "features/schedule/import/schedule_workbook_parser.h"
#include "next/ui/initial_setup_schedule_import_dialog.h"
#include "next/ui/schedule_import_request_mapper.h"

#include <QApplication>
#include <QColorDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QTranslator>
#include <QtTest/QtTest>

#include <expected>
#include <atomic>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QString scheduleFixturePath()
{
    return QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
        QStringLiteral("tests/fixtures/imports/schedule_review.xlsx")
        );
}

QString sourceStatus(const Ui::InitialSetupScheduleImportDialog& dialog)
{
    const auto* status = dialog.findChild<QLabel*>(
        QStringLiteral("classMngrNextScheduleImportSourceStatus")
        );
    return status ? status->text() : QString{};
}

Application::ScheduleImportStateSnapshot emptySnapshot()
{
    return {};
}

Application::ScheduleImportApplySummary successSummary()
{
    return {
        Application::ScheduleImportAppliedCount{3},
        Application::ScheduleImportAppliedCount{0},
        Application::ScheduleImportAppliedCount{3},
        Application::ScheduleImportAppliedCount{0},
        Application::ScheduleImportAppliedCount{0},
        Application::ScheduleImportAppliedCount{0},
        Application::ScheduleImportAppliedCount{0},
        false
    };
}

} // namespace

class ScheduleImportWorkflowTests final : public QObject
{
    Q_OBJECT

private slots:
    void fixtureMapsIntoTypedRequest();
    void reviewControlsMapSelectedDecisionsToExactApplyRequest();
    void dialogAppliesAndKeepsSetupPending();
    void applyFailureKeepsReviewAndReviewCancelReturnsToSource();
    void timeoutRetrySuppressesLateParseResult();
    void sourceCancelInvokesSetupRecovery();
    void profileNameMismatchRequiresExplicitConfirmation();
    void workbookDiagnosticsRequireAcknowledgement();
    void unsupportedExtensionIsRejectedBeforeParsing();
    void changedWorkbookAndModeRequireReload();
    void englishAndKoreanLabelsLoadAcrossLightAndDarkThemes_data();
    void englishAndKoreanLabelsLoadAcrossLightAndDarkThemes();
};

void ScheduleImportWorkflowTests::fixtureMapsIntoTypedRequest()
{
    QFile file(scheduleFixturePath());
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));
    const auto workbook = parseScheduleImportWorkbook(
        file,
        ScheduleImportKind::Normal
        );
    QVERIFY2(workbook.has_value(),
             workbook ? "" : qPrintable(workbook.error()));
    QCOMPARE(workbook->sheets.size(), 2);
    const auto& sheet = workbook->sheets.front();
    QVERIFY(sheet.visible);
    QCOMPARE(sheet.users.size(), 1);
    QCOMPARE(sheet.users.front().name, QStringLiteral("Alice"));

    auto mapping = Ui::mapScheduleImportReview(
        sheet.users.front(),
        ScheduleImportKind::Normal,
        emptySnapshot()
        );
    QVERIFY2(mapping.has_value(),
             mapping ? "" : qPrintable(mapping.error()));
    QCOMPARE(mapping->matching.classes.size(), std::size_t(3));
    QCOMPARE(mapping->decisions.teachers.size(), std::size_t(3));
    QCOMPARE(mapping->decisions.classes.size(), std::size_t(3));
    QCOMPARE(mapping->decisions.classes.front().action,
             Application::ScheduleImportReviewClassAction::CreateNew);

    const auto request = Ui::makeScheduleImportApplyRequest(
        *mapping,
        mapping->decisions,
        mapping->selectedTeacherIds,
        mapping->classColors,
        Application::ScheduleImportPlanIntensiveMode::UpdateExisting,
        true,
        false,
        false
        );
    QCOMPARE(request.selectedUserName, std::u16string(u"Alice"));
    QCOMPARE(request.candidates.size(), std::size_t(3));
    QCOMPARE(request.teachers.size(), std::size_t(3));
    QCOMPARE(request.classes.size(), std::size_t(3));
    QVERIFY(!request.saveProfileNameIfBlank);
    QVERIFY(!request.updateProfileName);
    QVERIFY(request.diagnosticsAcknowledged);

    const auto profileUpdate = Ui::makeScheduleImportApplyRequest(
        *mapping,
        mapping->decisions,
        mapping->selectedTeacherIds,
        mapping->classColors,
        Application::ScheduleImportPlanIntensiveMode::UpdateExisting,
        true,
        true,
        false
        );
    QVERIFY(profileUpdate.saveProfileNameIfBlank);
}

void ScheduleImportWorkflowTests::reviewControlsMapSelectedDecisionsToExactApplyRequest()
{
    QFile file(scheduleFixturePath());
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));
    const auto workbook = parseScheduleImportWorkbook(
        file,
        ScheduleImportKind::Normal
        );
    QVERIFY2(workbook.has_value(),
             workbook ? "" : qPrintable(workbook.error()));
    const ScheduleImportUserBlock& user = workbook->sheets.front().users.front();
    QVERIFY(!user.classes.isEmpty());
    const ScheduleImportClassCandidate& firstCandidate = user.classes.front();
    QVERIFY(!firstCandidate.rooms.isEmpty());

    const auto teacherId = Domain::TeacherId::fromString("51");
    const auto classId = Domain::ClassId::fromString("71");
    QVERIFY(teacherId.has_value());
    QVERIFY(classId.has_value());
    Application::ScheduleImportStateSnapshot snapshot;
    snapshot.teachers.push_back({
        *teacherId,
        firstCandidate.teacherKr.toStdU16String(),
        firstCandidate.rooms.front().toStdU16String()
    });
    Application::ScheduleImportStateReadClassSnapshot existingClass{
        *classId,
        *teacherId,
        firstCandidate.classLevel.toStdU16String(),
        firstCandidate.classGrade.toStdU16String(),
        firstCandidate.classLevel.toStdU16String(),
        u"#FFFFFF",
        {},
        {},
        firstCandidate.rooms.front().toStdU16String()
    };
    for (const ClassTime& time : firstCandidate.times)
    {
        existingClass.normalTimes.push_back({
            time.day.toStdU16String(),
            time.startTime.toStdU16String(),
            time.endTime.toStdU16String()
        });
    }
    snapshot.classes.push_back(std::move(existingClass));

    const QString path = scheduleFixturePath();
    std::optional<Application::ScheduleImportApplyRequest> appliedRequest;
    Ui::InitialSetupScheduleImportActions actions;
    actions.chooseWorkbook = [path] {
        return Domain::Result<std::optional<QString>>::success(path);
    };
    actions.readState = [snapshot] {
        return Application::ScheduleImportStateSnapshotOutcome{snapshot};
    };
    actions.validationTimeoutMilliseconds = [] { return 5000; };
    actions.apply = [&](const Application::ScheduleImportApplyRequest& request) {
        appliedRequest = request;
        return Application::ScheduleImportApplyResult{successSummary()};
    };
    actions.cancelSetup = [] {};

    Ui::InitialSetupScheduleImportDialog dialog(actions, QStringLiteral("Alice"));
    dialog.show();
    dialog.findChild<QPushButton*>(
        QStringLiteral("classMngrNextScheduleImportBrowse")
        )->click();
    QTRY_VERIFY_WITH_TIMEOUT(sourceStatus(dialog).contains(
        QStringLiteral("Workbook loaded")), 10000);
    auto* sheet = dialog.findChild<QComboBox*>(
        QStringLiteral("classMngrNextScheduleImportSheet")
        );
    QVERIFY(sheet);
    sheet->setCurrentIndex(1);

    QTimer::singleShot(0, &dialog, [&dialog, teacherId, classId,
                                    selectedRoom = firstCandidate.rooms.front()] {
        auto* review = dialog.findChild<QDialog*>(
            QStringLiteral("classMngrNextScheduleImportReviewDialog")
            );
        if (!review) return;
        auto* teacherAction = review->findChild<QComboBox*>(
            QStringLiteral("classMngrNextScheduleImportTeacherAction")
            );
        auto* teacherTarget = review->findChild<QComboBox*>(
            QStringLiteral("classMngrNextScheduleImportTeacherTarget")
            );
        auto* teacherRoom = review->findChild<QComboBox*>(
            QStringLiteral("classMngrNextScheduleImportTeacherRoom")
            );
        auto* classAction = review->findChild<QComboBox*>(
            QStringLiteral("classMngrNextScheduleImportClassAction")
            );
        auto* classTarget = review->findChild<QComboBox*>(
            QStringLiteral("classMngrNextScheduleImportClassTarget")
            );
        auto* classColor = review->findChild<QPushButton*>(
            QStringLiteral("classMngrNextScheduleImportClassColor")
            );
        auto* apply = review->findChild<QPushButton*>(
            QStringLiteral("classMngrNextScheduleImportApply")
            );
        if (!teacherAction || !teacherTarget || !teacherRoom || !classAction
            || !classTarget || !classColor || !apply)
        {
            return;
        }

        teacherAction->setCurrentIndex(teacherAction->findData(static_cast<int>(
            Application::ScheduleImportReviewTeacherAction::UpdateRoom
            )));
        teacherTarget->setCurrentIndex(teacherTarget->findData(
            QString::fromStdString(teacherId->value())
            ));
        teacherRoom->setCurrentIndex(teacherRoom->findData(selectedRoom));
        classAction->setCurrentIndex(classAction->findData(static_cast<int>(
            Application::ScheduleImportReviewClassAction::UpdateExisting
            )));
        classTarget->setCurrentIndex(classTarget->findData(
            QString::fromStdString(classId->value())
            ));

        QTimer::singleShot(0, qApp, [] {
            auto* colorDialog = qobject_cast<QColorDialog*>(
                QApplication::activeModalWidget()
                );
            if (!colorDialog) return;
            colorDialog->setCurrentColor(QColor(QStringLiteral("#123456")));
            colorDialog->accept();
        });
        classColor->click();
        apply->click();
    });
    dialog.findChild<QPushButton*>(
        QStringLiteral("classMngrNextScheduleImportReview")
        )->click();

    QVERIFY(appliedRequest.has_value());
    QCOMPARE(appliedRequest->teachers.size(), std::size_t(3));
    QCOMPARE(appliedRequest->teachers.front().action,
             Application::ScheduleImportReviewTeacherAction::UpdateRoom);
    QCOMPARE(appliedRequest->teachers.front().targetTeacherId,
             std::optional<Domain::TeacherId>{*teacherId});
    QCOMPARE(appliedRequest->teachers.front().selectedRoom,
             firstCandidate.rooms.front().toStdU16String());
    QCOMPARE(appliedRequest->classes.size(), std::size_t(3));
    QCOMPARE(appliedRequest->classes.front().action,
             Application::ScheduleImportReviewClassAction::UpdateExisting);
    QCOMPARE(appliedRequest->classes.front().targetClassId,
             std::optional<Domain::ClassId>{*classId});
    QCOMPARE(appliedRequest->classes.front().classColor, std::u16string(u"#123456"));
    QCOMPARE(appliedRequest->classes.front().fontColor, std::u16string(u"#FFFFFF"));
}

void ScheduleImportWorkflowTests::dialogAppliesAndKeepsSetupPending()
{
    const QString path = scheduleFixturePath();
    int applyCalls = 0;
    int cancelCalls = 0;
    std::optional<Application::ScheduleImportApplyRequest> appliedRequest;

    Ui::InitialSetupScheduleImportActions actions;
    actions.chooseWorkbook = [path] {
        return Domain::Result<std::optional<QString>>::success(path);
    };
    actions.readState = [] {
        return Application::ScheduleImportStateSnapshotOutcome{emptySnapshot()};
    };
    actions.validationTimeoutMilliseconds = [] { return 5000; };
    actions.apply = [&](const Application::ScheduleImportApplyRequest& request) {
        ++applyCalls;
        appliedRequest = request;
        return Application::ScheduleImportApplyResult{successSummary()};
    };
    actions.cancelSetup = [&] { ++cancelCalls; };

    Ui::InitialSetupScheduleImportDialog dialog(actions, QStringLiteral("Alice"));
    dialog.show();
    auto* browse = dialog.findChild<QPushButton*>(
        QStringLiteral("classMngrNextScheduleImportBrowse")
        );
    auto* load = dialog.findChild<QPushButton*>(
        QStringLiteral("classMngrNextScheduleImportLoad")
        );
    auto* sheet = dialog.findChild<QComboBox*>(
        QStringLiteral("classMngrNextScheduleImportSheet")
        );
    auto* user = dialog.findChild<QComboBox*>(
        QStringLiteral("classMngrNextScheduleImportUser")
        );
    auto* reviewButton = dialog.findChild<QPushButton*>(
        QStringLiteral("classMngrNextScheduleImportReview")
        );
    QVERIFY(browse && load && sheet && user && reviewButton);

    browse->click();
    QTRY_VERIFY_WITH_TIMEOUT(sourceStatus(dialog).contains(
        QStringLiteral("Workbook loaded")), 10000);
    QCOMPARE(sheet->count(), 3); // prompt plus both visible fixture sheets
    QCOMPARE(sheet->currentIndex(), 0); // ambiguous sheet choice stays explicit
    sheet->setCurrentIndex(1);
    QCOMPARE(user->count(), 2); // prompt plus its detected profile block
    QCOMPARE(user->currentText(), QStringLiteral("Alice"));

    QTimer::singleShot(0, &dialog, [&dialog] {
        QDialog* review = nullptr;
        for (QDialog* candidate : dialog.findChildren<QDialog*>())
        {
            if (candidate->objectName()
                == QStringLiteral("classMngrNextScheduleImportReviewDialog"))
            {
                review = candidate;
                break;
            }
        }
        if (!review)
        {
            QTimer::singleShot(10, &dialog, [&dialog] {
                for (QDialog* candidate : dialog.findChildren<QDialog*>())
                {
                    if (candidate->objectName()
                        == QStringLiteral("classMngrNextScheduleImportReviewDialog"))
                    {
                        if (auto* apply = candidate->findChild<QPushButton*>(
                                QStringLiteral("classMngrNextScheduleImportApply")))
                        {
                            apply->click();
                        }
                        return;
                    }
                }
            });
            return;
        }
        if (auto* apply = review->findChild<QPushButton*>(
                QStringLiteral("classMngrNextScheduleImportApply")))
        {
            apply->click();
        }
    });
    reviewButton->click();

    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
    QCOMPARE(applyCalls, 1);
    QCOMPARE(cancelCalls, 0);
    QVERIFY(appliedRequest.has_value());
    QCOMPARE(appliedRequest->selectedUserName, std::u16string(u"Alice"));
    QCOMPARE(appliedRequest->candidates.size(), std::size_t(3));
    QVERIFY(dialog.successSummary().contains(QStringLiteral("Teachers created: 3")));
}

void ScheduleImportWorkflowTests::applyFailureKeepsReviewAndReviewCancelReturnsToSource()
{
    const QString path = scheduleFixturePath();
    int applyCalls = 0;
    int cancelCalls = 0;
    std::optional<Application::ScheduleImportApplyRequest> successfulRequest;

    Ui::InitialSetupScheduleImportActions actions;
    actions.chooseWorkbook = [path] {
        return Domain::Result<std::optional<QString>>::success(path);
    };
    actions.readState = [] {
        return Application::ScheduleImportStateSnapshotOutcome{emptySnapshot()};
    };
    actions.validationTimeoutMilliseconds = [] { return 5000; };
    actions.apply = [&](const Application::ScheduleImportApplyRequest& request) {
        ++applyCalls;
        if (applyCalls == 1)
        {
            return Application::ScheduleImportApplyResult{
                std::unexpected(Application::ScheduleImportApplyFailure{
                    u"temporary apply failure",
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    Domain::ErrorCode::Technical
                })
            };
        }
        successfulRequest = request;
        return Application::ScheduleImportApplyResult{successSummary()};
    };
    actions.cancelSetup = [&] { ++cancelCalls; };

    Ui::InitialSetupScheduleImportDialog dialog(actions, QStringLiteral("Alice"));
    dialog.show();
    dialog.findChild<QPushButton*>(
        QStringLiteral("classMngrNextScheduleImportBrowse")
        )->click();
    QTRY_VERIFY_WITH_TIMEOUT(sourceStatus(dialog).contains(
        QStringLiteral("Workbook loaded")), 10000);
    dialog.findChild<QComboBox*>(
        QStringLiteral("classMngrNextScheduleImportSheet")
        )->setCurrentIndex(1);
    auto* reviewButton = dialog.findChild<QPushButton*>(
        QStringLiteral("classMngrNextScheduleImportReview")
        );
    QVERIFY(reviewButton);

    QTimer::singleShot(0, &dialog, [&dialog] {
        QTimer::singleShot(0, &dialog, [&dialog] {
            QDialog* review = nullptr;
            for (QDialog* candidate : dialog.findChildren<QDialog*>())
            {
                if (candidate->objectName()
                    == QStringLiteral("classMngrNextScheduleImportReviewDialog"))
                {
                    review = candidate;
                    break;
                }
            }
            if (!review) return;
            auto* back = review->findChild<QPushButton*>(
                QStringLiteral("classMngrNextScheduleImportBackToSource")
                );
            if (back) back->click();
        });
    });
    reviewButton->click();
    QCOMPARE(cancelCalls, 0);
    QCOMPARE(dialog.result(), 0); // review rejection leaves the source dialog open

    auto* load = dialog.findChild<QPushButton*>(
        QStringLiteral("classMngrNextScheduleImportLoad")
        );
    QVERIFY(load);
    // Reopening review and failing once must leave the typed form available for
    // a successful retry with its current action selections intact.
    QTimer::singleShot(0, &dialog, [&dialog] {
        QDialog* review = nullptr;
        for (QDialog* candidate : dialog.findChildren<QDialog*>())
        {
            if (candidate->objectName()
                == QStringLiteral("classMngrNextScheduleImportReviewDialog"))
            {
                review = candidate;
                break;
            }
        }
        if (!review) return;
        auto* apply = review->findChild<QPushButton*>(
            QStringLiteral("classMngrNextScheduleImportApply")
            );
        if (!apply) return;
        apply->click();
        auto* status = review->findChild<QLabel*>(
            QStringLiteral("classMngrNextScheduleImportReviewStatus")
            );
        if (!status || !status->text().contains(QStringLiteral("failed"))) return;
        apply->click();
    });
    reviewButton->click();

    QCOMPARE(applyCalls, 2);
    QVERIFY(successfulRequest.has_value());
    QCOMPARE(successfulRequest->classes.size(), std::size_t(3));
    QCOMPARE(cancelCalls, 0);
    QVERIFY(dialog.result() == static_cast<int>(QDialog::Accepted));
}

void ScheduleImportWorkflowTests::timeoutRetrySuppressesLateParseResult()
{
    const QString path = scheduleFixturePath();
    int timeoutMilliseconds = 15;
    std::atomic<int> parserCalls = 0;
    int cancelCalls = 0;
    Ui::InitialSetupScheduleImportActions actions;
    actions.chooseWorkbook = [path] {
        return Domain::Result<std::optional<QString>>::success(path);
    };
    actions.readState = [] {
        return Application::ScheduleImportStateSnapshotOutcome{emptySnapshot()};
    };
    actions.validationTimeoutMilliseconds = [&] { return timeoutMilliseconds; };
    actions.parseWorkbook = [&parserCalls](
        const QByteArray& bytes,
        const ScheduleImportKind kind
        ) -> std::expected<ScheduleImportWorkbook, QString> {
        if (++parserCalls == 1)
        {
            QThread::msleep(300);
        }
        return parseScheduleImportWorkbook(bytes, kind);
    };
    actions.apply = [](const Application::ScheduleImportApplyRequest&) {
        return Application::ScheduleImportApplyResult{successSummary()};
    };
    actions.cancelSetup = [&] { ++cancelCalls; };

    Ui::InitialSetupScheduleImportDialog dialog(actions, QStringLiteral("Alice"));
    dialog.show();
    dialog.findChild<QPushButton*>(
        QStringLiteral("classMngrNextScheduleImportBrowse")
        )->click();
    QTRY_VERIFY_WITH_TIMEOUT(sourceStatus(dialog).contains(
        QStringLiteral("timed out")), 5000);
    timeoutMilliseconds = 10000;
    dialog.findChild<QPushButton*>(
        QStringLiteral("classMngrNextScheduleImportLoad")
        )->click();
    QTRY_VERIFY_WITH_TIMEOUT(sourceStatus(dialog).contains(
        QStringLiteral("Workbook loaded")), 10000);
    QTest::qWait(350);
    QVERIFY(sourceStatus(dialog).contains(QStringLiteral("Workbook loaded")));
    QCOMPARE(parserCalls.load(), 2);
    QCOMPARE(cancelCalls, 0);
    dialog.reject();
    QCOMPARE(cancelCalls, 1);
}

void ScheduleImportWorkflowTests::sourceCancelInvokesSetupRecovery()
{
    int cancelCalls = 0;
    Ui::InitialSetupScheduleImportActions actions;
    actions.cancelSetup = [&] { ++cancelCalls; };
    Ui::InitialSetupScheduleImportDialog dialog(actions, QString{});
    dialog.reject();
    QCOMPARE(cancelCalls, 1);
    dialog.reject();
    QCOMPARE(cancelCalls, 1);
}

void ScheduleImportWorkflowTests::profileNameMismatchRequiresExplicitConfirmation()
{
    const QString path = scheduleFixturePath();
    const auto applyWithProfileChoice = [&path](const bool confirmUpdate) {
        std::optional<Application::ScheduleImportApplyRequest> appliedRequest;
        int confirmationDialogs = 0;
        Ui::InitialSetupScheduleImportActions actions;
        actions.chooseWorkbook = [path] {
            return Domain::Result<std::optional<QString>>::success(path);
        };
        actions.readState = [] {
            return Application::ScheduleImportStateSnapshotOutcome{emptySnapshot()};
        };
        actions.validationTimeoutMilliseconds = [] { return 5000; };
        actions.apply = [&](const Application::ScheduleImportApplyRequest& request) {
            appliedRequest = request;
            return Application::ScheduleImportApplyResult{successSummary()};
        };

        Ui::InitialSetupScheduleImportDialog dialog(actions, QStringLiteral("Bob"));
        dialog.show();
        bool browseStarted = false;
        bool reviewStarted = false;
        bool applyStarted = false;
        auto driver = std::make_shared<std::function<void()>>();
        *driver = [
            &dialog,
            confirmUpdate,
            &confirmationDialogs,
            &browseStarted,
            &reviewStarted,
            &applyStarted,
            &appliedRequest,
            driver
            ] {
            if (applyStarted)
            {
                return;
            }
            QWidget* const modal = QApplication::activeModalWidget();
            if (!modal)
            {
                QTimer::singleShot(10, &dialog, *driver);
                return;
            }
            if (auto* const confirmation = qobject_cast<QMessageBox*>(modal))
            {
                ++confirmationDialogs;
                const auto button = confirmUpdate
                    ? QMessageBox::Yes
                    : QMessageBox::Cancel;
                QTimer::singleShot(0, &dialog, *driver);
                confirmation->button(button)->click();
                return;
            }
            if (modal->objectName()
                == QStringLiteral("classMngrNextScheduleImportSourceDialog"))
            {
                auto* const status = modal->findChild<QLabel*>(
                    QStringLiteral("classMngrNextScheduleImportSourceStatus")
                    );
                auto* const browse = modal->findChild<QPushButton*>(
                    QStringLiteral("classMngrNextScheduleImportBrowse")
                    );
                auto* const sheet = modal->findChild<QComboBox*>(
                    QStringLiteral("classMngrNextScheduleImportSheet")
                    );
                auto* const user = modal->findChild<QComboBox*>(
                    QStringLiteral("classMngrNextScheduleImportUser")
                    );
                auto* const confirmation = modal->findChild<QCheckBox*>(
                    QStringLiteral("classMngrNextScheduleImportProfileNameConfirmation")
                    );
                auto* const review = modal->findChild<QPushButton*>(
                    QStringLiteral("classMngrNextScheduleImportReview")
                    );
                if (!status || !browse || !sheet || !user || !confirmation || !review)
                {
                    return;
                }
                if (!browseStarted)
                {
                    browseStarted = true;
                    browse->click();
                }
                if (!status->text().contains(QStringLiteral("Workbook loaded")))
                {
                    QTimer::singleShot(10, &dialog, *driver);
                    return;
                }
                sheet->setCurrentIndex(1);
                QCOMPARE(user->currentText(), QStringLiteral("Alice"));
                confirmation->setChecked(confirmUpdate);
                if (!reviewStarted)
                {
                    reviewStarted = true;
                    QTimer::singleShot(0, &dialog, *driver);
                    review->click();
                }
                return;
            }
            if (modal->objectName()
                != QStringLiteral("classMngrNextScheduleImportReviewDialog"))
            {
                return;
            }
            auto* const apply = modal->findChild<QPushButton*>(
                QStringLiteral("classMngrNextScheduleImportApply")
                );
            if (!apply)
            {
                return;
            }
            apply->click();
            applyStarted = true;
        };

        dialog.findChild<QPushButton*>(
            QStringLiteral("classMngrNextScheduleImportBrowse")
            )->click();
        QTRY_VERIFY_WITH_TIMEOUT(
            sourceStatus(dialog).contains(QStringLiteral("Workbook loaded")),
            10000
            );
        dialog.findChild<QComboBox*>(
            QStringLiteral("classMngrNextScheduleImportSheet")
            )->setCurrentIndex(1);
        auto* const user = dialog.findChild<QComboBox*>(
            QStringLiteral("classMngrNextScheduleImportUser")
            );
        QVERIFY(user && user->count() == 2);
        user->setCurrentIndex(1);
        auto* const checkbox = dialog.findChild<QCheckBox*>(
            QStringLiteral("classMngrNextScheduleImportProfileNameConfirmation")
            );
        QVERIFY(checkbox && checkbox->isVisible());
        checkbox->setChecked(confirmUpdate);
        QTimer::singleShot(0, &dialog, *driver);
        dialog.findChild<QPushButton*>(
            QStringLiteral("classMngrNextScheduleImportReview")
            )->click();

        QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
        QVERIFY(appliedRequest.has_value());
        QCOMPARE(appliedRequest->updateProfileName, confirmUpdate);
        QVERIFY(!appliedRequest->saveProfileNameIfBlank);
        QCOMPARE(confirmationDialogs, confirmUpdate ? 1 : 0);
    };

    applyWithProfileChoice(true);
    applyWithProfileChoice(false);
}

void ScheduleImportWorkflowTests::unsupportedExtensionIsRejectedBeforeParsing()
{
    int parseCalls = 0;
    int cancelCalls = 0;
    Ui::InitialSetupScheduleImportActions actions;
    actions.chooseWorkbook = [] {
        return Domain::Result<std::optional<QString>>::success(
            scheduleFixturePath().chopped(5) + QStringLiteral(".xls")
            );
    };
    actions.parseWorkbook = [&parseCalls](
        const QByteArray& bytes,
        const ScheduleImportKind kind
        ) -> std::expected<ScheduleImportWorkbook, QString> {
        ++parseCalls;
        return parseScheduleImportWorkbook(bytes, kind);
    };
    actions.cancelSetup = [&] { ++cancelCalls; };

    Ui::InitialSetupScheduleImportDialog dialog(actions, QStringLiteral("Alice"));
    dialog.show();
    dialog.findChild<QPushButton*>(
        QStringLiteral("classMngrNextScheduleImportBrowse")
        )->click();
    QCOMPARE(sourceStatus(dialog),
             QStringLiteral("Schedule import accepts .xlsx workbooks only."));
    QCOMPARE(parseCalls, 0);
    dialog.reject();
    QCOMPARE(cancelCalls, 1);
}

void ScheduleImportWorkflowTests::workbookDiagnosticsRequireAcknowledgement()
{
    const QString path = scheduleFixturePath();
    std::optional<Application::ScheduleImportApplyRequest> appliedRequest;
    Ui::InitialSetupScheduleImportActions actions;
    actions.chooseWorkbook = [path] {
        return Domain::Result<std::optional<QString>>::success(path);
    };
    actions.readState = [] {
        return Application::ScheduleImportStateSnapshotOutcome{emptySnapshot()};
    };
    actions.validationTimeoutMilliseconds = [] { return 5000; };
    actions.parseWorkbook = [](const QByteArray& bytes, const ScheduleImportKind kind)
        -> std::expected<ScheduleImportWorkbook, QString> {
        auto parsed = parseScheduleImportWorkbook(bytes, kind);
        if (parsed && !parsed->sheets.isEmpty()
            && !parsed->sheets.front().users.isEmpty())
        {
            parsed->sheets.front().users.front().diagnostics.append({
                QStringLiteral("Current"),
                QStringLiteral("Alice"),
                QStringLiteral("B2"),
                QStringLiteral("Unknown cell"),
                QStringLiteral("A workbook cell requires review.")
            });
        }
        return parsed;
    };
    actions.apply = [&](const Application::ScheduleImportApplyRequest& request) {
        appliedRequest = request;
        return Application::ScheduleImportApplyResult{successSummary()};
    };

    Ui::InitialSetupScheduleImportDialog dialog(actions, QStringLiteral("Alice"));
    dialog.show();
    dialog.findChild<QPushButton*>(
        QStringLiteral("classMngrNextScheduleImportBrowse")
        )->click();
    QTRY_VERIFY_WITH_TIMEOUT(sourceStatus(dialog).contains(
        QStringLiteral("Workbook loaded")), 10000);
    dialog.findChild<QComboBox*>(
        QStringLiteral("classMngrNextScheduleImportSheet")
        )->setCurrentIndex(1);
    auto* const reviewButton = dialog.findChild<QPushButton*>(
        QStringLiteral("classMngrNextScheduleImportReview")
        );
    QVERIFY(reviewButton);

    bool acknowledgementVerified = false;
    QTimer::singleShot(0, &dialog, [&] {
        QTimer::singleShot(0, &dialog, [&] {
            QDialog* review = nullptr;
            for (QDialog* candidate : dialog.findChildren<QDialog*>())
            {
                if (candidate->objectName()
                    == QStringLiteral("classMngrNextScheduleImportReviewDialog"))
                {
                    review = candidate;
                    break;
                }
            }
            if (!review)
            {
                return;
            }
            auto* const acknowledge = review->findChild<QCheckBox*>(
                QStringLiteral("classMngrNextScheduleImportAcknowledgeDiagnostics")
                );
            auto* const apply = review->findChild<QPushButton*>(
                QStringLiteral("classMngrNextScheduleImportApply")
                );
            if (!acknowledge || !apply)
            {
                return;
            }
            QVERIFY(acknowledge->isVisible());
            QVERIFY(!acknowledge->isChecked());
            QVERIFY(!apply->isEnabled());
            acknowledge->setChecked(true);
            QVERIFY(apply->isEnabled());
            acknowledgementVerified = true;
            apply->click();
        });
    });
    reviewButton->click();

    QVERIFY(acknowledgementVerified);
    QVERIFY(appliedRequest.has_value());
    QVERIFY(appliedRequest->diagnosticsAcknowledged);
    QCOMPARE(appliedRequest->diagnostics.size(), std::size_t(1));
}

void ScheduleImportWorkflowTests::changedWorkbookAndModeRequireReload()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QFile original(scheduleFixturePath());
    QVERIFY(original.open(QIODevice::ReadOnly));
    const QByteArray originalBytes = original.readAll();
    const QString path = temporaryDirectory.filePath(QStringLiteral("source.xlsx"));
    QVERIFY(QFile::copy(scheduleFixturePath(), path));

    std::atomic<int> parseCalls = 0;
    std::atomic<bool> mutationSucceeded = false;
    int cancelCalls = 0;
    Ui::InitialSetupScheduleImportActions actions;
    actions.chooseWorkbook = [path] {
        return Domain::Result<std::optional<QString>>::success(path);
    };
    actions.validationTimeoutMilliseconds = [] { return 5000; };
    actions.parseWorkbook = [&parseCalls, &mutationSucceeded, path](
        const QByteArray& bytes,
        const ScheduleImportKind kind
        ) -> std::expected<ScheduleImportWorkbook, QString> {
        auto parsed = parseScheduleImportWorkbook(bytes, kind);
        if (++parseCalls == 1)
        {
            QFile changed(path);
            if (changed.open(QIODevice::Append)
                && changed.write("x", 1) == 1
                && changed.flush())
            {
                mutationSucceeded = true;
            }
        }
        return parsed;
    };
    actions.cancelSetup = [&] { ++cancelCalls; };

    Ui::InitialSetupScheduleImportDialog dialog(actions, QStringLiteral("Alice"));
    dialog.show();
    dialog.findChild<QPushButton*>(
        QStringLiteral("classMngrNextScheduleImportBrowse")
        )->click();
    QTRY_VERIFY_WITH_TIMEOUT(mutationSucceeded.load(), 10000);
    QTRY_COMPARE_WITH_TIMEOUT(
        sourceStatus(dialog),
        QStringLiteral("The workbook changed while it was being read. Load it again."),
        10000
        );
    QCOMPARE(parseCalls.load(), 1);
    QCOMPARE(parseCalls.load(), 1);

    QFile restored(path);
    QVERIFY(restored.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(restored.write(originalBytes), originalBytes.size());
    restored.close();
    dialog.findChild<QPushButton*>(
        QStringLiteral("classMngrNextScheduleImportLoad")
        )->click();
    QTRY_VERIFY_WITH_TIMEOUT(sourceStatus(dialog).contains(
        QStringLiteral("Workbook loaded")), 10000);
    QCOMPARE(parseCalls.load(), 2);

    auto* const kind = dialog.findChild<QComboBox*>(
        QStringLiteral("classMngrNextScheduleImportKind")
        );
    auto* const sheet = dialog.findChild<QComboBox*>(
        QStringLiteral("classMngrNextScheduleImportSheet")
        );
    auto* const review = dialog.findChild<QPushButton*>(
        QStringLiteral("classMngrNextScheduleImportReview")
        );
    QVERIFY(kind && sheet && review);
    kind->setCurrentIndex(1);
    QCOMPARE(sourceStatus(dialog),
             QStringLiteral("The import type changed. Load the workbook again."));
    QCOMPARE(sheet->count(), 0);
    QVERIFY(!review->isEnabled());
    QCOMPARE(cancelCalls, 0);
    dialog.reject();
    QCOMPARE(cancelCalls, 1);
}

void ScheduleImportWorkflowTests::englishAndKoreanLabelsLoadAcrossLightAndDarkThemes_data()
{
    QTest::addColumn<QString>("locale");
    QTest::addColumn<QString>("theme");
    QTest::addColumn<QString>("expectedTitle");
    QTest::addColumn<QString>("expectedBrowse");
    QTest::addColumn<QString>("expectedCancel");
    QTest::addColumn<QString>("expectedStatus");
    QTest::addColumn<QString>("expectedThemeColor");
    QTest::newRow("English light")
        << QStringLiteral("en_US") << QStringLiteral("light")
        << QStringLiteral("Import Schedule")
        << QStringLiteral("Choose Workbook…") << QStringLiteral("Cancel Setup")
        << QStringLiteral("Select a schedule workbook to begin.")
        << QStringLiteral("#e9e8e3");
    QTest::newRow("English dark")
        << QStringLiteral("en_US") << QStringLiteral("dark")
        << QStringLiteral("Import Schedule")
        << QStringLiteral("Choose Workbook…") << QStringLiteral("Cancel Setup")
        << QStringLiteral("Select a schedule workbook to begin.")
        << QStringLiteral("#2b2b2b");
    QTest::newRow("Korean light")
        << QStringLiteral("ko_KR") << QStringLiteral("light")
        << QString::fromUtf8("일정 가져오기")
        << QString::fromUtf8("통합 문서 선택…")
        << QString::fromUtf8("초기 설정 취소")
        << QString::fromUtf8("가져올 일정 통합 문서를 선택하세요.")
        << QStringLiteral("#e9e8e3");
    QTest::newRow("Korean dark")
        << QStringLiteral("ko_KR") << QStringLiteral("dark")
        << QString::fromUtf8("일정 가져오기")
        << QString::fromUtf8("통합 문서 선택…")
        << QString::fromUtf8("초기 설정 취소")
        << QString::fromUtf8("가져올 일정 통합 문서를 선택하세요.")
        << QStringLiteral("#2b2b2b");
}

void ScheduleImportWorkflowTests::englishAndKoreanLabelsLoadAcrossLightAndDarkThemes()
{
    QFETCH(QString, locale);
    QFETCH(QString, theme);
    QFETCH(QString, expectedTitle);
    QFETCH(QString, expectedBrowse);
    QFETCH(QString, expectedCancel);
    QFETCH(QString, expectedStatus);
    QFETCH(QString, expectedThemeColor);

    QApplication& application = *qApp;
    const QString translationDirectory = QStringLiteral(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY
        );
    QTranslator translator;
    if (locale == QStringLiteral("ko_KR"))
    {
        const QString translationPath = QDir(translationDirectory).filePath(
            QStringLiteral("ClassMngr_ko_KR.qm")
            );
        QVERIFY2(translator.load(translationPath), qPrintable(translationPath));
        QVERIFY(application.installTranslator(&translator));
    }

    QFile stylesheet(QDir(QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT)).filePath(
        QStringLiteral("assets/styles/%1.qss").arg(theme)
        ));
    QVERIFY2(stylesheet.open(QIODevice::ReadOnly), qPrintable(stylesheet.fileName()));
    const QString previousStylesheet = application.styleSheet();
    application.setStyleSheet(QString::fromUtf8(stylesheet.readAll()));

    Ui::InitialSetupScheduleImportDialog dialog({}, QString{});
    QCOMPARE(dialog.windowTitle(), expectedTitle);
    QCOMPARE(dialog.findChild<QPushButton*>(
                 QStringLiteral("classMngrNextScheduleImportBrowse"))->text(),
             expectedBrowse);
    QCOMPARE(dialog.findChild<QPushButton*>(
                 QStringLiteral("classMngrNextScheduleImportCancel"))->text(),
             expectedCancel);
    QCOMPARE(sourceStatus(dialog), expectedStatus);
    QVERIFY(application.styleSheet().contains(expectedThemeColor));

    dialog.findChild<QLineEdit*>(
        QStringLiteral("classMngrNextScheduleImportFilePath")
        )->setText(QStringLiteral("schedule.xlsx"));
    dialog.findChild<QComboBox*>(
        QStringLiteral("classMngrNextScheduleImportKind")
        )->setCurrentIndex(1);
    const QString expectedModeChange = locale == QStringLiteral("ko_KR")
        ? QString::fromUtf8("가져오기 유형이 변경되었습니다. 통합 문서를 다시 불러오세요.")
        : QStringLiteral("The import type changed. Load the workbook again.");
    QCOMPARE(sourceStatus(dialog), expectedModeChange);

    application.setStyleSheet(previousStylesheet);
    if (locale == QStringLiteral("ko_KR"))
    {
        QVERIFY(application.removeTranslator(&translator));
    }
}

QTEST_MAIN(ScheduleImportWorkflowTests)

#include "schedule_import_workflow_tests.moc"
