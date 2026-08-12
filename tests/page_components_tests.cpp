#include "core/fontmanager.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/pages/scrollable_page_body.h"

#include <QLabel>
#include <QPushButton>
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
