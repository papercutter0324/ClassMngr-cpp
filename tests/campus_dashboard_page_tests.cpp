#include "features/campus/ui/campus_dashboard_page.h"
#include "core/settingsmanager.h"
#include "features/campus/data/campus_json_repository.h"
#include "next/application/campus_dashboard_campus_save_port.h"
#include "next/application/campus_dashboard_selected_campus_read_port.h"
#include "next/platform/campus_dashboard_campus_repository_adapter.h"
#include "next/platform/campus_dashboard_selected_campus_read_adapter.h"
#include "next/platform/settings_manager_last_selected_campus_port.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{

class RecordingCampusReadPort final
    : public ClassMngr::Next::Application::
          CampusDashboardSelectedCampusReadPort
{
public:
    explicit RecordingCampusReadPort(QString campusDirectory)
        : m_directory(std::move(campusDirectory))
        , m_adapter(m_directory)
    {
    }

    QString absentCampusId;
    QString failedCampusId;
    mutable std::vector<std::string> readIds;
    mutable QString alphaNameWhenBetaWasRead;

    [[nodiscard]] ClassMngr::Next::Application::
        CampusDashboardSelectedCampusReadResult loadCampus(
            const ClassMngr::Next::Domain::CampusId& campusId
            ) const override
    {
        readIds.push_back(campusId.value());

        if (campusId.value() == "beta")
        {
            const auto alpha = CampusJsonRepository(m_directory)
                .loadCampus(QStringLiteral("alpha"));
            if (alpha.has_value())
            {
                alphaNameWhenBetaWasRead = alpha->campusName;
            }
        }

        if (QString::fromUtf8(campusId.value()) == absentCampusId)
        {
            return ClassMngr::Next::Application::
                CampusDashboardSelectedCampusReadResult::success(
                    std::nullopt
                    );
        }

        if (QString::fromUtf8(campusId.value()) == failedCampusId)
        {
            return ClassMngr::Next::Application::
                CampusDashboardSelectedCampusReadResult::failure({
                    .code = ClassMngr::Next::Domain::ErrorCode::Technical,
                    .message = "injected selected-campus read failure",
                    .recoverable = false
                });
        }

        return m_adapter.loadCampus(campusId);
    }

private:
    QString m_directory;
    ClassMngr::Next::Platform::CampusDashboardSelectedCampusReadAdapter
        m_adapter;
};

class RecordingCampusSavePort final
    : public ClassMngr::Next::Application::CampusDashboardCampusSavePort
{
public:
    explicit RecordingCampusSavePort(QString campusDirectory)
        : m_directory(std::move(campusDirectory))
    {
    }

    QString failureMessage;
    mutable std::vector<
        ClassMngr::Next::Application::CampusDashboardCampusSnapshot
        > savedSnapshots;

    [[nodiscard]] ClassMngr::Next::Domain::Result<void> saveCampus(
        const ClassMngr::Next::Application::CampusDashboardCampusSnapshot& campus
        ) const override
    {
        savedSnapshots.push_back(campus);
        if (!failureMessage.isEmpty())
        {
            return ClassMngr::Next::Domain::Result<void>::failure({
                .code = ClassMngr::Next::Domain::ErrorCode::Technical,
                .message = failureMessage.toUtf8().toStdString(),
                .recoverable = false
            });
        }

        return ClassMngr::Next::Platform::
            CampusDashboardCampusRepositoryAdapter(m_directory)
                .saveCampus(campus);
    }

private:
    QString m_directory;
};

void saveCampus(
    const QString& directory,
    const QString& id,
    const QString& name,
    const QString& building
    )
{
    CampusInfo campus;
    campus.id = id;
    campus.campusName = name;
    campus.campusCode = id.toUpper();
    campus.buildingName = building;
    campus.printerDriverUrlUnavailable = true;

    const Status saved = CampusJsonRepository(directory).saveCampus(campus);
    if (!saved)
    {
        qFatal("Unable to create Campus Dashboard test data.");
    }
}

QPushButton* buttonWithText(
    QWidget& parent,
    const QString& text
    )
{
    for (QPushButton* button : parent.findChildren<QPushButton*>())
    {
        if (button->text() == text)
        {
            return button;
        }
    }
    return nullptr;
}

QLabel* labelWithText(
    QWidget& parent,
    const QString& text
    )
{
    for (QLabel* label : parent.findChildren<QLabel*>())
    {
        if (label->text() == text)
        {
            return label;
        }
    }
    return nullptr;
}

QLineEdit* campusCodeEdit(CampusDashboardPage& page)
{
    const QList<QLineEdit*> directEdits = page.findChildren<QLineEdit*>(
        QString(),
        Qt::FindDirectChildrenOnly
        );
    for (QLineEdit* edit : directEdits)
    {
        if (edit->objectName().isEmpty())
        {
            return edit;
        }
    }
    return nullptr;
}

bool markCampusFieldsEdited(
    CampusDashboardPage& page,
    const QString& campusName,
    const QString& campusCode
    )
{
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("campusNameEdit")
        );
    auto* codeEdit = campusCodeEdit(page);
    if (!nameEdit || !codeEdit)
    {
        return false;
    }

    nameEdit->setText(campusName);
    codeEdit->setText(campusCode);
    return QMetaObject::invokeMethod(
        &page,
        "handleFieldEdited",
        Qt::DirectConnection
        );
}

class CampusDashboardPageFixture final
{
public:
    QTemporaryDir campusDirectory;
    RecordingCampusReadPort readPort{campusDirectory.path()};
    RecordingCampusSavePort savePort{campusDirectory.path()};
    std::unique_ptr<CampusDashboardPage> page;

    [[nodiscard]] bool initialize(
        SaveMode saveMode = SaveMode::Manual
        )
    {
        if (!campusDirectory.isValid())
        {
            return false;
        }

        saveCampus(
            campusDirectory.path(),
            QStringLiteral("alpha"),
            QStringLiteral("Alpha"),
            QStringLiteral("Alpha Hall")
            );
        saveCampus(
            campusDirectory.path(),
            QStringLiteral("beta"),
            QStringLiteral("Beta"),
            QStringLiteral("Beta Hall")
            );

        ClassMngr::Next::Platform::SettingsManagerLastSelectedCampusPort()
            .write(std::nullopt);
        page = std::make_unique<CampusDashboardPage>(
            true,
            CampusDashboardPageDependencies{
                .campusDirectory = campusDirectory.path(),
                .selectedCampusReadPort = &readPort,
                .campusSavePort = &savePort
            }
            );
        page->setSaveMode(saveMode);
        page->refresh();
        return true;
    }
};

}

class CampusDashboardPageTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void unavailableCheckboxesAreAdminOnly_data();
    void unavailableCheckboxesAreAdminOnly();
    void campusSelectorPreservesDirectoryOrderAndLabels_data();
    void campusSelectorPreservesDirectoryOrderAndLabels();
    void storedCampusSelectionIsUsedAsFallback();
    void campusSelectionIsPersistedThroughTypedPort();
    void selectedCampusDetailUsesQueryAndSavesDirtyRecordFirst();
    void manualSaveWaitsForActionAndPersistsNormalizedFields();
    void automaticSaveUsesDebounceAndPersistsOnce();
    void manualSaveFailureRetainsDirtyForm();
    void automaticSaveFailureRetainsDirtyForm();
    void failedSaveRestoresSelectionAndSkipsRequestedRead();
    void failedSavePreventsNewCampusDraft();
    void missingSelectedCampusDetailLeavesVisibleAndStoredStateUntouched();
    void failedSelectedCampusDetailLeavesVisibleAndStoredStateUntouched();

private:
    QTemporaryDir m_settingsDirectory;
};

void CampusDashboardPageTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QVERIFY(
        qputenv(
            "CLASSMNGR_SETTINGS_ROOT",
            m_settingsDirectory.path().toUtf8()
            )
        );

    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void CampusDashboardPageTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void CampusDashboardPageTests::unavailableCheckboxesAreAdminOnly_data()
{
    QTest::addColumn<bool>("adminMode");

    QTest::newRow("standard") << false;
    QTest::newRow("admin") << true;
}

void CampusDashboardPageTests::unavailableCheckboxesAreAdminOnly()
{
    QFETCH(bool, adminMode);

    CampusDashboardPage page(adminMode);

    const QStringList objectNames{
        QStringLiteral("printerDriverUrlUnavailableCheck"),
        QStringLiteral("photocopierCodeUnavailableCheck")
    };

    for (const QString& objectName : objectNames)
    {
        auto* checkBox =
            page.findChild<QCheckBox*>(objectName);

        QVERIFY2(checkBox, qPrintable(objectName));
        QCOMPARE(checkBox->isHidden(), !adminMode);
        QCOMPARE(checkBox->isEnabled(), adminMode);
    }
}

void CampusDashboardPageTests::
campusSelectorPreservesDirectoryOrderAndLabels_data()
{
    QTest::addColumn<bool>("adminMode");

    QTest::newRow("standard") << false;
    QTest::newRow("admin") << true;
}

void CampusDashboardPageTests::
campusSelectorPreservesDirectoryOrderAndLabels()
{
    using ClassMngr::Next::Platform::
        SettingsManagerLastSelectedCampusPort;

    QFETCH(bool, adminMode);

    SettingsManagerLastSelectedCampusPort().write(std::nullopt);

    CampusDashboardPage page(adminMode);
    page.refresh();

    auto* combo = page.findChild<QComboBox*>();
    QVERIFY(combo);

    const QStringList expectedIds{
        QStringLiteral("bundang"),
        QStringLiteral("daechi"),
        QStringLiteral("dongtan"),
        QStringLiteral("dongtan_2"),
        QStringLiteral("hanam"),
        QStringLiteral("ilsan"),
        QStringLiteral("j"),
        QStringLiteral("juk"),
        QStringLiteral("jung"),
        QStringLiteral("pyeongchon"),
        QStringLiteral("songpa"),
        QStringLiteral("suji"),
        QStringLiteral("yongtong")
    };
    const QStringList expectedNames{
        QStringLiteral("Bundang"),
        QStringLiteral("Daechi"),
        QStringLiteral("Dongtan 1"),
        QStringLiteral("Dongtan 2"),
        QStringLiteral("Hanam"),
        QStringLiteral("Ilsan"),
        QStringLiteral("Jeongja"),
        QStringLiteral("Jukjeon"),
        QStringLiteral("Junggye"),
        QStringLiteral("Pyeongchon"),
        QStringLiteral("Songpa"),
        QStringLiteral("Suji"),
        QStringLiteral("Yeongtong")
    };
    const QStringList expectedCodes{
        QStringLiteral("BDG"),
        QStringLiteral("DAE"),
        QStringLiteral("DGT1"),
        QStringLiteral("DGT2"),
        QStringLiteral("HNM"),
        QStringLiteral("ILS"),
        QStringLiteral("JJA"),
        QStringLiteral("JJN"),
        QStringLiteral("JGY"),
        QStringLiteral("PYC"),
        QStringLiteral("SPA"),
        QStringLiteral("SUJ"),
        QStringLiteral("YTG")
    };

    QCOMPARE(combo->count(), expectedIds.size());
    for (int index = 0; index < expectedIds.size(); ++index)
    {
        const QString expectedLabel =
            adminMode
                ? expectedNames.at(index)
                : QStringLiteral("%1 (%2)")
                    .arg(expectedNames.at(index), expectedCodes.at(index));

        QCOMPARE(combo->itemData(index).toString(), expectedIds.at(index));
        QCOMPARE(combo->itemText(index), expectedLabel);
    }

    QCOMPARE(combo->currentData().toString(), QStringLiteral("bundang"));

    combo->setCurrentIndex(combo->count() - 1);
    QCOMPARE(combo->currentData().toString(), QStringLiteral("yongtong"));

    page.refresh();
    QCOMPARE(combo->currentData().toString(), QStringLiteral("yongtong"));
}

void CampusDashboardPageTests::storedCampusSelectionIsUsedAsFallback()
{
    using ClassMngr::Next::Domain::CampusId;
    using ClassMngr::Next::Platform::
        SettingsManagerLastSelectedCampusPort;

    SettingsManagerLastSelectedCampusPort port;
    port.write(CampusId::fromString("j"));

    CampusDashboardPage page(
        false,
        CampusDashboardPageDependencies{}
        );
    page.refresh();

    auto* combo = page.findChild<QComboBox*>();
    QVERIFY(combo);
    QCOMPARE(combo->currentData().toString(), QStringLiteral("j"));
    QCOMPARE(combo->currentText(), QStringLiteral("Jeongja (JJA)"));
}

void CampusDashboardPageTests::campusSelectionIsPersistedThroughTypedPort()
{
    using ClassMngr::Next::Platform::
        SettingsManagerLastSelectedCampusPort;

    SettingsManagerLastSelectedCampusPort port;
    port.write(std::nullopt);

    CampusDashboardPage page(false);
    page.refresh();

    auto* combo = page.findChild<QComboBox*>();
    QVERIFY(combo);
    QVERIFY(combo->count() > 1);

    combo->setCurrentIndex(combo->count() - 1);
    QCoreApplication::processEvents();

    const auto storedCampusId = port.read();
    QVERIFY(storedCampusId.has_value());
    QCOMPARE(
        QString::fromStdString(storedCampusId->value()),
        combo->currentData().toString()
        );
}

void CampusDashboardPageTests::
selectedCampusDetailUsesQueryAndSavesDirtyRecordFirst()
{
    using ClassMngr::Next::Platform::SettingsManagerLastSelectedCampusPort;

    QTemporaryDir campusDirectory;
    QVERIFY(campusDirectory.isValid());
    saveCampus(
        campusDirectory.path(),
        QStringLiteral("alpha"),
        QStringLiteral("Alpha"),
        QStringLiteral("Alpha Hall")
        );
    saveCampus(
        campusDirectory.path(),
        QStringLiteral("beta"),
        QStringLiteral("Beta"),
        QStringLiteral("Beta Hall")
        );

    SettingsManagerLastSelectedCampusPort().write(std::nullopt);
    RecordingCampusReadPort readPort(campusDirectory.path());
    CampusDashboardPage page(
        true,
        CampusDashboardPageDependencies{
            .campusDirectory = campusDirectory.path(),
            .selectedCampusReadPort = &readPort
        }
        );
    page.setSaveMode(SaveMode::Manual);
    page.refresh();

    auto* selector = page.findChild<QComboBox*>();
    auto* campusName = page.findChild<QLineEdit*>(QStringLiteral("campusNameEdit"));
    auto* buildingName = page.findChild<QLineEdit*>(
        QStringLiteral("campusBuildingNameEdit")
        );
    QVERIFY(selector);
    QVERIFY(campusName);
    QVERIFY(buildingName);
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(campusName->text(), QStringLiteral("Alpha"));
    QCOMPARE(buildingName->text(), QStringLiteral("Alpha Hall"));

    campusName->setText(QStringLiteral("Alpha Revised"));
    QVERIFY(QMetaObject::invokeMethod(
        &page,
        "handleFieldEdited",
        Qt::DirectConnection
        ));
    QVERIFY(page.hasUnsavedChanges());

    selector->setCurrentIndex(selector->findData(QStringLiteral("beta")));
    QCoreApplication::processEvents();

    QCOMPARE(readPort.readIds.size(), std::size_t(2));
    QCOMPARE(readPort.readIds.at(0), std::string("alpha"));
    QCOMPARE(readPort.readIds.at(1), std::string("beta"));
    QCOMPARE(
        readPort.alphaNameWhenBetaWasRead,
        QStringLiteral("Alpha Revised")
        );
    QCOMPARE(selector->currentData().toString(), QStringLiteral("beta"));
    QCOMPARE(campusName->text(), QStringLiteral("Beta"));
    QCOMPARE(buildingName->text(), QStringLiteral("Beta Hall"));

    const auto storedCampus = SettingsManagerLastSelectedCampusPort().read();
    QVERIFY(storedCampus.has_value());
    QCOMPARE(storedCampus->value(), std::string("beta"));

    const auto savedAlpha = CampusJsonRepository(campusDirectory.path())
        .loadCampus(QStringLiteral("alpha"));
    QVERIFY(savedAlpha.has_value());
    QCOMPARE(savedAlpha->campusName, QStringLiteral("Alpha Revised"));

}

void CampusDashboardPageTests::
manualSaveWaitsForActionAndPersistsNormalizedFields()
{
    CampusDashboardPageFixture fixture;
    QVERIFY(fixture.initialize());
    CampusDashboardPage& page = *fixture.page;

    auto* selector = page.findChild<QComboBox*>();
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("campusNameEdit")
        );
    QVERIFY(selector);
    QVERIFY(nameEdit);
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));

    const auto beforeSave = CampusJsonRepository(fixture.campusDirectory.path())
        .loadCampus(QStringLiteral("alpha"));
    QVERIFY(beforeSave.has_value());
    QCOMPARE(beforeSave->campusName, QStringLiteral("Alpha"));
    QCOMPARE(beforeSave->campusCode, QStringLiteral("ALPHA"));

    QVERIFY(markCampusFieldsEdited(
        page,
        QStringLiteral("Alpha Revised"),
        QStringLiteral("  AL2  ")
        ));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(fixture.savePort.savedSnapshots.empty());
    QVERIFY(labelWithText(page, QStringLiteral("Unsaved changes")));

    const auto stillUnchanged = CampusJsonRepository(
        fixture.campusDirectory.path()
        ).loadCampus(QStringLiteral("alpha"));
    QVERIFY(stillUnchanged.has_value());
    QCOMPARE(stillUnchanged->campusName, QStringLiteral("Alpha"));
    QCOMPARE(stillUnchanged->campusCode, QStringLiteral("ALPHA"));

    QPushButton* saveButton = buttonWithText(
        page,
        QStringLiteral("Save Campus")
        );
    QVERIFY(saveButton);
    QVERIFY(saveButton->isEnabled());
    saveButton->click();

    QCOMPARE(fixture.savePort.savedSnapshots.size(), std::size_t(1));
    const auto& savedSnapshot = fixture.savePort.savedSnapshots.front();
    QCOMPARE(savedSnapshot.id.value(), std::string("alpha"));
    QCOMPARE(savedSnapshot.campusName, std::string("Alpha Revised"));
    QCOMPARE(savedSnapshot.campusCode, std::string("AL2"));

    const auto savedCampus = CampusJsonRepository(
        fixture.campusDirectory.path()
        ).loadCampus(QStringLiteral("alpha"));
    QVERIFY(savedCampus.has_value());
    QCOMPARE(savedCampus->campusName, QStringLiteral("Alpha Revised"));
    QCOMPARE(savedCampus->campusCode, QStringLiteral("AL2"));
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(selector->currentText(), QStringLiteral("Alpha Revised"));
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(labelWithText(page, QStringLiteral("Saved")));
}

void CampusDashboardPageTests::
automaticSaveUsesDebounceAndPersistsOnce()
{
    CampusDashboardPageFixture fixture;
    QVERIFY(fixture.initialize(SaveMode::Automatic));
    CampusDashboardPage& page = *fixture.page;

    auto* selector = page.findChild<QComboBox*>();
    QVERIFY(selector);
    QVERIFY(markCampusFieldsEdited(
        page,
        QStringLiteral("Alpha Autosaved"),
        QStringLiteral("  AUTO  ")
        ));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(fixture.savePort.savedSnapshots.empty());

    QTest::qWait(500);
    QVERIFY(fixture.savePort.savedSnapshots.empty());

    QVERIFY(markCampusFieldsEdited(
        page,
        QStringLiteral("Alpha Autosaved"),
        QStringLiteral("  AUTO  ")
        ));
    QTest::qWait(500);
    QVERIFY(fixture.savePort.savedSnapshots.empty());

    QTRY_COMPARE_WITH_TIMEOUT(
        fixture.savePort.savedSnapshots.size(),
        std::size_t(1),
        1500
        );
    QTest::qWait(900);
    QCOMPARE(fixture.savePort.savedSnapshots.size(), std::size_t(1));
    QCOMPARE(
        fixture.savePort.savedSnapshots.front().campusCode,
        std::string("AUTO")
        );

    const auto savedCampus = CampusJsonRepository(
        fixture.campusDirectory.path()
        ).loadCampus(QStringLiteral("alpha"));
    QVERIFY(savedCampus.has_value());
    QCOMPARE(savedCampus->campusName, QStringLiteral("Alpha Autosaved"));
    QCOMPARE(savedCampus->campusCode, QStringLiteral("AUTO"));
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(selector->currentText(), QStringLiteral("Alpha Autosaved"));
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(labelWithText(page, QStringLiteral("Saved")));
}

void CampusDashboardPageTests::manualSaveFailureRetainsDirtyForm()
{
    CampusDashboardPageFixture fixture;
    QVERIFY(fixture.initialize());
    CampusDashboardPage& page = *fixture.page;
    const QString failureMessage =
        QStringLiteral("injected Campus Dashboard save failure");
    fixture.savePort.failureMessage = failureMessage;

    auto* selector = page.findChild<QComboBox*>();
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("campusNameEdit")
        );
    QVERIFY(selector);
    QVERIFY(nameEdit);
    QVERIFY(markCampusFieldsEdited(
        page,
        QStringLiteral("Alpha Draft"),
        QStringLiteral("ALPHA")
        ));

    QPushButton* saveButton = buttonWithText(
        page,
        QStringLiteral("Save Campus")
        );
    QVERIFY(saveButton);
    saveButton->click();

    QCOMPARE(fixture.savePort.savedSnapshots.size(), std::size_t(1));
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(nameEdit->text(), QStringLiteral("Alpha Draft"));
    QVERIFY(labelWithText(page, failureMessage));
    const auto persisted = CampusJsonRepository(fixture.campusDirectory.path())
        .loadCampus(QStringLiteral("alpha"));
    QVERIFY(persisted.has_value());
    QCOMPARE(persisted->campusName, QStringLiteral("Alpha"));
}

void CampusDashboardPageTests::automaticSaveFailureRetainsDirtyForm()
{
    CampusDashboardPageFixture fixture;
    QVERIFY(fixture.initialize(SaveMode::Automatic));
    CampusDashboardPage& page = *fixture.page;
    const QString failureMessage =
        QStringLiteral("injected Campus Dashboard save failure");
    fixture.savePort.failureMessage = failureMessage;

    auto* selector = page.findChild<QComboBox*>();
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("campusNameEdit")
        );
    QVERIFY(selector);
    QVERIFY(nameEdit);
    QVERIFY(markCampusFieldsEdited(
        page,
        QStringLiteral("Alpha Draft"),
        QStringLiteral("ALPHA")
        ));

    QTRY_COMPARE_WITH_TIMEOUT(
        fixture.savePort.savedSnapshots.size(),
        std::size_t(1),
        2000
        );

    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(nameEdit->text(), QStringLiteral("Alpha Draft"));
    QVERIFY(labelWithText(page, failureMessage));
    const auto persisted = CampusJsonRepository(fixture.campusDirectory.path())
        .loadCampus(QStringLiteral("alpha"));
    QVERIFY(persisted.has_value());
    QCOMPARE(persisted->campusName, QStringLiteral("Alpha"));
}

void CampusDashboardPageTests::
failedSaveRestoresSelectionAndSkipsRequestedRead()
{
    CampusDashboardPageFixture fixture;
    QVERIFY(fixture.initialize());
    CampusDashboardPage& page = *fixture.page;
    const QString failureMessage =
        QStringLiteral("injected Campus Dashboard save failure");
    fixture.savePort.failureMessage = failureMessage;

    auto* selector = page.findChild<QComboBox*>();
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("campusNameEdit")
        );
    QVERIFY(selector);
    QVERIFY(nameEdit);
    QCOMPARE(fixture.readPort.readIds.size(), std::size_t(1));
    QCOMPARE(fixture.readPort.readIds.front(), std::string("alpha"));

    QVERIFY(markCampusFieldsEdited(
        page,
        QStringLiteral("Alpha Draft"),
        QStringLiteral("ALPHA")
        ));
    selector->setCurrentIndex(selector->findData(QStringLiteral("beta")));
    QCoreApplication::processEvents();

    QCOMPARE(fixture.savePort.savedSnapshots.size(), std::size_t(1));
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(nameEdit->text(), QStringLiteral("Alpha Draft"));
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(fixture.readPort.readIds.size(), std::size_t(1));
    QVERIFY(labelWithText(page, failureMessage));
    const auto storedCampus =
        ClassMngr::Next::Platform::SettingsManagerLastSelectedCampusPort()
            .read();
    QVERIFY(storedCampus.has_value());
    QCOMPARE(storedCampus->value(), std::string("alpha"));
}

void CampusDashboardPageTests::failedSavePreventsNewCampusDraft()
{
    CampusDashboardPageFixture fixture;
    QVERIFY(fixture.initialize());
    CampusDashboardPage& page = *fixture.page;
    const QString failureMessage =
        QStringLiteral("injected Campus Dashboard save failure");
    fixture.savePort.failureMessage = failureMessage;

    auto* selector = page.findChild<QComboBox*>();
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("campusNameEdit")
        );
    QVERIFY(selector);
    QVERIFY(nameEdit);
    const int originalCampusCount = selector->count();
    QVERIFY(markCampusFieldsEdited(
        page,
        QStringLiteral("Alpha Draft"),
        QStringLiteral("ALPHA")
        ));

    QPushButton* newCampusButton = buttonWithText(
        page,
        QStringLiteral("New Campus")
        );
    QVERIFY(newCampusButton);
    newCampusButton->click();

    QCOMPARE(fixture.savePort.savedSnapshots.size(), std::size_t(1));
    QCOMPARE(selector->count(), originalCampusCount);
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(nameEdit->text(), QStringLiteral("Alpha Draft"));
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(fixture.readPort.readIds.size(), std::size_t(1));
    QVERIFY(labelWithText(page, failureMessage));
}

void CampusDashboardPageTests::
missingSelectedCampusDetailLeavesVisibleAndStoredStateUntouched()
{
    using ClassMngr::Next::Platform::SettingsManagerLastSelectedCampusPort;

    QTemporaryDir campusDirectory;
    QVERIFY(campusDirectory.isValid());
    saveCampus(
        campusDirectory.path(),
        QStringLiteral("alpha"),
        QStringLiteral("Alpha"),
        QStringLiteral("Alpha Hall")
        );
    saveCampus(
        campusDirectory.path(),
        QStringLiteral("beta"),
        QStringLiteral("Beta"),
        QStringLiteral("Beta Hall")
        );

    SettingsManagerLastSelectedCampusPort().write(std::nullopt);
    RecordingCampusReadPort readPort(campusDirectory.path());
    readPort.absentCampusId = QStringLiteral("beta");
    CampusDashboardPage page(
        false,
        CampusDashboardPageDependencies{
            .campusDirectory = campusDirectory.path(),
            .selectedCampusReadPort = &readPort
        }
        );
    page.refresh();

    auto* selector = page.findChild<QComboBox*>();
    auto* buildingName = page.findChild<QLineEdit*>(
        QStringLiteral("campusBuildingNameEdit")
        );
    QVERIFY(selector);
    QVERIFY(buildingName);
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(buildingName->text(), QStringLiteral("Alpha Hall"));

    selector->setCurrentIndex(selector->findData(QStringLiteral("beta")));
    QCoreApplication::processEvents();

    QCOMPARE(selector->currentData().toString(), QStringLiteral("beta"));
    QCOMPARE(buildingName->text(), QStringLiteral("Alpha Hall"));
    QCOMPARE(readPort.readIds.size(), std::size_t(2));
    QCOMPARE(readPort.readIds.at(1), std::string("beta"));
    const auto storedCampus = SettingsManagerLastSelectedCampusPort().read();
    QVERIFY(storedCampus.has_value());
    QCOMPARE(storedCampus->value(), std::string("alpha"));
}

void CampusDashboardPageTests::
failedSelectedCampusDetailLeavesVisibleAndStoredStateUntouched()
{
    using ClassMngr::Next::Platform::SettingsManagerLastSelectedCampusPort;

    QTemporaryDir campusDirectory;
    QVERIFY(campusDirectory.isValid());
    saveCampus(
        campusDirectory.path(),
        QStringLiteral("alpha"),
        QStringLiteral("Alpha"),
        QStringLiteral("Alpha Hall")
        );
    saveCampus(
        campusDirectory.path(),
        QStringLiteral("beta"),
        QStringLiteral("Beta"),
        QStringLiteral("Beta Hall")
        );

    SettingsManagerLastSelectedCampusPort().write(std::nullopt);
    RecordingCampusReadPort readPort(campusDirectory.path());
    readPort.failedCampusId = QStringLiteral("beta");
    CampusDashboardPage page(
        false,
        CampusDashboardPageDependencies{
            .campusDirectory = campusDirectory.path(),
            .selectedCampusReadPort = &readPort
        }
        );
    page.refresh();

    auto* selector = page.findChild<QComboBox*>();
    auto* buildingName = page.findChild<QLineEdit*>(
        QStringLiteral("campusBuildingNameEdit")
        );
    QVERIFY(selector);
    QVERIFY(buildingName);
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(buildingName->text(), QStringLiteral("Alpha Hall"));

    selector->setCurrentIndex(selector->findData(QStringLiteral("beta")));
    QCoreApplication::processEvents();

    QCOMPARE(selector->currentData().toString(), QStringLiteral("beta"));
    QCOMPARE(buildingName->text(), QStringLiteral("Alpha Hall"));
    QCOMPARE(readPort.readIds.size(), std::size_t(2));
    QCOMPARE(readPort.readIds.at(1), std::string("beta"));
    const auto storedCampus = SettingsManagerLastSelectedCampusPort().read();
    QVERIFY(storedCampus.has_value());
    QCOMPARE(storedCampus->value(), std::string("alpha"));
}

QTEST_MAIN(CampusDashboardPageTests)

#include "campus_dashboard_page_tests.moc"
