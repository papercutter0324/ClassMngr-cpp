#include "core/fontmanager.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/pages/scrollable_page_body.h"
#include "ui/shared/validation/form_validation_binder.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>

class PageComponentsTests : public QObject
{
    Q_OBJECT

private slots:
    void autosaveDebouncesAndRequestsNonInteractiveSave();
    void autosaveSuppressesChangesWhileLoading();
    void invalidStatePausesAndThenResumesAutosave();
    void saveModeAndManualRequirementControlButton();
    void formValidationMapsIssuesAndControlsAutosave();
    void formValidationFocusesTheFirstMappedError();
    void pageHeaderOwnsStandardLabelsAndFonts();
    void scrollableBodyOwnsContentAndLayoutPolicy();
};

void PageComponentsTests::autosaveDebouncesAndRequestsNonInteractiveSave()
{
    AutosaveCoordinator coordinator;
    coordinator.setDebounceInterval(10);
    QSignalSpy saveSpy(&coordinator, &AutosaveCoordinator::saveRequested);
    QSignalSpy dirtySpy(&coordinator, &AutosaveCoordinator::dirtyChanged);

    coordinator.markDirty();

    QCOMPARE(dirtySpy.count(), 1);
    QVERIFY(coordinator.isDirty());
    QTRY_COMPARE_WITH_TIMEOUT(saveSpy.count(), 1, 200);
    QCOMPARE(saveSpy.first().at(0).toBool(), false);

    coordinator.markClean();
    QVERIFY(!coordinator.isDirty());
    QCOMPARE(dirtySpy.count(), 2);
}

void PageComponentsTests::autosaveSuppressesChangesWhileLoading()
{
    AutosaveCoordinator coordinator;
    coordinator.setDebounceInterval(10);
    QSignalSpy saveSpy(&coordinator, &AutosaveCoordinator::saveRequested);

    coordinator.setLoading(true);
    coordinator.markDirty();
    QVERIFY(!coordinator.isDirty());
    QTest::qWait(30);
    QCOMPARE(saveSpy.count(), 0);

    coordinator.setLoading(false);
    coordinator.markDirty();
    QTRY_COMPARE_WITH_TIMEOUT(saveSpy.count(), 1, 200);
}

void PageComponentsTests::invalidStatePausesAndThenResumesAutosave()
{
    AutosaveCoordinator coordinator;
    coordinator.setDebounceInterval(10);
    QSignalSpy saveSpy(&coordinator, &AutosaveCoordinator::saveRequested);

    coordinator.setValid(false);
    coordinator.markDirty();
    QVERIFY(coordinator.isDirty());
    QTest::qWait(30);
    QCOMPARE(saveSpy.count(), 0);

    coordinator.setValid(true);
    QTRY_COMPARE_WITH_TIMEOUT(saveSpy.count(), 1, 200);
}

void PageComponentsTests::saveModeAndManualRequirementControlButton()
{
    AutosaveCoordinator coordinator;
    QPushButton button;
    coordinator.bindSaveButton(&button);
    coordinator.setSaveMode(SaveMode::Manual);
    coordinator.markDirty();

    QVERIFY(!button.isHidden());
    QVERIFY(button.isEnabled());
    QCOMPARE(button.text(), QStringLiteral("Save Changes *"));

    QSignalSpy saveSpy(&coordinator, &AutosaveCoordinator::saveRequested);
    button.click();
    QCOMPARE(saveSpy.count(), 1);
    QCOMPARE(saveSpy.first().at(0).toBool(), true);

    coordinator.setSaveMode(SaveMode::Automatic);
    QVERIFY(button.isHidden());
    coordinator.setManualSaveRequired(true);
    QVERIFY(!button.isHidden());
}

void PageComponentsTests::formValidationMapsIssuesAndControlsAutosave()
{
    QWidget form;
    auto* formLayout = new QVBoxLayout(&form);
    auto* nameEdit = new QLineEdit(&form);
    nameEdit->setAccessibleDescription(QStringLiteral("Teacher name"));
    formLayout->addWidget(nameEdit);

    AutosaveCoordinator autosave;
    autosave.setDebounceInterval(10);
    FormValidationBinder binder(&autosave, nullptr, &form);
    auto* message = binder.createMessageLabel(&form);
    formLayout->addWidget(message);
    binder.registerField(QStringLiteral("name"), nameEdit, message);

    QSignalSpy saveSpy(&autosave, &AutosaveCoordinator::saveRequested);
    binder.setValidation(ValidationResult(ValidationIssue{
        .code = QStringLiteral("teacher.name.required"),
        .field = QStringLiteral("name")
        }));
    autosave.markDirty();

    QVERIFY(binder.hasErrors());
    QVERIFY(!autosave.isValid());
    QCOMPARE(
        nameEdit->property("formValidationState").toString(),
        QStringLiteral("error")
        );
    QCOMPARE(message->text(), QStringLiteral("This field is required."));
    QVERIFY(!message->isHidden());
    QCOMPARE(
        nameEdit->accessibleDescription(),
        QStringLiteral("Teacher name\nError: This field is required.")
        );
    QTest::qWait(30);
    QCOMPARE(saveSpy.count(), 0);

    binder.clear();

    QVERIFY(!binder.hasErrors());
    QVERIFY(autosave.isValid());
    QVERIFY(!nameEdit->property("formValidationState").isValid());
    QCOMPARE(nameEdit->accessibleDescription(), QStringLiteral("Teacher name"));
    QVERIFY(message->isHidden());
    QTRY_COMPARE_WITH_TIMEOUT(saveSpy.count(), 1, 200);
}

void PageComponentsTests::formValidationFocusesTheFirstMappedError()
{
    QWidget window;
    window.resize(240, 120);
    auto* scrollArea = new QScrollArea(&window);
    auto* content = new QWidget(scrollArea);
    auto* contentLayout = new QVBoxLayout(content);
    auto* firstEdit = new QLineEdit(content);
    auto* secondEdit = new QLineEdit(content);
    contentLayout->addWidget(firstEdit);
    contentLayout->addStretch(1);
    contentLayout->addWidget(secondEdit);
    scrollArea->setWidget(content);
    scrollArea->setWidgetResizable(true);

    FormValidationBinder binder(nullptr, scrollArea, &window);
    binder.registerField(QStringLiteral("first"), firstEdit);
    binder.registerField(QStringLiteral("second"), secondEdit);
    binder.setValidation(ValidationResult(ValidationIssues{
        {.code = QStringLiteral("validation.required"),
         .field = QStringLiteral("second")},
        {.code = QStringLiteral("validation.required"),
         .field = QStringLiteral("first")}
        }));

    window.show();
    QTest::qWait(10);

    QVERIFY(binder.focusFirstError());
    QTRY_VERIFY_WITH_TIMEOUT(secondEdit->hasFocus(), 200);
}

void PageComponentsTests::pageHeaderOwnsStandardLabelsAndFonts()
{
    PageHeader header(QStringLiteral("Title"), QStringLiteral("Subtitle"));

    QCOMPARE(header.title(), QStringLiteral("Title"));
    QCOMPARE(header.subtitle(), QStringLiteral("Subtitle"));
    QCOMPARE(
        header.titleLabel()->objectName(),
        QStringLiteral("pageTitle")
        );
    QCOMPARE(
        header.subtitleLabel()->objectName(),
        QStringLiteral("pageSubtitle")
        );
    QCOMPARE(
        header.titleLabel()->font(),
        FontManager::getUiFont(
            UiConstants::Pages::TitleFontSize,
            QFont::Bold
            )
        );
    QCOMPARE(
        header.subtitleLabel()->font(),
        FontManager::getUiFont(UiConstants::Pages::SubtitleFontSize)
        );
    QVERIFY(header.subtitleLabel()->wordWrap());
}

void PageComponentsTests::scrollableBodyOwnsContentAndLayoutPolicy()
{
    ScrollablePageBody standard;
    QVERIFY(standard.widgetResizable());
    QCOMPARE(standard.widget(), standard.contentWidget());
    QCOMPARE(
        standard.contentLayout()->contentsMargins(),
        QMargins(
            UiConstants::Pages::Margin,
            UiConstants::Pages::Margin,
            UiConstants::Pages::Margin,
            UiConstants::Pages::Margin
            )
        );

    ScrollablePageBody compact(nullptr, QMargins(0, 0, 0, 0), 3);
    QCOMPARE(compact.contentLayout()->contentsMargins(), QMargins());
    QCOMPARE(compact.contentLayout()->spacing(), 3);
}

QTEST_MAIN(PageComponentsTests)

#include "page_components_tests.moc"
