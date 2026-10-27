#include "features/teacher/import/teacher_import_file_validator.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/resources/resource_scope.h"
#include "next/ui/initial_setup_teacher_import_dialog.h"
#include "next/ui/teacher_import_request_mapper.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest/QtTest>

#include <atomic>
#include <expected>
#include <optional>
#include <string>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

QString fixturePath()
{
    return QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
        QStringLiteral("tests/fixtures/teacher_import/sectioned_review.xlsx")
        );
}

QString temporaryResourceCatalog()
{
    return QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG);
}

QString bakedResourceRoot()
{
    return QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
}

QString compiledTranslationDirectory()
{
    return QStringLiteral(CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY);
}

bool copyFile(const QString& source, const QString& destination)
{
    if (!QDir().mkpath(QFileInfo(destination).absolutePath()))
    {
        return false;
    }
    QFile::remove(destination);
    return QFile::copy(source, destination);
}

QString importStatus(const Ui::InitialSetupTeacherImportDialog& dialog)
{
    const auto* status = dialog.findChild<QLabel*>(
        QStringLiteral("classMngrNextTeacherImportStatus"));
    return status ? status->text() : QString();
}

class StylesheetGuard final
{
public:
    explicit StylesheetGuard(QApplication& application)
        : m_application(application)
        , m_previous(application.styleSheet())
    {
    }

    ~StylesheetGuard()
    {
        m_application.setStyleSheet(m_previous);
    }

private:
    QApplication& m_application;
    QString m_previous;
};

class ResourceFixture final
{
public:
    bool prepare(const QString& locale)
    {
        if (!m_directory.isValid())
        {
            return false;
        }
        m_root = m_directory.filePath(QStringLiteral("resources"));
        const QString translations = QDir(m_root).filePath(
            QStringLiteral("assets/translations"));
        const QString styles = QDir(m_root).filePath(QStringLiteral("assets/styles"));
        if (!QDir().mkpath(translations) || !QDir().mkpath(styles))
        {
            return false;
        }
        for (const QString& theme : {QStringLiteral("light"), QStringLiteral("dark")})
        {
            const QString relative = QStringLiteral("assets/styles/%1.qss").arg(theme);
            if (!copyFile(QDir(bakedResourceRoot()).filePath(relative),
                          QDir(m_root).filePath(relative)))
            {
                return false;
            }
        }
        const QString qmName = QStringLiteral("ClassMngr_%1.qm").arg(locale);
        return copyFile(
            QDir(compiledTranslationDirectory()).filePath(qmName),
            QDir(translations).filePath(qmName));
    }

    [[nodiscard]] const QString& root() const noexcept
    {
        return m_root;
    }

private:
    QTemporaryDir m_directory;
    QString m_root;
};

Resources::ResourceRequest requestFor(
    const QString& resourceId,
    const QString& owner
    )
{
    const auto id = Resources::ResourceId::fromString(resourceId);
    if (!id)
    {
        qFatal("resource ID was rejected");
    }
    auto request = Resources::ResourceRequest::create(
        *id,
        Resources::ResourceScopeKind::Operation,
        owner);
    if (!request)
    {
        qFatal("resource request was rejected");
    }
    return std::move(*request);
}

}  // namespace

class TeacherImportWorkflowTests final : public QObject
{
    Q_OBJECT

private slots:
    void fixtureBuildsTypedRequestWithReviewChoices();
    void dialogKeepsReviewOnApplyFailureAndRetries();
    void timeoutRetryAndStaleValidationResultAreSafe();
    void englishAndKoreanLabelsLoadAcrossLightAndDarkThemes_data();
    void englishAndKoreanLabelsLoadAcrossLightAndDarkThemes();
};

void TeacherImportWorkflowTests::fixtureBuildsTypedRequestWithReviewChoices()
{
    const TeacherImportFileValidation validation =
        validateTeacherImportFile(fixturePath());
    QVERIFY2(validation.isValid(),
             qPrintable(validation.diagnostics.join(QLatin1Char('\n'))));
    QCOMPARE(validation.templateId, QStringLiteral("sectioned-contact-list-v1"));
    QCOMPARE(validation.sourceDate, QDate(2026, 9, 1));
    QCOMPARE(validation.discoveredSections,
             QStringList({QStringLiteral("M1"), QStringLiteral("M2"),
                          QStringLiteral("H1"), QStringLiteral("Ntr"),
                          QStringLiteral("GS")}));
    QCOMPARE(validation.previewCounts.koreanTeachers, 4);
    QCOMPARE(validation.previewCounts.nativeEnglishTeachers, 1);
    QCOMPARE(validation.previewCounts.gsTeamMembers, 1);
    QCOMPARE(validation.preview.koreanGroups.size(), 3);
    QVERIFY(validation.preview.koreanGroups.at(0).candidates.at(0).selectedByDefault);
    QVERIFY(!validation.preview.koreanGroups.at(0).candidates.at(1).selectedByDefault);

    using namespace Application;
    const std::vector<TeacherImportGroupDecision> decisions{
        {TeacherImportGroupKey("M1"), TeacherImportGroupMode::Selected,
         {TeacherImportRequestedCandidateIndex{0}}},
        {TeacherImportGroupKey("M2"), TeacherImportGroupMode::None, {}},
        {TeacherImportGroupKey("H1"), TeacherImportGroupMode::All, {}}
    };
    const auto request = Ui::teacherImportPreviewToUseCaseRequest(
        validation.preview,
        decisions);
    QVERIFY(request.has_value());
    QCOMPARE(request->sourceDateIso, std::string("2026-09-01"));
    QCOMPARE(request->validation.koreanTeacherKeys.size(), std::size_t(2));
    QCOMPARE(request->koreanTeachers.size(), std::size_t(2));
    QCOMPARE(request->nativeEnglishTeachers.size(), std::size_t(1));
    QCOMPARE(request->gsTeamMembers.size(), std::size_t(1));
    QVERIFY(request->review.has_value());
    QCOMPARE(request->review->candidateGroups.size(), std::size_t(3));
    QCOMPARE(request->review->decisions.size(), std::size_t(3));
    QCOMPARE(request->review->candidateGroups.at(0).candidateValidationKeys.size(),
             std::size_t(2));
    QCOMPARE(request->review->decisions.at(0).mode, TeacherImportGroupMode::Selected);
    QCOMPARE(request->review->decisions.at(1).mode, TeacherImportGroupMode::None);
    QCOMPARE(request->review->decisions.at(2).mode, TeacherImportGroupMode::All);
    QCOMPARE(request->review->decisions.at(0).selectedCandidateIndexes,
             (std::vector<TeacherImportRequestedCandidateIndex>{
                 TeacherImportRequestedCandidateIndex{0}
             }));
    QCOMPARE(request->koreanTeachers.at(0).teacherKr, std::u16string(u"홍길동"));
    QCOMPARE(request->koreanTeachers.at(1).teacherKr, std::u16string(u"박민준"));
    QCOMPARE(request->validation.gsTeamMemberKeys.size(), std::size_t(1));
}

void TeacherImportWorkflowTests::dialogKeepsReviewOnApplyFailureAndRetries()
{
    const QString path = fixturePath();
    int applyCalls = 0;
    int cancelCalls = 0;
    Application::TeacherImportUseCaseRequest appliedRequest;

    Ui::InitialSetupTeacherImportActions actions;
    actions.chooseWorkbook = [path] {
        return Domain::Result<std::optional<QString>>::success(path);
    };
    actions.validationTimeoutMilliseconds = [] { return 2000; };
    actions.apply = [&] (const Application::TeacherImportUseCaseRequest& request)
        -> Ui::TeacherImportApplyResult {
        ++applyCalls;
        if (applyCalls == 1)
        {
            return std::unexpected(Application::TeacherImportUseCaseError{
                .kind = Application::TeacherImportUseCaseErrorKind::PersistenceFailure,
                .persistenceFailure = {
                    .operation =
                        Application::TeacherImportPersistenceOperation::CreateGsTeamMember,
                    .message = u"simulated failure"
                }
            });
        }
        appliedRequest = request;
        Application::TeacherImportUseCaseResult result;
        result.koreanTeachers.created = Application::TeacherImportCount{
            static_cast<int>(request.koreanTeachers.size())};
        result.nativeEnglishTeachers.created = Application::TeacherImportCount{
            static_cast<int>(request.nativeEnglishTeachers.size())};
        result.gsTeamMembers.created = Application::TeacherImportCount{
            static_cast<int>(request.gsTeamMembers.size())};
        return result;
    };
    actions.cancelSetup = [&] { ++cancelCalls; };

    Ui::InitialSetupTeacherImportDialog dialog(actions);
    dialog.show();
    QTemporaryDir invalidDirectory;
    QVERIFY(invalidDirectory.isValid());
    const QString invalidPath = invalidDirectory.filePath(QStringLiteral("invalid.xlsx"));
    QFile invalidFile(invalidPath);
    QVERIFY(invalidFile.open(QIODevice::WriteOnly));
    QCOMPARE(invalidFile.write(QByteArrayLiteral("not an xlsx")), qint64(11));
    invalidFile.close();
    dialog.setFilePath(invalidPath);
    QTRY_VERIFY_WITH_TIMEOUT(importStatus(dialog).contains(QStringLiteral("invalid")),
                             3000);
    QVERIFY(!dialog.findChild<QPushButton*>(
                 QStringLiteral("classMngrNextTeacherImportApply"))->isEnabled());

    dialog.findChild<QPushButton*>(
        QStringLiteral("classMngrNextTeacherImportBrowse"))->click();
    QTRY_VERIFY_WITH_TIMEOUT(importStatus(dialog).contains(QStringLiteral("Review")),
                             3000);
    QCOMPARE(dialog.findChild<QLineEdit*>(
                 QStringLiteral("classMngrNextTeacherImportFilePath"))->text(), path);

    auto* allM1 = dialog.findChild<QRadioButton*>(
        QStringLiteral("classMngrNextTeacherImportAll_0"));
    auto* selectM1 = dialog.findChild<QRadioButton*>(
        QStringLiteral("classMngrNextTeacherImportSelect_0"));
    auto* noneM2 = dialog.findChild<QRadioButton*>(
        QStringLiteral("classMngrNextTeacherImportNone_1"));
    auto* allH1 = dialog.findChild<QRadioButton*>(
        QStringLiteral("classMngrNextTeacherImportAll_2"));
    auto* firstM1 = dialog.findChild<QCheckBox*>(
        QStringLiteral("classMngrNextTeacherImportCandidate_0_0"));
    auto* secondM1 = dialog.findChild<QCheckBox*>(
        QStringLiteral("classMngrNextTeacherImportCandidate_0_1"));
    QVERIFY(allM1 && selectM1 && noneM2 && allH1 && firstM1 && secondM1);
    QVERIFY(allM1->isChecked());
    QVERIFY(firstM1->isChecked());
    QVERIFY(!secondM1->isChecked());
    selectM1->setChecked(true);
    noneM2->setChecked(true);
    allH1->setChecked(true);

    auto* applyButton = dialog.findChild<QPushButton*>(
        QStringLiteral("classMngrNextTeacherImportApply"));
    QVERIFY(applyButton && applyButton->isEnabled());
    applyButton->click();
    QCOMPARE(applyCalls, 1);
    QVERIFY(importStatus(dialog).contains(QStringLiteral("Import failed")));
    QVERIFY(dialog.result() != QDialog::Accepted);
    QVERIFY(dialog.findChild<QLineEdit*>(
                 QStringLiteral("classMngrNextTeacherImportFilePath"))->text() == path);
    QVERIFY(selectM1->isChecked());
    QVERIFY(noneM2->isChecked());
    QVERIFY(allH1->isChecked());
    QVERIFY(firstM1->isChecked());
    QVERIFY(!secondM1->isChecked());

    applyButton->click();
    QCOMPARE(applyCalls, 2);
    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
    QCOMPARE(
        dialog.successSummary(),
        QStringLiteral(
            "Import complete from 2026-09-01.\n"
            "Korean teachers: 2 created, 0 updated, 0 unchanged.\n"
            "Native English teachers: 1 created, 0 updated, 0 unchanged.\n"
            "GS Team members: 1 created, 0 updated, 0 unchanged."
            )
        );
    QCOMPARE(appliedRequest.koreanTeachers.size(), std::size_t(2));
    QCOMPARE(appliedRequest.nativeEnglishTeachers.size(), std::size_t(1));
    QCOMPARE(appliedRequest.gsTeamMembers.size(), std::size_t(1));
    QCOMPARE(cancelCalls, 0);
}

void TeacherImportWorkflowTests::timeoutRetryAndStaleValidationResultAreSafe()
{
    const QString path = fixturePath();
    std::atomic_int validationCalls{0};
    Ui::InitialSetupTeacherImportActions actions;
    actions.validationTimeoutMilliseconds = [] { return 15; };
    actions.validateWorkbook = [&validationCalls](const QString& requestedPath) {
        const int call = ++validationCalls;
        if (call == 1)
        {
            QThread::msleep(90);
        }
        return validateTeacherImportFile(requestedPath);
    };
    actions.apply = [](const Application::TeacherImportUseCaseRequest&) {
        return Ui::TeacherImportApplyResult{
            std::unexpected(Application::TeacherImportUseCaseError{})
        };
    };

    Ui::InitialSetupTeacherImportDialog timeoutDialog(actions);
    timeoutDialog.show();
    timeoutDialog.setFilePath(path);
    QTRY_VERIFY_WITH_TIMEOUT(importStatus(timeoutDialog).contains(
                                 QStringLiteral("timed out")), 1000);
    auto* retryButton = timeoutDialog.findChild<QPushButton*>(
        QStringLiteral("classMngrNextTeacherImportRetry"));
    QVERIFY(retryButton && retryButton->isVisible());
    QVERIFY(!timeoutDialog.findChild<QPushButton*>(
                 QStringLiteral("classMngrNextTeacherImportApply"))->isEnabled());
    QTest::qWait(120);
    QVERIFY(importStatus(timeoutDialog).contains(QStringLiteral("timed out")));
    retryButton->click();
    QTRY_VERIFY_WITH_TIMEOUT(importStatus(timeoutDialog).contains(
                                 QStringLiteral("Review")), 1000);
    QCOMPARE(validationCalls.load(), 2);

    Ui::InitialSetupTeacherImportActions staleActions;
    staleActions.validationTimeoutMilliseconds = [] { return 500; };
    staleActions.validateWorkbook = [path](const QString& requestedPath) {
        if (requestedPath == QStringLiteral("slow.xlsx"))
        {
            QThread::msleep(100);
            TeacherImportFileValidation stale;
            stale.status = TeacherImportFileStatus::RecognizedButInvalid;
            stale.diagnostics.append(QStringLiteral("stale validation result"));
            return stale;
        }
        return validateTeacherImportFile(path);
    };
    staleActions.apply = actions.apply;
    Ui::InitialSetupTeacherImportDialog staleDialog(staleActions);
    staleDialog.setFilePath(QStringLiteral("slow.xlsx"));
    staleDialog.setFilePath(path);
    QTRY_VERIFY_WITH_TIMEOUT(importStatus(staleDialog).contains(
                                 QStringLiteral("Review")), 1000);
    QTest::qWait(130);
    QVERIFY(importStatus(staleDialog).contains(QStringLiteral("Review")));
    QVERIFY(staleDialog.findChild<QPushButton*>(
                QStringLiteral("classMngrNextTeacherImportApply"))->isEnabled());
}

void TeacherImportWorkflowTests::
englishAndKoreanLabelsLoadAcrossLightAndDarkThemes_data()
{
    QTest::addColumn<QString>("locale");
    QTest::addColumn<QString>("theme");
    QTest::addColumn<QString>("expectedTitle");
    QTest::addColumn<QString>("expectedBrowse");
    QTest::addColumn<QString>("expectedCancel");
    QTest::addColumn<QString>("expectedInitialStatus");
    QTest::addColumn<QString>("expectedThemeColor");
    QTest::newRow("English light")
        << QStringLiteral("en_US") << QStringLiteral("light")
        << QStringLiteral("Import Teachers from Workbook")
        << QStringLiteral("Browse...") << QStringLiteral("Cancel Setup")
        << QStringLiteral("Choose an XLSX teacher workbook to preview.")
        << QStringLiteral("#e9e8e3");
    QTest::newRow("English dark")
        << QStringLiteral("en_US") << QStringLiteral("dark")
        << QStringLiteral("Import Teachers from Workbook")
        << QStringLiteral("Browse...") << QStringLiteral("Cancel Setup")
        << QStringLiteral("Choose an XLSX teacher workbook to preview.")
        << QStringLiteral("#2b2b2b");
    QTest::newRow("Korean light")
        << QStringLiteral("ko_KR") << QStringLiteral("light")
        << QString::fromUtf8("엑셀 파일에서 교사 가져오기")
        << QString::fromUtf8("찾아보기...")
        << QString::fromUtf8("초기 설정 취소")
        << QString::fromUtf8("미리 볼 XLSX 교사 파일을 선택하세요.")
        << QStringLiteral("#e9e8e3");
    QTest::newRow("Korean dark")
        << QStringLiteral("ko_KR") << QStringLiteral("dark")
        << QString::fromUtf8("엑셀 파일에서 교사 가져오기")
        << QString::fromUtf8("찾아보기...")
        << QString::fromUtf8("초기 설정 취소")
        << QString::fromUtf8("미리 볼 XLSX 교사 파일을 선택하세요.")
        << QStringLiteral("#2b2b2b");
}

void TeacherImportWorkflowTests::englishAndKoreanLabelsLoadAcrossLightAndDarkThemes()
{
    QFETCH(QString, locale);
    QFETCH(QString, theme);
    QFETCH(QString, expectedTitle);
    QFETCH(QString, expectedBrowse);
    QFETCH(QString, expectedCancel);
    QFETCH(QString, expectedInitialStatus);
    QFETCH(QString, expectedThemeColor);

    QApplication& application = *qApp;
    StylesheetGuard stylesheetGuard(application);
    ResourceFixture fixture;
    QVERIFY(fixture.prepare(locale));
    auto catalog = Resources::ResourceCatalog::load(
        temporaryResourceCatalog(), fixture.root());
    const QString catalogError = catalog.has_value()
        ? QString()
        : catalog.error().message;
    QVERIFY2(catalog.has_value(), qPrintable(catalogError));
    Resources::ResourceLoader loader(std::move(*catalog));

    auto translationRequest = requestFor(
        QStringLiteral("resource:assets/translations/ClassMngr_%1.ts").arg(locale),
        QStringLiteral("TeacherImportTranslationTest"));
    auto translation = loader.loadTranslation(translationRequest, application);
    const QString translationError = translation.has_value()
        ? QString()
        : translation.error().message;
    QVERIFY2(translation.has_value(), qPrintable(translationError));
    QVERIFY(translation->get().isInstalled());

    auto themeRequest = requestFor(
        QStringLiteral("resource:assets/styles/%1.qss").arg(theme),
        QStringLiteral("TeacherImportThemeTest"));
    auto stylesheet = loader.loadStylesheet(themeRequest);
    const QString stylesheetError = stylesheet.has_value()
        ? QString()
        : stylesheet.error().message;
    QVERIFY2(stylesheet.has_value(), qPrintable(stylesheetError));
    application.setStyleSheet(stylesheet->get());

    Ui::InitialSetupTeacherImportDialog dialog({});
    QCOMPARE(dialog.windowTitle(), expectedTitle);
    QCOMPARE(dialog.findChild<QPushButton*>(
                 QStringLiteral("classMngrNextTeacherImportBrowse"))->text(),
             expectedBrowse);
    QCOMPARE(dialog.findChild<QPushButton*>(
                 QStringLiteral("classMngrNextTeacherImportCancel"))->text(),
             expectedCancel);
    QCOMPARE(importStatus(dialog), expectedInitialStatus);
    QVERIFY(application.styleSheet().contains(expectedThemeColor));
    QVERIFY(dialog.findChild<QLabel*>(
                QStringLiteral("classMngrNextTeacherImportStatus"))
                ->palette().color(QPalette::Window).isValid());
}

QTEST_MAIN(TeacherImportWorkflowTests)

#include "teacher_import_workflow_tests.moc"
