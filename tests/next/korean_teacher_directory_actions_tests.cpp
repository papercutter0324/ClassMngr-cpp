#include "next/ui/korean_teacher_directory_actions.h"

#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QtTest/QtTest>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

[[nodiscard]] Domain::TeacherId teacherId(const qint64 value)
{
    return *Domain::TeacherId::fromString(std::to_string(value));
}

[[nodiscard]] Application::TeacherDirectoryEntry teacher(
    const qint64 id,
    std::u16string english
    )
{
    return {
        .teacherId = teacherId(id),
        .teacherKr = u"홍길동",
        .teacherEn = std::move(english)
    };
}

struct ActionProbe final
{
    Application::TeacherDirectoryReadSnapshot snapshot;
    int directoryReads = 0;
    int createCalls = 0;
    int profileReads = 0;
    int deleteCalls = 0;
    int refreshCalls = 0;
    int createProfileOpens = 0;
    int chooserCalls = 0;
    int confirmationCalls = 0;
    bool failCreate = false;
    bool failProfileRead = false;
    bool failDelete = false;
    bool refreshSucceeds = true;
    bool confirm = true;
    std::optional<Domain::TeacherId> choice;
    std::vector<Domain::TeacherId> deleted;
    std::vector<Domain::TeacherId> createdProfilesOpened;
    std::vector<QString> errors;
    Application::TeacherCreateRequest lastCreateRequest;

    [[nodiscard]] Ui::KoreanTeacherDirectoryActionPorts ports()
    {
        return {
            .readTeachers = [this] {
                ++directoryReads;
                return Application::TeacherDirectoryReadResult::success(snapshot);
            },
            .createTeacher = [this](
                const Application::TeacherCreateRequest& request
                ) {
                ++createCalls;
                lastCreateRequest = request;
                if (failCreate)
                {
                    return Application::TeacherCreateResult::failure({
                        .code = Domain::ErrorCode::Technical,
                        .message = "injected create failure",
                        .recoverable = false
                    });
                }
                if (request.fields.teacherKr.empty()
                    && request.fields.teacherEn.empty()
                    && request.fields.preferredRomanization.empty())
                {
                    return Application::TeacherCreateResult::failure({
                        .code = Domain::ErrorCode::Validation,
                        .message = "A teacher name is required.",
                        .recoverable = true
                    });
                }
                const Domain::TeacherId id = teacherId(2147483650LL);
                snapshot.teachers.push_back(teacher(2147483650LL,
                    request.fields.teacherEn));
                return Application::TeacherCreateResult::success(id);
            },
            .readTeacherProfile = [this](const Domain::TeacherId& id) {
                ++profileReads;
                if (failProfileRead)
                {
                    return Application::TeacherProfileReadResult::failure({
                        .code = Domain::ErrorCode::NotFound,
                        .message = "injected fresh profile read failure",
                        .recoverable = true
                    });
                }
                return Application::TeacherProfileReadResult::success({
                    .teacherId = id,
                    .fields = {
                        .teacherKr = u"홍길동",
                        .teacherEn = u"Hong Gildong"
                    }
                });
            },
            .deleteTeacher = [this](
                const Application::TeacherDeleteRequest& request
                ) {
                ++deleteCalls;
                deleted.push_back(request.teacherId);
                if (failDelete)
                {
                    return Application::TeacherDeleteResult::failure({
                        .code = Domain::ErrorCode::Technical,
                        .message = "injected delete failure",
                        .recoverable = false
                    });
                }
                auto& teachers = snapshot.teachers;
                teachers.erase(
                    std::remove_if(
                        teachers.begin(),
                        teachers.end(),
                        [&request](
                            const Application::TeacherDirectoryEntry& entry
                            ) {
                            return entry.teacherId == request.teacherId;
                        }
                        ),
                    teachers.end()
                    );
                return Application::TeacherDeleteResult::success();
            },
            .refreshTeacherRows = [this] {
                ++refreshCalls;
                return refreshSucceeds;
            },
            .openCreatedTeacherProfile = [this](
                const Domain::TeacherId& id
                ) {
                ++createProfileOpens;
                createdProfilesOpened.push_back(id);
                return true;
            },
            .chooseTeacherToDelete = [this](
                const Application::TeacherDirectoryReadSnapshot& loaded
                ) {
                ++chooserCalls;
                if (loaded.teachers.empty())
                {
                    return std::optional<Domain::TeacherId>{};
                }
                return choice;
            },
            .confirmDelete = [this](
                const Application::TeacherProfileReadSnapshot& profile
                ) {
                ++confirmationCalls;
                return confirm && profile.teacherId.value() != "0";
            },
            .reportError = [this](const QString&, const QString& message) {
                errors.push_back(message);
            }
        };
    }
};

void enterCreateDialog(
    const QString& korean,
    const QString& english,
    const QString& preferred,
    const bool submit
    )
{
    QTimer::singleShot(0, [korean, english, preferred, submit] {
        auto* const dialog = qobject_cast<QDialog*>(
            QApplication::activeModalWidget()
            );
        if (!dialog)
        {
            return;
        }
        dialog->findChild<QLineEdit*>(
            QStringLiteral("teacherCreateKoreanNameEdit")
            )->setText(korean);
        dialog->findChild<QLineEdit*>(
            QStringLiteral("teacherCreateEnglishNameEdit")
            )->setText(english);
        dialog->findChild<QLineEdit*>(
            QStringLiteral("teacherCreatePreferredSpellingEdit")
            )->setText(preferred);
        if (submit)
        {
            dialog->findChild<QPushButton*>(
                QStringLiteral("teacherCreateSubmitButton")
                )->click();
            if (dialog->isVisible())
            {
                dialog->reject();
            }
        }
        else
        {
            dialog->reject();
        }
    });
}

} // namespace

class KoreanTeacherDirectoryActionsTests final : public QObject
{
    Q_OBJECT

private slots:
    void createValidationCancelFailureAndSuccess();
    void deleteChooserFreshReadCancelFailureAndSuccess();
};

void KoreanTeacherDirectoryActionsTests::createValidationCancelFailureAndSuccess()
{
    ActionProbe probe;
    Ui::KoreanTeacherDirectoryActions actions(nullptr, probe.ports());

    bool invalidFormStayedOpen = false;
    QTimer::singleShot(0, [&invalidFormStayedOpen] {
        auto* const dialog = qobject_cast<QDialog*>(
            QApplication::activeModalWidget()
            );
        QVERIFY(dialog);
        dialog->findChild<QPushButton*>(
            QStringLiteral("teacherCreateSubmitButton")
            )->click();
        invalidFormStayedOpen = dialog->isVisible()
            && dialog->findChild<QLabel*>(
                QStringLiteral("teacherCreateStatus")
                )->isVisible();
        dialog->reject();
    });
    actions.createTeacher();
    QVERIFY(invalidFormStayedOpen);
    QCOMPARE(probe.createCalls, 1);
    QVERIFY(probe.snapshot.teachers.empty());
    QCOMPARE(probe.refreshCalls, 0);
    QCOMPARE(probe.createProfileOpens, 0);

    enterCreateDialog({}, {}, {}, false);
    actions.createTeacher();
    QCOMPARE(probe.createCalls, 1);
    QVERIFY(probe.snapshot.teachers.empty());
    QCOMPARE(probe.refreshCalls, 0);
    QCOMPARE(probe.createProfileOpens, 0);

    probe.failCreate = true;
    enterCreateDialog({}, QStringLiteral("Rejected Teacher"), {}, true);
    actions.createTeacher();
    QCOMPARE(probe.createCalls, 2);
    QVERIFY(probe.snapshot.teachers.empty());
    QCOMPARE(probe.refreshCalls, 0);
    QCOMPARE(probe.createProfileOpens, 0);

    probe.failCreate = false;
    enterCreateDialog(QStringLiteral("홍길동"), QStringLiteral("Hong Gildong"),
        QStringLiteral("Hong Gil-dong"), true);
    actions.createTeacher();
    QCOMPARE(probe.createCalls, 3);
    QCOMPARE(probe.lastCreateRequest.fields.teacherKr, u"홍길동");
    QCOMPARE(probe.lastCreateRequest.fields.teacherEn, u"Hong Gildong");
    QCOMPARE(probe.snapshot.teachers.size(), std::size_t(1));
    QCOMPARE(probe.refreshCalls, 1);
    QCOMPARE(probe.createProfileOpens, 1);
    QCOMPARE(probe.createdProfilesOpened.front(), teacherId(2147483650LL));
}

void KoreanTeacherDirectoryActionsTests::
deleteChooserFreshReadCancelFailureAndSuccess()
{
    ActionProbe probe;
    probe.snapshot.teachers = {teacher(7, u"Delete Me")};
    Ui::KoreanTeacherDirectoryActions actions(nullptr, probe.ports());
    const Domain::TeacherId target = teacherId(7);

    probe.choice = std::nullopt;
    actions.deleteTeacher(std::nullopt);
    QCOMPARE(probe.directoryReads, 1);
    QCOMPARE(probe.chooserCalls, 1);
    QCOMPARE(probe.profileReads, 0);
    QCOMPARE(probe.confirmationCalls, 0);
    QCOMPARE(probe.deleteCalls, 0);
    QCOMPARE(probe.refreshCalls, 0);

    probe.choice = target;
    probe.confirm = false;
    actions.deleteTeacher(std::nullopt);
    QCOMPARE(probe.directoryReads, 2);
    QCOMPARE(probe.chooserCalls, 2);
    QCOMPARE(probe.profileReads, 1);
    QCOMPARE(probe.confirmationCalls, 1);
    QCOMPARE(probe.deleteCalls, 0);
    QCOMPARE(probe.refreshCalls, 0);

    probe.confirm = true;
    probe.failProfileRead = true;
    actions.deleteTeacher(target);
    QCOMPARE(probe.profileReads, 2);
    QCOMPARE(probe.confirmationCalls, 1);
    QCOMPARE(probe.deleteCalls, 0);
    QCOMPARE(probe.refreshCalls, 0);
    QCOMPARE(probe.errors.size(), std::size_t(1));

    probe.failProfileRead = false;
    probe.failDelete = true;
    actions.deleteTeacher(target);
    QCOMPARE(probe.profileReads, 3);
    QCOMPARE(probe.confirmationCalls, 2);
    QCOMPARE(probe.deleteCalls, 1);
    QCOMPARE(probe.refreshCalls, 0);
    QCOMPARE(probe.snapshot.teachers.size(), std::size_t(1));
    QCOMPARE(probe.errors.size(), std::size_t(2));

    probe.failDelete = false;
    actions.deleteTeacher(target);
    QCOMPARE(probe.profileReads, 4);
    QCOMPARE(probe.confirmationCalls, 3);
    QCOMPARE(probe.deleteCalls, 2);
    QCOMPARE(probe.deleted.back(), target);
    QCOMPARE(probe.refreshCalls, 1);
    QVERIFY(probe.snapshot.teachers.empty());
}

QTEST_MAIN(KoreanTeacherDirectoryActionsTests)

#include "korean_teacher_directory_actions_tests.moc"
