#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "features/setup/ui/initial_setup_wizard.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/widgets/on_screen_keyboard.h"
#include "features/my_info/data/signature_image_processor.h"
#include "app/services/feature_services.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/widgets/sectioncards/class_time_row.h"
#include "ui/shared/widgets/sections/class_schedule_section.h"

#include <QApplication>
#include <QBuffer>
#include <QComboBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

#include <utility>

namespace
{

constexpr auto SignatureImageKey = "myInfo/signatureImage";
constexpr auto DisplayNameKey = "myInfo/name";
constexpr auto CampusKey = "myInfo/campus";
constexpr auto ZoomLoginIdKey = "myInfo/zoomLoginId";
constexpr auto ZoomPasswordKey = "myInfo/zoomPassword";
constexpr auto ZoomNotAvailableKey = "myInfo/zoomNotAvailable";
constexpr auto LegacyZoomLoginIdKey = "subPrep/personalZoomEmail";
constexpr auto LegacyZoomPasswordKey = "subPrep/personalZoomPassword";
constexpr auto LegacyZoomNotAvailableKey = "subPrep/personalZoomNotAvailable";
constexpr auto SignatureModeKey = "myInfo/signatureMode";
constexpr auto TypedSignatureTextKey = "myInfo/typedSignatureText";
constexpr auto TypedSignatureFontKey = "myInfo/typedSignatureFont";
constexpr auto UnrelatedKey = "myInfo/unrelatedPreference";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("initial-setup-wizard-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool openDatabase(
    ApplicationServices& services,
    QTemporaryDir& directory
    )
{
    return services.openDatabase(databasePath(directory)).has_value();
}

QByteArray sourcePng()
{
    QImage source(2, 1, QImage::Format_RGB32);
    source.setPixelColor(0, 0, Qt::white);
    source.setPixelColor(1, 0, Qt::black);

    QByteArray encoded;
    QBuffer buffer(&encoded);
    if (!buffer.open(QIODevice::WriteOnly) || !source.save(&buffer, "PNG"))
    {
        return {};
    }

    return encoded;
}

QLabel* signaturePreview(InitialSetupWizard& wizard)
{
    return wizard.findChild<QLabel*>(
        QStringLiteral("signatureImagePreview"));
}

void showPersonalDetailsPage(InitialSetupWizard& wizard)
{
    wizard.setStartId(InitialSetupWizard::PersonalDetailsPage);
    wizard.show();
    QApplication::processEvents();
}

bool executeSql(
    ApplicationServices& services,
    const QString& statement
    )
{
    DataService* dataService = services.dataService();
    if (!dataService || !dataService->databaseSession())
    {
        return false;
    }

    QSqlQuery query(dataService->databaseSession()->database());
    return query.exec(statement);
}

bool installFailingSignaturePreferenceReadView(
    ApplicationServices& services
    )
{
    return executeSql(
               services,
               QStringLiteral(
                   "ALTER TABLE app_settings "
                   "RENAME TO app_settings_preserved"
                   )
               )
        && executeSql(
            services,
            QStringLiteral(R"(
                CREATE VIEW app_settings AS
                SELECT
                    key,
                    CASE
                        WHEN key IN (
                            'myInfo/signatureMode',
                            'myInfo/typedSignatureFont',
                            'myInfo/typedSignatureText'
                        )
                        THEN abs(-9223372036854775807 - 1)
                        ELSE value
                    END AS value
                FROM app_settings_preserved
            )")
            );
}

bool restoreSettingsTable(ApplicationServices& services)
{
    return executeSql(services, QStringLiteral("DROP VIEW app_settings"))
        && executeSql(
            services,
            QStringLiteral(
                "ALTER TABLE app_settings_preserved RENAME TO app_settings"
                )
            );
}

bool addTeacher(ApplicationServices& services, const QString& name)
{
    Teacher teacher;
    teacher.teacherEn = name;
    teacher.teacherKr = QStringLiteral("Korean Name");
    teacher.preferredName = name;
    TeacherRepository* const repository =
        services.databaseSession()->teacherRepository();
    if (!repository)
    {
        return false;
    }
    return repository->createTeacher(teacher).has_value();
}

struct ScopedUserPromptService final
{
    explicit ScopedUserPromptService(IUserPromptService* service)
    {
        DialogServices::setUserPromptServiceForTesting(service);
    }

    ~ScopedUserPromptService()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }
};

} // namespace

class InitialSetupWizardTests : public QObject
{
    Q_OBJECT

private slots:
    void keyboardIsAvailableAtTextEntryStages();
    void personalNameFieldRetainsFocusWhileTyping();
    void storedSignatureImagePreviewsAndSurvivesSaveWithoutReplacement();
    void missingInvalidAndUnavailableSignatureImagesStayEmpty();
    void storedDisplayNamePreservesUtf8AndWhitespace();
    void missingAndUnavailableDisplayNameStayEmpty();
    void classDetailsShowsOrderedPreferredTeacherNamesAndSelectsSingleTeacher();
    void classDetailsShowsTeacherReadFailureAndLeavesChoicesEmpty();
    void classTimesRejectsNoncanonicalWeekdaysAfterCreatingBlankClass();
    void personalDetailsRoutesToClassDetailsWhenTeachersExist();
    void personalDetailsRoutesToTeacherEntryWhenNoTeachersExist();
    void blankTeacherEntrySkipsWhenTeachersExist();
    void teacherChoiceReadFailuresBehaveAsEmptyWithoutWarnings();
    void unavailableSettingsBlockPersonalDetailsValidationWithoutMutation();
    void campusReadFailureBlocksAggregateSaveAndCanRetry();
    void signaturePreferenceReadFailureBlocksAggregateSaveAndCanRetry();
    void aggregateSavePreservesAllPersonalDetailsWithoutDataLoss();
    void aggregateSaveFailureLeavesAllPersonalDetailsUnchanged();
    void aggregateSaveComposesTypedReadsWithoutDataLoss();

private:
    QTemporaryDir m_directory;
};

void InitialSetupWizardTests::
classTimesRejectsNoncanonicalWeekdaysAfterCreatingBlankClass()
{
    const QStringList invalidWeekdays{
        QStringLiteral("monday"),
        QStringLiteral(" Monday ")
    };

    for (const QString& invalidWeekday : invalidWeekdays)
    {
        ApplicationServices services;
        QVERIFY(openDatabase(services, m_directory));
        Teacher teacher;
        teacher.teacherEn = QStringLiteral("F723 Setup Teacher");
        teacher.teacherKr = QStringLiteral("Setup Teacher");
        TeacherRepository* const teacherRepository =
            services.databaseSession()->teacherRepository();
        QVERIFY(teacherRepository);
        const auto teacherId = teacherRepository->createTeacher(teacher);
        QVERIFY(teacherId);

        ClassInfo draft;
        draft.teacherId = *teacherId;
        draft.classGrade = QStringLiteral("E4");
        draft.classLevel = QStringLiteral("Theseus");
        draft.readingBook = QStringLiteral("Reading Explorer 1");
        draft.essayBook = QStringLiteral("4A");
        draft.classColor = QStringLiteral("#123456");
        draft.fontColor = QStringLiteral("#FFFFFF");

        FakeUserPromptService prompts;
        [[maybe_unused]] const ScopedUserPromptService promptService(&prompts);
        InitialSetupWizard wizard(&services);
        wizard.setClassDraft(draft);
        wizard.setStartId(InitialSetupWizard::ClassTimesPage);
        wizard.show();
        QApplication::processEvents();
        QCOMPARE(wizard.currentId(), InitialSetupWizard::ClassTimesPage);

        auto* schedules = wizard.findChild<ClassScheduleSection*>(
            QStringLiteral("setupClassSchedules")
            );
        QVERIFY(schedules);
        QVERIFY(!schedules->regularRows().isEmpty());
        ClassTimeRow* const row = schedules->regularRows().constFirst();
        QVERIFY(row);
        row->setStartTime(QStringLiteral("9:00 AM"));
        row->setEndTime(QStringLiteral("9:55 AM"));
        row->dayCombo()->addItem(invalidWeekday, invalidWeekday);
        row->dayCombo()->setCurrentIndex(row->dayCombo()->count() - 1);
        QCOMPARE(row->day(), invalidWeekday);
        QCOMPARE(row->startTime(), QStringLiteral("9:00 AM"));
        QCOMPARE(row->endTime(), QStringLiteral("9:55 AM"));

        auto* next = wizard.button(QWizard::NextButton);
        QVERIFY(next);
        next->click();
        QApplication::processEvents();

        QCOMPARE(wizard.currentId(), InitialSetupWizard::ClassTimesPage);
        QCOMPARE(prompts.messages.size(), 1);
        QCOMPARE(
            prompts.messages.constFirst().severity,
            PromptSeverity::Warning
            );
        QCOMPARE(
            prompts.messages.constFirst().message,
            QStringLiteral("The class information could not be saved.")
            );

        DatabaseSession* const session = services.databaseSession();
        QVERIFY(session);
        QSqlQuery query(session->database());
        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM classes")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
        QVERIFY(query.exec(QStringLiteral("SELECT name FROM classes")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QString());
        QVERIFY(!query.next());

        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM class_info")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM class_times")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
        QVERIFY(query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM class_intensive_times")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
    }
}

void InitialSetupWizardTests::
classDetailsShowsOrderedPreferredTeacherNamesAndSelectsSingleTeacher()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.teacherService());

    Teacher romanizedTeacher;
    romanizedTeacher.teacherEn = QString();
    romanizedTeacher.teacherKr = QStringLiteral("Korean Beta");
    romanizedTeacher.preferredRomanization = QStringLiteral("Roman Beta");
    TeacherRepository* const teacherRepository =
        services.databaseSession()->teacherRepository();
    QVERIFY(teacherRepository);
    const auto romanizedId = teacherRepository->createTeacher(romanizedTeacher);
    QVERIFY(romanizedId);

    Teacher englishTeacher;
    englishTeacher.teacherEn = QStringLiteral("Alpha");
    englishTeacher.teacherKr = QStringLiteral("Korean Alpha");
    englishTeacher.preferredRomanization = QStringLiteral("Roman Alpha");
    const auto englishId = teacherRepository->createTeacher(englishTeacher);
    QVERIFY(englishId);

    Teacher preferredTeacher;
    preferredTeacher.teacherEn = QStringLiteral("Zulu");
    preferredTeacher.teacherKr = QStringLiteral("Korean Zulu");
    preferredTeacher.preferredRomanization = QStringLiteral("Roman Zulu");
    preferredTeacher.preferredName = QStringLiteral("Preferred Zulu");
    const auto preferredId = teacherRepository->createTeacher(preferredTeacher);
    QVERIFY(preferredId);

    InitialSetupWizard wizard(&services);
    wizard.setStartId(InitialSetupWizard::ClassDetailsPage);
    wizard.show();
    QApplication::processEvents();

    QCOMPARE(wizard.currentId(), InitialSetupWizard::ClassDetailsPage);
    auto* teachers = wizard.findChild<QComboBox*>(
        QStringLiteral("setupClassTeacher"));
    QVERIFY(teachers);
    QCOMPARE(teachers->count(), 4);
    QCOMPARE(teachers->itemText(0), QStringLiteral("Select a teacher..."));
    QCOMPARE(teachers->itemData(0).toInt(), -1);
    QCOMPARE(teachers->itemText(1), QStringLiteral("Roman Beta"));
    QCOMPARE(teachers->itemData(1).toInt(), romanizedId.value());
    QCOMPARE(teachers->itemText(2), QStringLiteral("Alpha"));
    QCOMPARE(teachers->itemData(2).toInt(), englishId.value());
    QCOMPARE(teachers->itemText(3), QStringLiteral("Preferred Zulu"));
    QCOMPARE(teachers->itemData(3).toInt(), preferredId.value());

    ApplicationServices singleTeacherServices;
    QVERIFY(openDatabase(singleTeacherServices, m_directory));
    Teacher onlyTeacher;
    onlyTeacher.teacherEn = QStringLiteral("Only Teacher");
    onlyTeacher.teacherKr = QStringLiteral("Korean Name");
    TeacherRepository* const singleTeacherRepository =
        singleTeacherServices.databaseSession()->teacherRepository();
    QVERIFY(singleTeacherRepository);
    const auto onlyTeacherId =
        singleTeacherRepository->createTeacher(onlyTeacher);
    QVERIFY(onlyTeacherId);

    InitialSetupWizard singleTeacherWizard(&singleTeacherServices);
    singleTeacherWizard.setStartId(InitialSetupWizard::ClassDetailsPage);
    singleTeacherWizard.show();
    QApplication::processEvents();

    auto* singleChoice = singleTeacherWizard.findChild<QComboBox*>(
        QStringLiteral("setupClassTeacher"));
    QVERIFY(singleChoice);
    QCOMPARE(singleChoice->count(), 1);
    QCOMPARE(singleChoice->currentIndex(), 0);
    QCOMPARE(singleChoice->currentText(), QStringLiteral("Only Teacher"));
    QCOMPARE(singleChoice->currentData().toInt(), onlyTeacherId.value());
}

void InitialSetupWizardTests::
classDetailsShowsTeacherReadFailureAndLeavesChoicesEmpty()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE teachers")));

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    InitialSetupWizard wizard(&services);
    wizard.setStartId(InitialSetupWizard::ClassDetailsPage);
    wizard.show();
    QApplication::processEvents();

    DialogServices::setUserPromptServiceForTesting(nullptr);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title, QStringLiteral("Load Teachers"));
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral("Teachers could not be loaded.")
        );
    QVERIFY(!prompts.messages.constFirst().details.trimmed().isEmpty());
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);

    auto* teachers = wizard.findChild<QComboBox*>(
        QStringLiteral("setupClassTeacher"));
    QVERIFY(teachers);
    QCOMPARE(teachers->count(), 0);
}

void InitialSetupWizardTests::
personalDetailsRoutesToClassDetailsWhenTeachersExist()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(addTeacher(services, QStringLiteral("Existing Teacher")));

    InitialSetupWizard wizard(&services);
    showPersonalDetailsPage(wizard);

    auto* name = wizard.findChild<QLineEdit*>(
        QStringLiteral("setupUserName")
        );
    auto* next = wizard.button(QWizard::NextButton);
    QVERIFY(name);
    QVERIFY(next);
    name->setText(QStringLiteral("Setup User"));
    next->click();
    QApplication::processEvents();

    QCOMPARE(wizard.currentId(), InitialSetupWizard::ClassDetailsPage);
}

void InitialSetupWizardTests::
personalDetailsRoutesToTeacherEntryWhenNoTeachersExist()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    InitialSetupWizard wizard(&services);
    showPersonalDetailsPage(wizard);

    auto* name = wizard.findChild<QLineEdit*>(
        QStringLiteral("setupUserName")
        );
    auto* next = wizard.button(QWizard::NextButton);
    QVERIFY(name);
    QVERIFY(next);
    name->setText(QStringLiteral("Setup User"));
    next->click();
    QApplication::processEvents();

    QCOMPARE(wizard.currentId(), InitialSetupWizard::TeacherEntryPage);
}

void InitialSetupWizardTests::blankTeacherEntrySkipsWhenTeachersExist()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(addTeacher(services, QStringLiteral("Existing Teacher")));

    InitialSetupWizard wizard(&services);
    wizard.setStartId(InitialSetupWizard::TeacherEntryPage);
    wizard.show();
    QApplication::processEvents();
    QCOMPARE(wizard.currentId(), InitialSetupWizard::TeacherEntryPage);

    auto* next = wizard.button(QWizard::NextButton);
    QVERIFY(next);
    next->click();
    QApplication::processEvents();

    QCOMPARE(wizard.currentId(), InitialSetupWizard::ClassDetailsPage);
}

void InitialSetupWizardTests::
teacherChoiceReadFailuresBehaveAsEmptyWithoutWarnings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE teachers")));

    FakeUserPromptService prompts;
    [[maybe_unused]] const ScopedUserPromptService promptService(&prompts);
    InitialSetupWizard wizard(&services);
    showPersonalDetailsPage(wizard);

    auto* name = wizard.findChild<QLineEdit*>(
        QStringLiteral("setupUserName")
        );
    auto* next = wizard.button(QWizard::NextButton);
    QVERIFY(name);
    QVERIFY(next);
    name->setText(QStringLiteral("Setup User"));
    next->click();
    QApplication::processEvents();

    QCOMPARE(wizard.currentId(), InitialSetupWizard::TeacherEntryPage);
    QVERIFY2(prompts.messages.isEmpty(),
             "A failed routing read should behave like an empty list without warning.");
    QVERIFY(prompts.asynchronousMessages.isEmpty());

    next->click();
    QApplication::processEvents();

    QCOMPARE(wizard.currentId(), InitialSetupWizard::TeacherEntryPage);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().severity,
        PromptSeverity::Information
        );
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Teacher Information")
        );
    QVERIFY(
        prompts.messages.constFirst().message.contains(
            QStringLiteral("Complete at least")
            )
        );
    for (const PromptRequest& prompt : prompts.messages)
    {
        QVERIFY(prompt.severity != PromptSeverity::Warning);
    }
    QVERIFY(prompts.asynchronousMessages.isEmpty());
}

void InitialSetupWizardTests::keyboardIsAvailableAtTextEntryStages()
{
    ApplicationServices services;

    for (const auto [pageId, title] : {
             std::pair{
                 InitialSetupWizard::PersonalDetailsPage,
                 QStringLiteral("Add Your Information")},
             std::pair{
                 InitialSetupWizard::TeacherEntryPage,
                 QStringLiteral("Add Korean Teacher(s)")}})
    {
        InitialSetupWizard wizard(&services);
        wizard.setStartId(pageId);
        wizard.show();
        QApplication::processEvents();

        QCOMPARE(wizard.currentId(), pageId);
        QCOMPARE(wizard.currentPage()->title(), title);

        auto* trigger = wizard.findChild<QPushButton*>(
            QStringLiteral("initialSetupKoreanKeyboardButton"));
        auto* keyboard = wizard.findChild<OnScreenKeyboard*>();
        QVERIFY(trigger);
        QVERIFY(keyboard);
        QVERIFY(trigger->isVisible());
        QVERIFY(!trigger->icon().isNull());
        QCOMPARE(trigger->accessibleName(), QStringLiteral("Korean Keyboard"));
        QVERIFY(trigger->geometry().center().x() > wizard.width() * 3 / 4);
        QVERIFY(trigger->geometry().top() < wizard.height() / 4);
        QCOMPARE(
            wizard.contentsRect().right() - trigger->geometry().right(), 20);

        trigger->click();
        QApplication::processEvents();
        QVERIFY(keyboard->isVisible());
        keyboard->close();
    }
}

void InitialSetupWizardTests::personalNameFieldRetainsFocusWhileTyping()
{
    ApplicationServices services;
    InitialSetupWizard wizard(&services);
    wizard.setStartId(InitialSetupWizard::PersonalDetailsPage);
    wizard.show();
    QApplication::processEvents();

    auto* name = wizard.findChild<QLineEdit*>(
        QStringLiteral("setupUserName"));
    QVERIFY(name);

    name->setFocus();
    QTRY_VERIFY(name->hasFocus());

    for (const auto key : {Qt::Key_A, Qt::Key_L, Qt::Key_E, Qt::Key_X})
    {
        QTest::keyClick(name, key);
        QVERIFY(name->hasFocus());
    }

    QCOMPARE(name->text(), QStringLiteral("alex"));
}

void InitialSetupWizardTests::
storedSignatureImagePreviewsAndSurvivesSaveWithoutReplacement()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.settingsService());

    const QByteArray source = sourcePng();
    QVERIFY(!source.isEmpty());
    QVERIFY(
        services.settingsService()->save(
            QString::fromUtf8(SignatureImageKey),
            QString::fromLatin1(source.toBase64())
            )
        );

    InitialSetupWizard wizard(&services);
    showPersonalDetailsPage(wizard);

    auto* preview = signaturePreview(wizard);
    QVERIFY(preview);
    QVERIFY(preview->text().isEmpty());

    auto* name = wizard.findChild<QLineEdit*>(
        QStringLiteral("setupUserName"));
    QVERIFY(name);
    name->setText(QStringLiteral("Stored Teacher"));

    auto* next = wizard.button(QWizard::NextButton);
    QVERIFY(next);
    next->click();
    QApplication::processEvents();

    const QByteArray expected =
        SignatureImage::prepareForEmbedding(source);
    const QByteArray stored = services.settingsService()->loadOrDefault(
        QString::fromUtf8(SignatureImageKey),
        QVariant()
        ).toString().toLatin1();
    QCOMPARE(stored, expected.toBase64());
}

void InitialSetupWizardTests::missingInvalidAndUnavailableSignatureImagesStayEmpty()
{
    {
        ApplicationServices services;
        QVERIFY(openDatabase(services, m_directory));

        InitialSetupWizard wizard(&services);
        showPersonalDetailsPage(wizard);

        auto* preview = signaturePreview(wizard);
        QVERIFY(preview);
        QVERIFY(!preview->text().isEmpty());
    }

    {
        ApplicationServices services;
        QVERIFY(openDatabase(services, m_directory));
        QVERIFY(services.settingsService());
        QVERIFY(
            services.settingsService()->save(
                QString::fromUtf8(SignatureImageKey),
                QStringLiteral("%%%not-base64%%")
                )
            );

        InitialSetupWizard wizard(&services);
        showPersonalDetailsPage(wizard);

        auto* preview = signaturePreview(wizard);
        QVERIFY(preview);
        QVERIFY(!preview->text().isEmpty());
    }

    {
        ApplicationServices services;
        InitialSetupWizard wizard(&services);
        showPersonalDetailsPage(wizard);

        auto* preview = signaturePreview(wizard);
        QVERIFY(preview);
        QVERIFY(!preview->text().isEmpty());
    }
}

void InitialSetupWizardTests::storedDisplayNamePreservesUtf8AndWhitespace()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.settingsService());

    const QString storedName = QStringLiteral("  홍길동  ");
    QVERIFY(
        services.settingsService()->save(
            QString::fromUtf8(DisplayNameKey),
            storedName
            )
        );

    InitialSetupWizard wizard(&services);
    showPersonalDetailsPage(wizard);

    auto* name = wizard.findChild<QLineEdit*>(
        QStringLiteral("setupUserName"));
    QVERIFY(name);
    QCOMPARE(name->text(), storedName);
}

void InitialSetupWizardTests::missingAndUnavailableDisplayNameStayEmpty()
{
    {
        ApplicationServices services;
        QVERIFY(openDatabase(services, m_directory));

        InitialSetupWizard wizard(&services);
        showPersonalDetailsPage(wizard);

        auto* name = wizard.findChild<QLineEdit*>(
            QStringLiteral("setupUserName"));
        QVERIFY(name);
        QVERIFY(name->text().isEmpty());
    }

    {
        ApplicationServices services;
        InitialSetupWizard wizard(&services);
        showPersonalDetailsPage(wizard);

        auto* name = wizard.findChild<QLineEdit*>(
            QStringLiteral("setupUserName"));
        QVERIFY(name);
        QVERIFY(name->text().isEmpty());
    }
}

void InitialSetupWizardTests::
unavailableSettingsBlockPersonalDetailsValidationWithoutMutation()
{
    ApplicationServices services;
    QVERIFY(services.settingsService());
    QVERIFY(!services.settingsService()->isAvailable());

    InitialSetupWizard wizard(&services);
    showPersonalDetailsPage(wizard);

    auto* name = wizard.findChild<QLineEdit*>(
        QStringLiteral("setupUserName")
        );
    auto* preview = signaturePreview(wizard);
    auto* next = wizard.button(QWizard::NextButton);
    QVERIFY(name);
    QVERIFY(preview);
    QVERIFY(next);
    QVERIFY(next->isEnabled());

    const QString existingName =
        QStringLiteral("  Existing Teacher  ");
    name->setText(existingName);
    const QString existingPreviewText = preview->text();

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    next->click();
    QApplication::processEvents();
    DialogServices::setUserPromptServiceForTesting(nullptr);

    QCOMPARE(wizard.currentId(), InitialSetupWizard::PersonalDetailsPage);
    QCOMPARE(name->text(), existingName);
    QCOMPARE(preview->text(), existingPreviewText);
    QVERIFY(prompts.messages.isEmpty());
}

void InitialSetupWizardTests::
aggregateSavePreservesAllPersonalDetailsWithoutDataLoss()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.settingsService());

    const QByteArray source = sourcePng();
    QVERIFY(!source.isEmpty());
    const QString typedText = QString::fromUtf8(
        "  \xEA\xB9\x80\xEC\x84\xA0\xEC\x83\x9D\xEB\x8B\x98 / "
        "\xF0\x9F\xA7\xAD  "
        );
    const QVariantMap initialValues = {
        {QString::fromUtf8(DisplayNameKey), QStringLiteral("Old Name")},
        {QString::fromUtf8(CampusKey), QStringLiteral("Jeongja")},
        {QString::fromUtf8(ZoomLoginIdKey), QStringLiteral("old-login")},
        {QString::fromUtf8(ZoomPasswordKey), QStringLiteral("old-password")},
        {QString::fromUtf8(ZoomNotAvailableKey), false},
        {
            QString::fromUtf8(SignatureImageKey),
            QString::fromLatin1(source.toBase64())
        },
        {QString::fromUtf8(SignatureModeKey), 1},
        {QString::fromUtf8(TypedSignatureTextKey), typedText},
        {QString::fromUtf8(TypedSignatureFontKey), 2},
        {QString::fromUtf8(UnrelatedKey), QStringLiteral("preserved")}
    };
    QVERIFY(services.settingsService()->saveAll(initialValues));

    InitialSetupWizard wizard(&services);
    showPersonalDetailsPage(wizard);

    auto* name = wizard.findChild<QLineEdit*>(
        QStringLiteral("setupUserName"));
    QVERIFY(name);
    name->setText(QStringLiteral("  New Teacher  "));

    auto* next = wizard.button(QWizard::NextButton);
    QVERIFY(next);
    next->click();
    QApplication::processEvents();

    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(DisplayNameKey), QVariant()
            ).toString(),
        QStringLiteral("New Teacher")
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(CampusKey), QVariant()
            ).toString(),
        initialValues.value(QString::fromUtf8(CampusKey)).toString()
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(ZoomLoginIdKey), QVariant()
            ).toString(),
        initialValues.value(QString::fromUtf8(ZoomLoginIdKey)).toString()
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(ZoomPasswordKey), QVariant()
            ).toString(),
        initialValues.value(QString::fromUtf8(ZoomPasswordKey)).toString()
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(ZoomNotAvailableKey), QVariant()
            ).toBool(),
        false
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(SignatureImageKey), QVariant()
            ).toString(),
        QString::fromLatin1(
            SignatureImage::prepareForEmbedding(source).toBase64()
            )
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(SignatureModeKey), QVariant()
            ).toInt(),
        1
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(TypedSignatureTextKey), QVariant()
            ).toString(),
        typedText
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(TypedSignatureFontKey), QVariant()
            ).toInt(),
        2
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(UnrelatedKey), QVariant()
            ).toString(),
        QStringLiteral("preserved")
        );
}

void InitialSetupWizardTests::
campusReadFailureBlocksAggregateSaveAndCanRetry()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.settingsService());

    const QVariantMap initialValues = {
        {QString::fromUtf8(DisplayNameKey), QStringLiteral("Old Name")},
        {QString::fromUtf8(CampusKey), QStringLiteral("Bundang")},
        {QString::fromUtf8(ZoomLoginIdKey), QStringLiteral("old-login")},
        {QString::fromUtf8(ZoomPasswordKey), QStringLiteral("old-password")}
    };
    QVERIFY(services.settingsService()->saveAll(initialValues));

    FakeUserPromptService prompts;
    [[maybe_unused]] const ScopedUserPromptService promptService(&prompts);

    InitialSetupWizard wizard(&services);
    showPersonalDetailsPage(wizard);

    auto* const name = wizard.findChild<QLineEdit*>(
        QStringLiteral("setupUserName")
        );
    auto* const next = wizard.button(QWizard::NextButton);
    QVERIFY(name);
    QVERIFY(next);
    name->setText(QStringLiteral("New Name"));

    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "ALTER TABLE app_settings RENAME TO app_settings_preserved"
            )
        ));

    next->click();
    QApplication::processEvents();

    QCOMPARE(wizard.currentId(), InitialSetupWizard::PersonalDetailsPage);
    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& warning = prompts.messages.constFirst();
    QCOMPARE(warning.title, QStringLiteral("Initial Setup"));
    QVERIFY(warning.message.contains(QStringLiteral("Retry by selecting Next")));
    QVERIFY(warning.details.contains(
        QStringLiteral("Loading application setting")
        ));

    QSqlQuery preservedSettings(services.databaseSession()->database());
    QVERIFY(preservedSettings.exec(QStringLiteral(
        "SELECT value FROM app_settings_preserved WHERE key='myInfo/name'"
        )));
    QVERIFY(preservedSettings.next());
    QCOMPARE(preservedSettings.value(0).toString(), QStringLiteral("Old Name"));
    QVERIFY(preservedSettings.exec(QStringLiteral(
        "SELECT value FROM app_settings_preserved WHERE key='myInfo/campus'"
        )));
    QVERIFY(preservedSettings.next());
    QCOMPARE(preservedSettings.value(0).toString(), QStringLiteral("Bundang"));

    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "ALTER TABLE app_settings_preserved RENAME TO app_settings"
            )
        ));
    next->click();
    QApplication::processEvents();

    QVERIFY(wizard.currentId() != InitialSetupWizard::PersonalDetailsPage);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(DisplayNameKey), QVariant()
            ).toString(),
        QStringLiteral("New Name")
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(CampusKey), QVariant()
            ).toString(),
        QStringLiteral("Bundang")
        );
}

void InitialSetupWizardTests::
signaturePreferenceReadFailureBlocksAggregateSaveAndCanRetry()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.settingsService());

    const QString typedText = QString::fromUtf8(
        "  \xEA\xB9\x80\xEC\x84\xA0\xEC\x83\x9D\xEB\x8B\x98 / "
        "\xF0\x9F\xA7\xAD  "
        );
    const QVariantMap initialValues = {
        {QString::fromUtf8(DisplayNameKey), QStringLiteral("Old Name")},
        {QString::fromUtf8(CampusKey), QStringLiteral("Bundang")},
        {QString::fromUtf8(ZoomLoginIdKey), QStringLiteral("old-login")},
        {QString::fromUtf8(ZoomPasswordKey), QStringLiteral("old-password")},
        {QString::fromUtf8(ZoomNotAvailableKey), false},
        {QString::fromUtf8(SignatureModeKey), 1},
        {QString::fromUtf8(TypedSignatureTextKey), typedText},
        {QString::fromUtf8(TypedSignatureFontKey), 2},
        {QString::fromUtf8(UnrelatedKey), QStringLiteral("preserved")}
    };
    QVERIFY(services.settingsService()->saveAll(initialValues));

    FakeUserPromptService prompts;
    [[maybe_unused]] const ScopedUserPromptService promptService(&prompts);

    InitialSetupWizard wizard(&services);
    showPersonalDetailsPage(wizard);

    auto* const name = wizard.findChild<QLineEdit*>(
        QStringLiteral("setupUserName")
        );
    auto* const next = wizard.button(QWizard::NextButton);
    QVERIFY(name);
    QVERIFY(next);
    name->setText(QStringLiteral("New Name"));

    QVERIFY(installFailingSignaturePreferenceReadView(services));
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(
            QStringLiteral("Failed to load setting.*myInfo/signatureMode.*")
            )
        );
    next->click();
    QApplication::processEvents();

    QCOMPARE(wizard.currentId(), InitialSetupWizard::PersonalDetailsPage);
    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& warning = prompts.messages.constFirst();
    QCOMPARE(warning.title, QStringLiteral("Initial Setup"));
    QVERIFY(warning.message.contains(QStringLiteral("not saved")));
    QVERIFY(warning.message.contains(QStringLiteral("Retry by selecting Next")));
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(DisplayNameKey), QVariant()
            ).toString(),
        QStringLiteral("Old Name")
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(CampusKey), QVariant()
            ).toString(),
        QStringLiteral("Bundang")
        );

    QVERIFY(restoreSettingsTable(services));
    for (auto setting = initialValues.cbegin();
         setting != initialValues.cend();
         ++setting)
    {
        QCOMPARE(
            services.settingsService()->loadOrDefault(
                setting.key(), QVariant()
                ),
            setting.value()
            );
    }

    next->click();
    QApplication::processEvents();

    QVERIFY(wizard.currentId() != InitialSetupWizard::PersonalDetailsPage);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(DisplayNameKey), QVariant()
            ).toString(),
        QStringLiteral("New Name")
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(SignatureModeKey), QVariant()
            ).toInt(),
        1
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(TypedSignatureTextKey), QVariant()
            ).toString(),
        typedText
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(TypedSignatureFontKey), QVariant()
            ).toInt(),
        2
        );
}

void InitialSetupWizardTests::
aggregateSaveFailureLeavesAllPersonalDetailsUnchanged()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.settingsService());

    const QVariantMap initialValues = {
        {QString::fromUtf8(DisplayNameKey), QStringLiteral("Old Name")},
        {QString::fromUtf8(CampusKey), QStringLiteral("Jeongja")},
        {QString::fromUtf8(ZoomLoginIdKey), QStringLiteral("old-login")},
        {QString::fromUtf8(ZoomPasswordKey), QStringLiteral("old-password")},
        {QString::fromUtf8(ZoomNotAvailableKey), true},
        {QString::fromUtf8(SignatureImageKey), QStringLiteral("old-image")},
        {QString::fromUtf8(SignatureModeKey), 0},
        {QString::fromUtf8(TypedSignatureTextKey), QStringLiteral("old text")},
        {QString::fromUtf8(TypedSignatureFontKey), 1},
        {QString::fromUtf8(UnrelatedKey), QStringLiteral("preserved")}
    };
    QVERIFY(services.settingsService()->saveAll(initialValues));
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_initial_setup_personal_details_save
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'myInfo/typedSignatureFont'
            BEGIN
                SELECT RAISE(ABORT, 'forced initial setup save failure');
            END
        )")
        ));

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    InitialSetupWizard wizard(&services);
    showPersonalDetailsPage(wizard);
    auto* name = wizard.findChild<QLineEdit*>(
        QStringLiteral("setupUserName"));
    QVERIFY(name);
    name->setText(QStringLiteral("New Teacher"));

    auto* next = wizard.button(QWizard::NextButton);
    QVERIFY(next);
    next->click();
    QApplication::processEvents();

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title, QStringLiteral("Initial Setup"));
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral("Your personal information could not be saved.")
        );

    for (auto setting = initialValues.cbegin();
         setting != initialValues.cend();
         ++setting)
    {
        QCOMPARE(
            services.settingsService()->loadOrDefault(
                setting.key(), QVariant()
                ),
            setting.value()
            );
    }

    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void InitialSetupWizardTests::
aggregateSaveComposesTypedReadsWithoutDataLoss()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.settingsService());

    const QByteArray source = sourcePng();
    QVERIFY(!source.isEmpty());
    const QString typedText = QString::fromUtf8(
        "  \xEA\xB9\x80\xEC\x84\xA0\xEC\x83\x9D\xEB\x8B\x98 / "
        "\xF0\x9F\xA7\xAD  "
        );
    const QVariantMap initialValues = {
        {QString::fromUtf8(CampusKey), QStringLiteral("  Seoul  ")},
        {
            QString::fromUtf8(LegacyZoomLoginIdKey),
            QStringLiteral("legacy@example.com")
        },
        {
            QString::fromUtf8(LegacyZoomPasswordKey),
            QStringLiteral("legacy secret")
        },
        {QString::fromUtf8(LegacyZoomNotAvailableKey), false},
        {
            QString::fromUtf8(SignatureImageKey),
            QString::fromLatin1(source.toBase64())
        },
        {QString::fromUtf8(SignatureModeKey), 1},
        {QString::fromUtf8(TypedSignatureTextKey), typedText},
        {QString::fromUtf8(TypedSignatureFontKey), 2},
        {QString::fromUtf8(UnrelatedKey), QStringLiteral("preserved")}
    };
    QVERIFY(services.settingsService()->saveAll(initialValues));

    InitialSetupWizard wizard(&services);
    showPersonalDetailsPage(wizard);

    auto* name = wizard.findChild<QLineEdit*>(
        QStringLiteral("setupUserName"));
    QVERIFY(name);
    name->setText(QStringLiteral("  New Teacher  "));

    auto* next = wizard.button(QWizard::NextButton);
    QVERIFY(next);
    next->click();
    QApplication::processEvents();

    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(DisplayNameKey), QVariant()
            ).toString(),
        QStringLiteral("New Teacher")
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(CampusKey), QVariant()
            ).toString(),
        QStringLiteral("  Seoul  ")
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(ZoomLoginIdKey), QVariant()
            ).toString(),
        QStringLiteral("legacy@example.com")
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(ZoomPasswordKey), QVariant()
            ).toString(),
        QStringLiteral("legacy secret")
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(ZoomNotAvailableKey), QVariant()
            ).toBool(),
        false
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(SignatureImageKey), QVariant()
            ).toString(),
        QString::fromLatin1(
            SignatureImage::prepareForEmbedding(source).toBase64()
            )
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(SignatureModeKey), QVariant()
            ).toInt(),
        1
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(TypedSignatureTextKey), QVariant()
            ).toString(),
        typedText
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(TypedSignatureFontKey), QVariant()
            ).toInt(),
        2
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(UnrelatedKey), QVariant()
            ).toString(),
        QStringLiteral("preserved")
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(LegacyZoomLoginIdKey), QVariant()
            ).toString(),
        QStringLiteral("legacy@example.com")
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(LegacyZoomPasswordKey), QVariant()
            ).toString(),
        QStringLiteral("legacy secret")
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(LegacyZoomNotAvailableKey), QVariant()
            ).toBool(),
        false
        );
}

QTEST_MAIN(InitialSetupWizardTests)

#include "initial_setup_wizard_tests.moc"
