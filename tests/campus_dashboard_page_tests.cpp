#include "features/campus/ui/campus_dashboard_page.h"
#include "core/settingsmanager.h"
#include "features/campus/data/campus_json_repository.h"
#include "next/application/campus_dashboard_campus_save_port.h"
#include "next/application/campus_dashboard_selected_campus_read_port.h"
#include "next/application/last_selected_campus_port.h"
#include "next/platform/campus_dashboard_campus_repository_adapter.h"
#include "next/platform/campus_dashboard_selected_campus_read_adapter.h"
#include "next/platform/settings_manager_last_selected_campus_port.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QTest>
#include <QTranslator>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{

std::optional<CampusInfo> loadCampusOrFail(
    const QString& directory,
    const QString& campusId
    );

class KoreanDefaultTranslator final : public QTranslator
{
public:
    [[nodiscard]] QString translate(
        const char* context,
        const char* sourceText,
        const char* disambiguation = nullptr,
        const int n = -1
        ) const override
    {
        Q_UNUSED(context);
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);
        if (
            sourceText
            && QString::fromUtf8(sourceText) == QStringLiteral("Default")
            )
        {
            return QStringLiteral("기본값");
        }
        return {};
    }
};

class ScopedTranslatorInstallation final
{
public:
    explicit ScopedTranslatorInstallation(QTranslator& translator)
        : m_translator(&translator)
        , m_installed(QCoreApplication::installTranslator(m_translator))
    {
    }

    ~ScopedTranslatorInstallation()
    {
        if (m_installed)
        {
            QCoreApplication::removeTranslator(m_translator);
        }
    }

    [[nodiscard]] bool installed() const
    {
        return m_installed;
    }

private:
    QTranslator* m_translator;
    bool m_installed;
};

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
            const auto alpha = loadCampusOrFail(
                m_directory,
                QStringLiteral("alpha")
                );
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
    QString createConflictMessage;
    mutable std::optional<ClassMngr::Next::Domain::ErrorCode> lastErrorCode;
    mutable std::vector<
        ClassMngr::Next::Application::CampusDashboardCampusSnapshot
        > savedSnapshots;
    mutable std::vector<
        ClassMngr::Next::Application::CampusDashboardCampusSaveIntent
        > saveIntents;
    mutable std::vector<
        ClassMngr::Next::Application::CampusDashboardCampusNameOrigin
        > savedNameOrigins;

    [[nodiscard]] ClassMngr::Next::Domain::Result<void> saveCampus(
        const ClassMngr::Next::Application::CampusDashboardCampusSnapshot& campus,
        ClassMngr::Next::Application::CampusDashboardCampusSaveIntent intent,
        ClassMngr::Next::Application::CampusDashboardCampusNameOrigin
            nameOrigin = ClassMngr::Next::Application::
                CampusDashboardCampusNameOrigin::UserEntered
        ) const override
    {
        savedSnapshots.push_back(campus);
        saveIntents.push_back(intent);
        savedNameOrigins.push_back(nameOrigin);
        if (
            intent == ClassMngr::Next::Application::
                          CampusDashboardCampusSaveIntent::Create
            && !createConflictMessage.isEmpty()
            )
        {
            return ClassMngr::Next::Domain::Result<void>::failure({
                .code = ClassMngr::Next::Domain::ErrorCode::Conflict,
                .message = createConflictMessage.toUtf8().toStdString(),
                .recoverable = false
            });
        }

        if (!failureMessage.isEmpty())
        {
            return ClassMngr::Next::Domain::Result<void>::failure({
                .code = ClassMngr::Next::Domain::ErrorCode::Technical,
                .message = failureMessage.toUtf8().toStdString(),
                .recoverable = false
            });
        }

        const auto result = ClassMngr::Next::Platform::
            CampusDashboardCampusRepositoryAdapter(m_directory)
                .saveCampus(campus, intent, nameOrigin);
        if (result)
        {
            lastErrorCode.reset();
        }
        else
        {
            lastErrorCode = result.error().code;
        }
        return result;
    }

private:
    QString m_directory;
};

class RecordingLastSelectedCampusPort final
    : public ClassMngr::Next::Application::LastSelectedCampusPort
{
public:
    mutable bool failNextWrite = false;
    mutable std::optional<ClassMngr::Next::Domain::CampusId> storedCampusId;
    mutable std::vector<
        std::optional<ClassMngr::Next::Domain::CampusId>
        > writeAttempts;

    [[nodiscard]] std::optional<ClassMngr::Next::Domain::CampusId> read()
        const override
    {
        return storedCampusId;
    }

    [[nodiscard]] ClassMngr::Next::Domain::Result<void> write(
        const std::optional<ClassMngr::Next::Domain::CampusId>& campusId
        ) const override
    {
        writeAttempts.push_back(campusId);
        if (failNextWrite)
        {
            failNextWrite = false;
            return ClassMngr::Next::Domain::Result<void>::failure({
                .code = ClassMngr::Next::Domain::ErrorCode::Technical,
                .message = "injected last-selected campus write failure",
                .recoverable = true
            });
        }

        storedCampusId = campusId;
        return ClassMngr::Next::Domain::Result<void>::success();
    }
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

    const RepositoryStatus saved =
        CampusJsonRepository(directory).saveCampus(campus);
    if (!saved)
    {
        qFatal("Unable to create Campus Dashboard test data.");
    }
}

std::optional<CampusInfo> loadCampusOrFail(
    const QString& directory,
    const QString& campusId
    )
{
    const RepositoryResult<std::optional<CampusInfo>> loaded =
        CampusJsonRepository(directory).loadCampus(campusId);
    if (!loaded)
    {
        qFatal("Unable to read Campus Dashboard test data.");
    }
    return loaded.value();
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

QLabel* labelContainingText(
    QWidget& parent,
    const QString& text
    )
{
    for (QLabel* label : parent.findChildren<QLabel*>())
    {
        if (label->text().contains(text))
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

        const auto clearLastSelectedCampus =
            ClassMngr::Next::Platform::
                SettingsManagerLastSelectedCampusPort().write(std::nullopt);
        if (!clearLastSelectedCampus)
        {
            return false;
        }
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
    void campusPreferenceWriteFailureKeepsLoadedCampusAndCanRetry();
    void selectedCampusDetailUsesQueryAndSavesDirtyRecordFirst();
    void selectedCampusReadPreservesUtf8IdBytes();
    void manualSaveWaitsForActionAndPersistsNormalizedFields();
    void automaticSaveUsesDebounceAndPersistsOnce();
    void manualSaveFailureRetainsDirtyForm();
    void automaticSaveFailureRetainsDirtyForm();
    void missingUpdateTargetKeepsDraftDirtyAndShowsError();
    void localizedDefaultFallbackIsRejectedAfterEditingFinished_data();
    void localizedDefaultFallbackIsRejectedAfterEditingFinished();
    void userEnteredLocalizedDefaultRemainsOrdinaryAfterFallback();
    void reservedCreateIdentityFailureRetainsDirtyForm();
    void reservedUpdateNameFailureRetainsDirtyForm();
    void failedSaveRestoresSelectionAndSkipsRequestedRead();
    void failedSavePreventsNewCampusDraft();
    void successfulNewCampusSaveRetainsIdForUpdates();
    void cleanRefreshWithNoCampusesClearsCurrentCampus();
    void cleanRefreshWithMissingSelectedCampusFallsBackToRemainingCampus();
    void dirtyRefreshWithNoCampusesPreservesDraftAndPendingSave();
    void dirtyRefreshWithPresentSelectedCampusSavesDraft();
    void dirtyRefreshWithMissingSelectedCampusPreservesDraftUntilDiscarded();
    void saveIntentTracksDraftAndCreateConflictRetainsIt();
    void missingSelectedCampusDetailLeavesVisibleAndStoredStateUntouched();
    void failedSelectedCampusDetailLeavesVisibleAndStoredStateUntouched();
    void adminSelectedCampusDetailFailureRestoresLoadedCampus_data();
    void adminSelectedCampusDetailFailureRestoresLoadedCampus();

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

    const auto clearLastSelectedCampus =
        SettingsManagerLastSelectedCampusPort().write(std::nullopt);
    QVERIFY(clearLastSelectedCampus);

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
    const auto setLastSelectedCampus = port.write(CampusId::fromString("j"));
    QVERIFY(setLastSelectedCampus);

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
    const auto clearLastSelectedCampus = port.write(std::nullopt);
    QVERIFY(clearLastSelectedCampus);

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
campusPreferenceWriteFailureKeepsLoadedCampusAndCanRetry()
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

    const auto clearPersistedCampus =
        SettingsManagerLastSelectedCampusPort().write(std::nullopt);
    QVERIFY(clearPersistedCampus);

    RecordingCampusReadPort readPort(campusDirectory.path());
    RecordingCampusSavePort savePort(campusDirectory.path());
    RecordingLastSelectedCampusPort lastSelectedCampusPort;
    CampusDashboardPage page(
        true,
        CampusDashboardPageDependencies{
            .campusDirectory = campusDirectory.path(),
            .selectedCampusReadPort = &readPort,
            .campusSavePort = &savePort,
            .lastSelectedCampusPort = &lastSelectedCampusPort
        }
        );
    page.refresh();

    auto* selector = page.findChild<QComboBox*>();
    auto* campusName = page.findChild<QLineEdit*>(
        QStringLiteral("campusNameEdit")
        );
    auto* buildingName = page.findChild<QLineEdit*>(
        QStringLiteral("campusBuildingNameEdit")
        );
    QVERIFY(selector);
    QVERIFY(campusName);
    QVERIFY(buildingName);
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(campusName->text(), QStringLiteral("Alpha"));
    QCOMPARE(buildingName->text(), QStringLiteral("Alpha Hall"));
    QCOMPARE(lastSelectedCampusPort.writeAttempts.size(), std::size_t(1));
    QVERIFY(lastSelectedCampusPort.storedCampusId.has_value());
    QCOMPARE(
        lastSelectedCampusPort.storedCampusId->value(),
        std::string("alpha")
        );

    lastSelectedCampusPort.failNextWrite = true;
    selector->setCurrentIndex(selector->findData(QStringLiteral("beta")));
    QCoreApplication::processEvents();

    const QString diagnostic =
        QStringLiteral("The selected campus could not be saved as your last campus.");
    QLabel* const statusLabel = labelWithText(page, diagnostic);
    QVERIFY(statusLabel);
    int diagnosticLabelCount = 0;
    for (QLabel* label : page.findChildren<QLabel*>())
    {
        if (label->text() == diagnostic)
        {
            ++diagnosticLabelCount;
        }
    }
    QCOMPARE(diagnosticLabelCount, 1);
    QCOMPARE(selector->currentData().toString(), QStringLiteral("beta"));
    QCOMPARE(campusName->text(), QStringLiteral("Beta"));
    QCOMPARE(buildingName->text(), QStringLiteral("Beta Hall"));
    QCOMPARE(readPort.readIds.size(), std::size_t(2));
    QCOMPARE(readPort.readIds.at(1), std::string("beta"));
    QCOMPARE(lastSelectedCampusPort.writeAttempts.size(), std::size_t(2));
    QVERIFY(lastSelectedCampusPort.storedCampusId.has_value());
    QCOMPARE(
        lastSelectedCampusPort.storedCampusId->value(),
        std::string("alpha")
        );

    selector->setCurrentIndex(selector->findData(QStringLiteral("alpha")));
    QCoreApplication::processEvents();
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QVERIFY(statusLabel->text().isEmpty());
    QVERIFY(lastSelectedCampusPort.storedCampusId.has_value());
    QCOMPARE(
        lastSelectedCampusPort.storedCampusId->value(),
        std::string("alpha")
        );

    selector->setCurrentIndex(selector->findData(QStringLiteral("beta")));
    QCoreApplication::processEvents();
    QCOMPARE(selector->currentData().toString(), QStringLiteral("beta"));
    QCOMPARE(campusName->text(), QStringLiteral("Beta"));
    QCOMPARE(buildingName->text(), QStringLiteral("Beta Hall"));
    QCOMPARE(lastSelectedCampusPort.writeAttempts.size(), std::size_t(4));
    QVERIFY(lastSelectedCampusPort.storedCampusId.has_value());
    QCOMPARE(
        lastSelectedCampusPort.storedCampusId->value(),
        std::string("beta")
        );
    QVERIFY(statusLabel->text().isEmpty());
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

    const auto clearLastSelectedCampus =
        SettingsManagerLastSelectedCampusPort().write(std::nullopt);
    QVERIFY(clearLastSelectedCampus);
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

    const auto savedAlpha = loadCampusOrFail(
        campusDirectory.path(),
        QStringLiteral("alpha")
        );
    QVERIFY(savedAlpha.has_value());
    QCOMPARE(savedAlpha->campusName, QStringLiteral("Alpha Revised"));

}

void CampusDashboardPageTests::selectedCampusReadPreservesUtf8IdBytes()
{
    using ClassMngr::Next::Platform::SettingsManagerLastSelectedCampusPort;

    QTemporaryDir campusDirectory;
    QVERIFY(campusDirectory.isValid());
    const QString campusId = QStringLiteral("서울 캠퍼스");
    const std::string expectedUtf8Id(
        "\xEC\x84\x9C\xEC\x9A\xB8\x20"
        "\xEC\xBA\xA0\xED\x8D\xBC\xEC\x8A\xA4"
        );
    saveCampus(
        campusDirectory.path(),
        campusId,
        QStringLiteral("Seoul Campus"),
        QStringLiteral("Seoul Hall")
        );

    QVERIFY(SettingsManagerLastSelectedCampusPort().write(std::nullopt));
    RecordingCampusReadPort readPort(campusDirectory.path());
    CampusDashboardPage page(
        true,
        CampusDashboardPageDependencies{
            .campusDirectory = campusDirectory.path(),
            .selectedCampusReadPort = &readPort
        }
        );
    page.refresh();

    auto* selector = page.findChild<QComboBox*>();
    auto* campusName = page.findChild<QLineEdit*>(
        QStringLiteral("campusNameEdit")
        );
    QVERIFY(selector);
    QVERIFY(campusName);
    QCOMPARE(selector->currentData().toString(), campusId);
    QCOMPARE(readPort.readIds.size(), std::size_t(1));
    QCOMPARE(readPort.readIds.front(), expectedUtf8Id);
    QCOMPARE(campusName->text(), QStringLiteral("Seoul Campus"));
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

    const auto beforeSave = loadCampusOrFail(
        fixture.campusDirectory.path(),
        QStringLiteral("alpha")
        );
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

    const auto stillUnchanged = loadCampusOrFail(
        fixture.campusDirectory.path(),
        QStringLiteral("alpha")
        );
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
    QCOMPARE(
        fixture.savePort.saveIntents.front(),
        ClassMngr::Next::Application::CampusDashboardCampusSaveIntent::Update
        );
    const auto& savedSnapshot = fixture.savePort.savedSnapshots.front();
    QCOMPARE(savedSnapshot.id.value(), std::string("alpha"));
    QCOMPARE(savedSnapshot.campusName, std::string("Alpha Revised"));
    QCOMPARE(savedSnapshot.campusCode, std::string("AL2"));

    const auto savedCampus = loadCampusOrFail(
        fixture.campusDirectory.path(),
        QStringLiteral("alpha")
        );
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

    const auto savedCampus = loadCampusOrFail(
        fixture.campusDirectory.path(),
        QStringLiteral("alpha")
        );
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
    const auto persisted = loadCampusOrFail(
        fixture.campusDirectory.path(),
        QStringLiteral("alpha")
        );
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
    const auto persisted = loadCampusOrFail(
        fixture.campusDirectory.path(),
        QStringLiteral("alpha")
        );
    QVERIFY(persisted.has_value());
    QCOMPARE(persisted->campusName, QStringLiteral("Alpha"));
}

void CampusDashboardPageTests::missingUpdateTargetKeepsDraftDirtyAndShowsError()
{
    CampusDashboardPageFixture fixture;
    QVERIFY(fixture.initialize());
    CampusDashboardPage& page = *fixture.page;
    const QString failureMessage = QStringLiteral(
        "This campus no longer exists. Reload it before saving."
        );
    const QString alphaPath = CampusJsonRepository(
        fixture.campusDirectory.path()
        ).filePathForCampusId(QStringLiteral("alpha"));
    QVERIFY(QFile::exists(alphaPath));
    QVERIFY(QFile::remove(alphaPath));

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
    QCOMPARE(
        fixture.savePort.saveIntents.front(),
        ClassMngr::Next::Application::CampusDashboardCampusSaveIntent::Update
        );
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(nameEdit->text(), QStringLiteral("Alpha Draft"));
    QVERIFY(labelWithText(page, failureMessage));
    QVERIFY(!labelWithText(page, QStringLiteral("Saved")));
    QVERIFY(!QFile::exists(alphaPath));
}

void CampusDashboardPageTests::
localizedDefaultFallbackIsRejectedAfterEditingFinished_data()
{
    QTest::addColumn<bool>("create");

    QTest::newRow("create") << true;
    QTest::newRow("update") << false;
}

void CampusDashboardPageTests::
localizedDefaultFallbackIsRejectedAfterEditingFinished()
{
    QFETCH(bool, create);

    KoreanDefaultTranslator translator;
    ScopedTranslatorInstallation translatorInstallation(translator);
    QVERIFY(translatorInstallation.installed());

    CampusDashboardPageFixture fixture;
    QVERIFY(fixture.initialize());
    CampusDashboardPage& page = *fixture.page;
    const CampusJsonRepository repository(fixture.campusDirectory.path());
    const QString alphaPath = repository.filePathForCampusId(
        QStringLiteral("alpha")
        );
    const QString betaPath = repository.filePathForCampusId(
        QStringLiteral("beta")
        );
    std::vector<std::pair<QString, QByteArray>> existingFileBytes;
    for (const QString& path : {alphaPath, betaPath})
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        existingFileBytes.emplace_back(path, file.readAll());
    }

    if (create)
    {
        QPushButton* newCampusButton = buttonWithText(
            page,
            QStringLiteral("New Campus")
            );
        QVERIFY(newCampusButton);
        newCampusButton->click();
    }

    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("campusNameEdit")
        );
    QVERIFY(nameEdit);

    const QString emptyName;
    nameEdit->setText(emptyName);
    QVERIFY(QMetaObject::invokeMethod(
        nameEdit,
        "textEdited",
        Qt::DirectConnection,
        Q_ARG(QString, emptyName)
        ));
    QVERIFY(QMetaObject::invokeMethod(
        nameEdit,
        "editingFinished",
        Qt::DirectConnection
        ));

    QCOMPARE(nameEdit->text(), QStringLiteral("기본값"));
    QVERIFY(page.hasUnsavedChanges());

    QPushButton* saveButton = buttonWithText(
        page,
        QStringLiteral("Save Campus")
        );
    QVERIFY(saveButton);
    saveButton->click();

    QCOMPARE(fixture.savePort.savedSnapshots.size(), std::size_t(1));
    QCOMPARE(fixture.savePort.saveIntents.size(), std::size_t(1));
    QCOMPARE(fixture.savePort.savedNameOrigins.size(), std::size_t(1));
    QCOMPARE(
        fixture.savePort.savedNameOrigins.front(),
        ClassMngr::Next::Application::
            CampusDashboardCampusNameOrigin::LocalizedDefaultFallback
        );
    QCOMPARE(
        fixture.savePort.saveIntents.front(),
        create
            ? ClassMngr::Next::Application::
                  CampusDashboardCampusSaveIntent::Create
            : ClassMngr::Next::Application::
                  CampusDashboardCampusSaveIntent::Update
        );
    QCOMPARE(
        fixture.savePort.savedSnapshots.front().campusName,
        std::string("기본값")
        );
    QVERIFY(fixture.savePort.lastErrorCode.has_value());
    QCOMPARE(
        fixture.savePort.lastErrorCode.value(),
        ClassMngr::Next::Domain::ErrorCode::InvalidInput
        );
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(labelContainingText(
        page,
        QStringLiteral("Choose a different campus name")
        ));
    QVERIFY(!labelWithText(page, QStringLiteral("Saved")));

    if (create)
    {
        QVERIFY(!QFile::exists(
            repository.filePathForCampusId(QStringLiteral("campus"))
            ));
    }
    for (const auto& [path, expectedBytes] : existingFileBytes)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), expectedBytes);
    }
}

void CampusDashboardPageTests::
userEnteredLocalizedDefaultRemainsOrdinaryAfterFallback()
{
    KoreanDefaultTranslator translator;
    ScopedTranslatorInstallation translatorInstallation(translator);
    QVERIFY(translatorInstallation.installed());

    CampusDashboardPageFixture fixture;
    QVERIFY(fixture.initialize());
    CampusDashboardPage& page = *fixture.page;

    QPushButton* newCampusButton = buttonWithText(
        page,
        QStringLiteral("New Campus")
        );
    QVERIFY(newCampusButton);
    newCampusButton->click();

    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("campusNameEdit")
        );
    QVERIFY(nameEdit);
    const QString emptyName;
    nameEdit->setText(emptyName);
    QVERIFY(QMetaObject::invokeMethod(
        nameEdit,
        "textEdited",
        Qt::DirectConnection,
        Q_ARG(QString, emptyName)
        ));
    QVERIFY(QMetaObject::invokeMethod(
        nameEdit,
        "editingFinished",
        Qt::DirectConnection
        ));
    QCOMPARE(nameEdit->text(), QStringLiteral("기본값"));

    const QString intermediateName = QStringLiteral("Temporary");
    nameEdit->setText(intermediateName);
    QVERIFY(QMetaObject::invokeMethod(
        nameEdit,
        "textEdited",
        Qt::DirectConnection,
        Q_ARG(QString, intermediateName)
        ));

    const QString enteredLocalizedName = QStringLiteral("기본값");
    nameEdit->setText(enteredLocalizedName);
    QVERIFY(QMetaObject::invokeMethod(
        nameEdit,
        "textEdited",
        Qt::DirectConnection,
        Q_ARG(QString, enteredLocalizedName)
        ));

    QPushButton* saveButton = buttonWithText(
        page,
        QStringLiteral("Save Campus")
        );
    QVERIFY(saveButton);
    saveButton->click();

    QCOMPARE(fixture.savePort.savedNameOrigins.size(), std::size_t(1));
    QCOMPARE(
        fixture.savePort.savedNameOrigins.front(),
        ClassMngr::Next::Application::CampusDashboardCampusNameOrigin::
            UserEntered
        );
    QCOMPARE(
        fixture.savePort.savedSnapshots.front().campusName,
        std::string("기본값")
        );
    QVERIFY(!fixture.savePort.lastErrorCode.has_value());
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(labelWithText(page, QStringLiteral("Saved")));

    const auto savedCampus = loadCampusOrFail(
        fixture.campusDirectory.path(),
        QStringLiteral("campus")
        );
    QVERIFY(savedCampus.has_value());
    QCOMPARE(savedCampus->campusName, QStringLiteral("기본값"));
}

void CampusDashboardPageTests::cleanRefreshWithNoCampusesClearsCurrentCampus()
{
    CampusDashboardPageFixture fixture;
    QVERIFY(fixture.initialize());
    CampusDashboardPage& page = *fixture.page;

    auto* selector = page.findChild<QComboBox*>();
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("campusNameEdit")
        );
    auto* codeEdit = campusCodeEdit(page);
    auto* buildingEdit = page.findChild<QLineEdit*>(
        QStringLiteral("campusBuildingNameEdit")
        );
    QVERIFY(selector);
    QVERIFY(nameEdit);
    QVERIFY(codeEdit);
    QVERIFY(buildingEdit);
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(nameEdit->text(), QStringLiteral("Alpha"));
    QCOMPARE(buildingEdit->text(), QStringLiteral("Alpha Hall"));

    QVERIFY(markCampusFieldsEdited(
        page,
        QStringLiteral("Alpha"),
        QStringLiteral("ALPHA")
        ));
    QVERIFY(page.saveChanges());
    QVERIFY(!page.hasUnsavedChanges());
    QLabel* const statusLabel = labelWithText(
        page,
        QStringLiteral("Saved")
        );
    QVERIFY(statusLabel);
    QCOMPARE(fixture.savePort.saveIntents.size(), std::size_t(1));

    const CampusJsonRepository repository(fixture.campusDirectory.path());
    for (const QString& campusId : {
             QStringLiteral("alpha"),
             QStringLiteral("beta")
         })
    {
        const QString campusPath = repository.filePathForCampusId(campusId);
        QVERIFY(QFile::exists(campusPath));
        QVERIFY(QFile::remove(campusPath));
    }

    const std::size_t readCountBeforeEmptyRefresh =
        fixture.readPort.readIds.size();
    page.refresh();

    QCOMPARE(selector->count(), 0);
    QCOMPARE(selector->currentIndex(), -1);
    QVERIFY(selector->currentData().toString().isEmpty());
    QCOMPARE(
        fixture.readPort.readIds.size(),
        readCountBeforeEmptyRefresh
        );
    QCOMPARE(nameEdit->text(), QString());
    QCOMPARE(codeEdit->text(), QString());
    QCOMPARE(buildingEdit->text(), QString());
    QVERIFY(!page.hasUnsavedChanges());
    QPushButton* const saveButton = buttonWithText(
        page,
        QStringLiteral("Save Campus")
        );
    QVERIFY(saveButton);
    QVERIFY(!saveButton->isEnabled());
    QVERIFY(statusLabel->text().isEmpty());

    QVERIFY(markCampusFieldsEdited(
        page,
        QStringLiteral("Gamma"),
        QStringLiteral("GAMMA")
        ));
    QVERIFY(page.saveChanges());
    QCOMPARE(fixture.savePort.savedSnapshots.size(), std::size_t(2));
    QCOMPARE(
        fixture.savePort.saveIntents.back(),
        ClassMngr::Next::Application::CampusDashboardCampusSaveIntent::Create
        );
    QCOMPARE(
        fixture.savePort.savedSnapshots.back().id.value(),
        std::string("gamma")
        );
}

void CampusDashboardPageTests::
    cleanRefreshWithMissingSelectedCampusFallsBackToRemainingCampus()
{
    CampusDashboardPageFixture fixture;
    QVERIFY(fixture.initialize());
    CampusDashboardPage& page = *fixture.page;

    auto* selector = page.findChild<QComboBox*>();
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("campusNameEdit")
        );
    auto* codeEdit = campusCodeEdit(page);
    auto* buildingEdit = page.findChild<QLineEdit*>(
        QStringLiteral("campusBuildingNameEdit")
        );
    QVERIFY(selector);
    QVERIFY(nameEdit);
    QVERIFY(codeEdit);
    QVERIFY(buildingEdit);
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(nameEdit->text(), QStringLiteral("Alpha"));
    QCOMPARE(buildingEdit->text(), QStringLiteral("Alpha Hall"));
    QVERIFY(!page.hasUnsavedChanges());

    const CampusJsonRepository repository(fixture.campusDirectory.path());
    const QString alphaPath =
        repository.filePathForCampusId(QStringLiteral("alpha"));
    const QString betaPath =
        repository.filePathForCampusId(QStringLiteral("beta"));
    QVERIFY(QFile::exists(alphaPath));
    QVERIFY(QFile::exists(betaPath));
    QVERIFY(QFile::remove(alphaPath));

    page.refresh();

    QCOMPARE(selector->count(), 1);
    QCOMPARE(selector->currentData().toString(), QStringLiteral("beta"));
    QCOMPARE(nameEdit->text(), QStringLiteral("Beta"));
    QCOMPARE(codeEdit->text(), QStringLiteral("BETA"));
    QCOMPARE(buildingEdit->text(), QStringLiteral("Beta Hall"));
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(fixture.savePort.savedSnapshots.empty());
    QCOMPARE(fixture.readPort.readIds.size(), std::size_t(2));
    QCOMPARE(fixture.readPort.readIds.back(), std::string("beta"));
    QVERIFY(QFile::exists(betaPath));
}

void CampusDashboardPageTests::
    dirtyRefreshWithNoCampusesPreservesDraftAndPendingSave()
{
    CampusDashboardPageFixture fixture;
    QVERIFY(fixture.initialize(SaveMode::Automatic));
    CampusDashboardPage& page = *fixture.page;

    auto* selector = page.findChild<QComboBox*>();
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("campusNameEdit")
        );
    auto* codeEdit = campusCodeEdit(page);
    auto* buildingEdit = page.findChild<QLineEdit*>(
        QStringLiteral("campusBuildingNameEdit")
        );
    auto* saveTimer = page.findChild<QTimer*>();
    QVERIFY(selector);
    QVERIFY(nameEdit);
    QVERIFY(codeEdit);
    QVERIFY(buildingEdit);
    QVERIFY(saveTimer);
    saveTimer->setInterval(60000);

    QVERIFY(markCampusFieldsEdited(
        page,
        QStringLiteral("Alpha Draft"),
        QStringLiteral("ALPHA-DRAFT")
        ));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(saveTimer->isActive());
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));

    const CampusJsonRepository repository(fixture.campusDirectory.path());
    for (const QString& campusId : {
             QStringLiteral("alpha"),
             QStringLiteral("beta")
         })
    {
        const QString campusPath = repository.filePathForCampusId(campusId);
        QVERIFY(QFile::exists(campusPath));
        QVERIFY(QFile::remove(campusPath));
    }

    page.refresh();

    QCOMPARE(selector->count(), 2);
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(nameEdit->text(), QStringLiteral("Alpha Draft"));
    QCOMPARE(codeEdit->text(), QStringLiteral("ALPHA-DRAFT"));
    QCOMPARE(buildingEdit->text(), QStringLiteral("Alpha Hall"));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(saveTimer->isActive());
    QVERIFY(fixture.savePort.savedSnapshots.empty());

    QVERIFY(!page.saveChanges());

    QCOMPARE(fixture.savePort.savedSnapshots.size(), std::size_t(1));
    QCOMPARE(
        fixture.savePort.saveIntents.front(),
        ClassMngr::Next::Application::CampusDashboardCampusSaveIntent::Update
        );
    QCOMPARE(
        fixture.savePort.savedSnapshots.front().campusName,
        std::string("Alpha Draft")
        );
    QCOMPARE(
        fixture.savePort.savedSnapshots.front().campusCode,
        std::string("ALPHA-DRAFT")
        );
    QCOMPARE(
        fixture.savePort.savedSnapshots.front().id.value(),
        std::string("alpha")
        );
    QVERIFY(fixture.savePort.lastErrorCode.has_value());
    QCOMPARE(
        fixture.savePort.lastErrorCode.value(),
        ClassMngr::Next::Domain::ErrorCode::NotFound
        );
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(nameEdit->text(), QStringLiteral("Alpha Draft"));
    QVERIFY(labelWithText(
        page,
        QStringLiteral("This campus no longer exists. Reload it before saving.")
        ));
    saveTimer->stop();
}

void CampusDashboardPageTests::
    dirtyRefreshWithMissingSelectedCampusPreservesDraftUntilDiscarded()
{
    CampusDashboardPageFixture fixture;
    QVERIFY(fixture.initialize(SaveMode::Automatic));
    CampusDashboardPage& page = *fixture.page;

    auto* selector = page.findChild<QComboBox*>();
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("campusNameEdit")
        );
    auto* codeEdit = campusCodeEdit(page);
    auto* buildingEdit = page.findChild<QLineEdit*>(
        QStringLiteral("campusBuildingNameEdit")
        );
    auto* saveTimer = page.findChild<QTimer*>();
    QVERIFY(selector);
    QVERIFY(nameEdit);
    QVERIFY(codeEdit);
    QVERIFY(buildingEdit);
    QVERIFY(saveTimer);
    saveTimer->setInterval(60000);

    QVERIFY(markCampusFieldsEdited(
        page,
        QStringLiteral("Alpha Draft"),
        QStringLiteral("ALPHA-DRAFT")
        ));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(saveTimer->isActive());
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QVERIFY(selector->findData(QStringLiteral("beta")) >= 0);
    const std::size_t readCountBeforeRefresh = fixture.readPort.readIds.size();

    const CampusJsonRepository repository(fixture.campusDirectory.path());
    const QString alphaPath =
        repository.filePathForCampusId(QStringLiteral("alpha"));
    const QString betaPath =
        repository.filePathForCampusId(QStringLiteral("beta"));
    QVERIFY(QFile::exists(alphaPath));
    QVERIFY(QFile::exists(betaPath));
    QVERIFY(QFile::remove(alphaPath));

    page.refresh();

    QCOMPARE(selector->count(), 2);
    QVERIFY(selector->findData(QStringLiteral("alpha")) >= 0);
    QVERIFY(selector->findData(QStringLiteral("beta")) >= 0);
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(nameEdit->text(), QStringLiteral("Alpha Draft"));
    QCOMPARE(codeEdit->text(), QStringLiteral("ALPHA-DRAFT"));
    QCOMPARE(buildingEdit->text(), QStringLiteral("Alpha Hall"));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(saveTimer->isActive());
    QVERIFY(fixture.savePort.savedSnapshots.empty());
    QCOMPARE(fixture.readPort.readIds.size(), readCountBeforeRefresh);
    QVERIFY(labelWithText(
        page,
        QStringLiteral("The selected campus is no longer available.")
        ));
    QVERIFY(QFile::exists(betaPath));

    QVERIFY(!page.saveChanges());

    QCOMPARE(fixture.savePort.savedSnapshots.size(), std::size_t(1));
    QCOMPARE(
        fixture.savePort.saveIntents.front(),
        ClassMngr::Next::Application::CampusDashboardCampusSaveIntent::Update
        );
    QCOMPARE(
        fixture.savePort.savedSnapshots.front().id.value(),
        std::string("alpha")
        );
    QCOMPARE(
        fixture.savePort.savedSnapshots.front().campusName,
        std::string("Alpha Draft")
        );
    QCOMPARE(
        fixture.savePort.savedSnapshots.front().campusCode,
        std::string("ALPHA-DRAFT")
        );
    QVERIFY(fixture.savePort.lastErrorCode.has_value());
    QCOMPARE(
        fixture.savePort.lastErrorCode.value(),
        ClassMngr::Next::Domain::ErrorCode::NotFound
        );
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(nameEdit->text(), QStringLiteral("Alpha Draft"));
    QCOMPARE(codeEdit->text(), QStringLiteral("ALPHA-DRAFT"));

    page.discardChanges();
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(!saveTimer->isActive());
    QCOMPARE(nameEdit->text(), QStringLiteral("Alpha"));

    page.refresh();

    QCOMPARE(selector->count(), 1);
    QCOMPARE(selector->currentData().toString(), QStringLiteral("beta"));
    QCOMPARE(nameEdit->text(), QStringLiteral("Beta"));
    QCOMPARE(buildingEdit->text(), QStringLiteral("Beta Hall"));
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(!saveTimer->isActive());
    QCOMPARE(fixture.savePort.savedSnapshots.size(), std::size_t(1));
    QCOMPARE(fixture.readPort.readIds.size(), readCountBeforeRefresh + 1);
    QCOMPARE(fixture.readPort.readIds.back(), std::string("beta"));
}

void CampusDashboardPageTests::
    dirtyRefreshWithPresentSelectedCampusSavesDraft()
{
    CampusDashboardPageFixture fixture;
    QVERIFY(fixture.initialize(SaveMode::Automatic));
    CampusDashboardPage& page = *fixture.page;

    auto* selector = page.findChild<QComboBox*>();
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("campusNameEdit")
        );
    auto* codeEdit = campusCodeEdit(page);
    auto* buildingEdit = page.findChild<QLineEdit*>(
        QStringLiteral("campusBuildingNameEdit")
        );
    auto* saveTimer = page.findChild<QTimer*>();
    QVERIFY(selector);
    QVERIFY(nameEdit);
    QVERIFY(codeEdit);
    QVERIFY(buildingEdit);
    QVERIFY(saveTimer);
    saveTimer->setInterval(60000);

    QVERIFY(markCampusFieldsEdited(
        page,
        QStringLiteral("Alpha Draft"),
        QStringLiteral("ALPHA-DRAFT")
        ));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(saveTimer->isActive());
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));

    const std::size_t readCountBeforeRefresh = fixture.readPort.readIds.size();
    page.refresh();

    QCOMPARE(selector->count(), 2);
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(nameEdit->text(), QStringLiteral("Alpha Draft"));
    QCOMPARE(codeEdit->text(), QStringLiteral("ALPHA-DRAFT"));
    QCOMPARE(buildingEdit->text(), QStringLiteral("Alpha Hall"));

    QCOMPARE(fixture.savePort.savedSnapshots.size(), std::size_t(1));
    QCOMPARE(fixture.savePort.saveIntents.size(), std::size_t(1));
    QCOMPARE(
        fixture.savePort.saveIntents.front(),
        ClassMngr::Next::Application::CampusDashboardCampusSaveIntent::Update
        );
    const auto& savedDraft = fixture.savePort.savedSnapshots.front();
    QCOMPARE(savedDraft.id.value(), std::string("alpha"));
    QCOMPARE(savedDraft.campusName, std::string("Alpha Draft"));
    QCOMPARE(savedDraft.campusCode, std::string("ALPHA-DRAFT"));
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(!saveTimer->isActive());
    QCOMPARE(fixture.readPort.readIds.size(), readCountBeforeRefresh + 1);
    QCOMPARE(fixture.readPort.readIds.back(), std::string("alpha"));
}

void CampusDashboardPageTests::reservedCreateIdentityFailureRetainsDirtyForm()
{
    CampusDashboardPageFixture fixture;
    QVERIFY(fixture.initialize());
    CampusDashboardPage& page = *fixture.page;

    QPushButton* newCampusButton = buttonWithText(
        page,
        QStringLiteral("New Campus")
        );
    QVERIFY(newCampusButton);
    newCampusButton->click();

    QVERIFY(markCampusFieldsEdited(
        page,
        QStringLiteral("Default"),
        QStringLiteral("DEFAULT")
        ));
    QVERIFY(page.hasUnsavedChanges());

    QPushButton* saveButton = buttonWithText(
        page,
        QStringLiteral("Save Campus")
        );
    QVERIFY(saveButton);
    saveButton->click();

    QCOMPARE(fixture.savePort.savedSnapshots.size(), std::size_t(1));
    QCOMPARE(
        fixture.savePort.saveIntents.front(),
        ClassMngr::Next::Application::CampusDashboardCampusSaveIntent::Create
        );
    QCOMPARE(
        fixture.savePort.savedSnapshots.front().id.value(),
        std::string("default")
        );
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(labelContainingText(
        page,
        QStringLiteral("Choose a different campus name")
        ));
    QVERIFY(!labelWithText(page, QStringLiteral("Saved")));
    QVERIFY(!QFile::exists(
        CampusJsonRepository(fixture.campusDirectory.path())
            .filePathForCampusId(QStringLiteral("default"))
        ));
}

void CampusDashboardPageTests::reservedUpdateNameFailureRetainsDirtyForm()
{
    CampusDashboardPageFixture fixture;
    QVERIFY(fixture.initialize());
    CampusDashboardPage& page = *fixture.page;

    const QString alphaPath = CampusJsonRepository(
        fixture.campusDirectory.path()
        ).filePathForCampusId(QStringLiteral("alpha"));
    QFile originalFile(alphaPath);
    QVERIFY(originalFile.open(QIODevice::ReadOnly));
    const QByteArray before = originalFile.readAll();
    originalFile.close();

    auto* selector = page.findChild<QComboBox*>();
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("campusNameEdit")
        );
    QVERIFY(selector);
    QVERIFY(nameEdit);
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));

    QVERIFY(markCampusFieldsEdited(
        page,
        QStringLiteral("Default"),
        QStringLiteral("ALPHA")
        ));
    QVERIFY(page.hasUnsavedChanges());

    QPushButton* saveButton = buttonWithText(
        page,
        QStringLiteral("Save Campus")
        );
    QVERIFY(saveButton);
    saveButton->click();

    QCOMPARE(fixture.savePort.savedSnapshots.size(), std::size_t(1));
    QCOMPARE(
        fixture.savePort.saveIntents.front(),
        ClassMngr::Next::Application::CampusDashboardCampusSaveIntent::Update
        );
    QCOMPARE(
        fixture.savePort.savedSnapshots.front().id.value(),
        std::string("alpha")
        );
    QCOMPARE(nameEdit->text(), QStringLiteral("Default"));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(labelContainingText(
        page,
        QStringLiteral("Choose a different campus name")
        ));
    QVERIFY(!labelWithText(page, QStringLiteral("Saved")));

    QFile unchangedFile(alphaPath);
    QVERIFY(unchangedFile.open(QIODevice::ReadOnly));
    QCOMPARE(unchangedFile.readAll(), before);
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
successfulNewCampusSaveRetainsIdForUpdates()
{
    CampusDashboardPageFixture fixture;
    QVERIFY(fixture.initialize());
    CampusDashboardPage& page = *fixture.page;

    auto* selector = page.findChild<QComboBox*>();
    QVERIFY(selector);
    QPushButton* newCampusButton = buttonWithText(
        page,
        QStringLiteral("New Campus")
        );
    QVERIFY(newCampusButton);
    newCampusButton->click();

    QPushButton* saveButton = buttonWithText(
        page,
        QStringLiteral("Save Campus")
        );
    QVERIFY(saveButton);
    saveButton->click();

    QCOMPARE(fixture.savePort.saveIntents.size(), std::size_t(1));
    QCOMPARE(
        fixture.savePort.saveIntents.front(),
        ClassMngr::Next::Application::CampusDashboardCampusSaveIntent::Create
        );
    QCOMPARE(
        fixture.savePort.savedSnapshots.front().id.value(),
        std::string("new_campus")
        );
    QCOMPARE(selector->currentData().toString(), QStringLiteral("new_campus"));
    QVERIFY(!page.hasUnsavedChanges());

    const QString updatedName = QStringLiteral("New Campus Edited");
    QVERIFY(markCampusFieldsEdited(
        page,
        updatedName,
        QStringLiteral("NEW-EDITED")
        ));
    saveButton->click();

    QCOMPARE(fixture.savePort.saveIntents.size(), std::size_t(2));
    QCOMPARE(
        fixture.savePort.saveIntents.back(),
        ClassMngr::Next::Application::CampusDashboardCampusSaveIntent::Update
        );
    QCOMPARE(
        fixture.savePort.savedSnapshots.back().id.value(),
        std::string("new_campus")
        );
    const auto saved = loadCampusOrFail(
        fixture.campusDirectory.path(),
        QStringLiteral("new_campus")
        );
    QVERIFY(saved.has_value());
    QCOMPARE(saved->campusName, updatedName);
    QCOMPARE(saved->campusCode, QStringLiteral("NEW-EDITED"));
}

void CampusDashboardPageTests::
saveIntentTracksDraftAndCreateConflictRetainsIt()
{
    CampusDashboardPageFixture fixture;
    QVERIFY(fixture.initialize());
    CampusDashboardPage& page = *fixture.page;

    auto* selector = page.findChild<QComboBox*>();
    QVERIFY(selector);
    QVERIFY(markCampusFieldsEdited(
        page,
        QStringLiteral("Alpha Updated Before Draft"),
        QStringLiteral("ALPHA-2")
        ));

    QPushButton* saveButton = buttonWithText(
        page,
        QStringLiteral("Save Campus")
        );
    QVERIFY(saveButton);
    saveButton->click();
    QCOMPARE(fixture.savePort.saveIntents.size(), std::size_t(1));
    QCOMPARE(
        fixture.savePort.saveIntents.front(),
        ClassMngr::Next::Application::CampusDashboardCampusSaveIntent::Update
        );

    fixture.savePort.createConflictMessage =
        QStringLiteral("A campus with this ID already exists.");
    QPushButton* newCampusButton = buttonWithText(
        page,
        QStringLiteral("New Campus")
        );
    QVERIFY(newCampusButton);
    newCampusButton->click();

    const QString editedName = QStringLiteral("North West");
    QVERIFY(markCampusFieldsEdited(
        page,
        editedName,
        QStringLiteral("NW")
        ));
    saveButton->click();

    QCOMPARE(fixture.savePort.saveIntents.size(), std::size_t(2));
    QCOMPARE(
        fixture.savePort.saveIntents.back(),
        ClassMngr::Next::Application::CampusDashboardCampusSaveIntent::Create
        );
    QCOMPARE(
        fixture.savePort.savedSnapshots.back().id.value(),
        std::string("north_west")
        );
    QVERIFY(selector->currentData().toString().isEmpty());
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(
        page.findChild<QLineEdit*>(QStringLiteral("campusNameEdit"))->text(),
        editedName
        );
    QVERIFY(labelWithText(page, fixture.savePort.createConflictMessage));
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

    const auto clearLastSelectedCampus =
        SettingsManagerLastSelectedCampusPort().write(std::nullopt);
    QVERIFY(clearLastSelectedCampus);
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

    const auto clearLastSelectedCampus =
        SettingsManagerLastSelectedCampusPort().write(std::nullopt);
    QVERIFY(clearLastSelectedCampus);
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

void CampusDashboardPageTests::
adminSelectedCampusDetailFailureRestoresLoadedCampus_data()
{
    QTest::addColumn<bool>("missingDetail");

    QTest::newRow("read-error") << false;
    QTest::newRow("missing-detail") << true;
}

void CampusDashboardPageTests::
adminSelectedCampusDetailFailureRestoresLoadedCampus()
{
    QFETCH(bool, missingDetail);

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

    RecordingCampusReadPort readPort(campusDirectory.path());
    if (missingDetail)
    {
        readPort.absentCampusId = QStringLiteral("beta");
    }
    else
    {
        readPort.failedCampusId = QStringLiteral("beta");
    }
    RecordingCampusSavePort savePort(campusDirectory.path());
    RecordingLastSelectedCampusPort lastSelectedCampusPort;
    CampusDashboardPage page(
        true,
        CampusDashboardPageDependencies{
            .campusDirectory = campusDirectory.path(),
            .selectedCampusReadPort = &readPort,
            .campusSavePort = &savePort,
            .lastSelectedCampusPort = &lastSelectedCampusPort
        }
        );
    page.setSaveMode(SaveMode::Manual);
    page.refresh();

    auto* selector = page.findChild<QComboBox*>();
    auto* campusName = page.findChild<QLineEdit*>(
        QStringLiteral("campusNameEdit")
        );
    auto* buildingName = page.findChild<QLineEdit*>(
        QStringLiteral("campusBuildingNameEdit")
        );
    QVERIFY(selector);
    QVERIFY(campusName);
    QVERIFY(buildingName);
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(campusName->text(), QStringLiteral("Alpha"));
    QCOMPARE(buildingName->text(), QStringLiteral("Alpha Hall"));
    QCOMPARE(lastSelectedCampusPort.writeAttempts.size(), std::size_t(1));
    QVERIFY(lastSelectedCampusPort.storedCampusId.has_value());
    QCOMPARE(
        lastSelectedCampusPort.storedCampusId->value(),
        std::string("alpha")
        );

    selector->setCurrentIndex(selector->findData(QStringLiteral("beta")));
    QCoreApplication::processEvents();

    const QString failureMessage =
        QStringLiteral("The requested campus could not be loaded.");
    QLabel* const statusLabel = labelWithText(page, failureMessage);
    QVERIFY(statusLabel);
    QVERIFY(!statusLabel->isHidden());
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(campusName->text(), QStringLiteral("Alpha"));
    QCOMPARE(buildingName->text(), QStringLiteral("Alpha Hall"));
    QCOMPARE(readPort.readIds.size(), std::size_t(2));
    QCOMPARE(readPort.readIds.at(1), std::string("beta"));
    QCOMPARE(lastSelectedCampusPort.writeAttempts.size(), std::size_t(1));
    QVERIFY(lastSelectedCampusPort.storedCampusId.has_value());
    QCOMPARE(
        lastSelectedCampusPort.storedCampusId->value(),
        std::string("alpha")
        );
    for (const auto& writeAttempt : lastSelectedCampusPort.writeAttempts)
    {
        QVERIFY(writeAttempt.has_value());
        QVERIFY(writeAttempt->value() != "beta");
    }

    QVERIFY(markCampusFieldsEdited(
        page,
        QStringLiteral("Alpha Revised"),
        QStringLiteral("ALPHA2")
        ));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(page.saveChanges());
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(selector->currentData().toString(), QStringLiteral("alpha"));
    QCOMPARE(campusName->text(), QStringLiteral("Alpha Revised"));
    QCOMPARE(savePort.savedSnapshots.size(), std::size_t(1));
    QCOMPARE(savePort.savedSnapshots.front().id.value(), std::string("alpha"));
    QCOMPARE(
        savePort.savedSnapshots.front().campusName,
        std::string("Alpha Revised")
        );
    QCOMPARE(lastSelectedCampusPort.writeAttempts.size(), std::size_t(1));
    QVERIFY(lastSelectedCampusPort.storedCampusId.has_value());
    QCOMPARE(
        lastSelectedCampusPort.storedCampusId->value(),
        std::string("alpha")
        );

    const auto savedAlpha = loadCampusOrFail(
        campusDirectory.path(),
        QStringLiteral("alpha")
        );
    const auto untouchedBeta = loadCampusOrFail(
        campusDirectory.path(),
        QStringLiteral("beta")
        );
    QVERIFY(savedAlpha.has_value());
    QVERIFY(untouchedBeta.has_value());
    QCOMPARE(savedAlpha->campusName, QStringLiteral("Alpha Revised"));
    QCOMPARE(untouchedBeta->campusName, QStringLiteral("Beta"));

    readPort.absentCampusId.clear();
    readPort.failedCampusId.clear();
    selector->setCurrentIndex(selector->findData(QStringLiteral("beta")));
    QCoreApplication::processEvents();

    QCOMPARE(selector->currentData().toString(), QStringLiteral("beta"));
    QCOMPARE(campusName->text(), QStringLiteral("Beta"));
    QCOMPARE(buildingName->text(), QStringLiteral("Beta Hall"));
    QCOMPARE(readPort.readIds.size(), std::size_t(3));
    QCOMPARE(readPort.readIds.at(2), std::string("beta"));
    QCOMPARE(lastSelectedCampusPort.writeAttempts.size(), std::size_t(2));
    QVERIFY(lastSelectedCampusPort.storedCampusId.has_value());
    QCOMPARE(
        lastSelectedCampusPort.storedCampusId->value(),
        std::string("beta")
        );
    QVERIFY(lastSelectedCampusPort.writeAttempts.back().has_value());
    QCOMPARE(
        lastSelectedCampusPort.writeAttempts.back()->value(),
        std::string("beta")
        );
    QVERIFY(statusLabel->text().isEmpty());
}

QTEST_MAIN(CampusDashboardPageTests)

#include "campus_dashboard_page_tests.moc"
