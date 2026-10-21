#include "core/application_services.h"
#include "features/classes/ui/class_analytics_page.h"
#include "features/classes/ui/class_analytics_ranking_header.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTableView>
#include <QTranslator>
#include <QtTest/QtTest>

#include <array>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

class EmptyDashboardReadPort final : public ClassAnalyticsDashboardReadPort
{
public:
    mutable int rosterReadCount = 0;
    mutable int evaluationBatchReadCount = 0;
    bool failRoster = false;
    bool failEvaluationBatch = false;
    ClassAnalyticsRosterNames roster;
    std::array<ClassAnalyticsEvaluationRows, 4> evaluations;

    [[nodiscard]] Domain::Result<ClassAnalyticsRosterNames> readRosterNames(
        const Domain::ClassId& classId
        ) const override
    {
        (void)classId;
        ++rosterReadCount;
        if (failRoster)
        {
            return Domain::Result<ClassAnalyticsRosterNames>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "roster read failed",
                .recoverable = true
            });
        }
        return Domain::Result<ClassAnalyticsRosterNames>::success(roster);
    }

    [[nodiscard]] Domain::Result<ClassAnalyticsEvaluationBatch>
    readEvaluationBatch(
        const Domain::ClassId& classId
        ) const override
    {
        (void)classId;
        ++evaluationBatchReadCount;
        if (failEvaluationBatch)
        {
            return Domain::Result<ClassAnalyticsEvaluationBatch>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "evaluation batch read failed",
                .recoverable = true
            });
        }
        return Domain::Result<ClassAnalyticsEvaluationBatch>::success(
            evaluations);
    }
};

class AnalyticsReadErrorTranslator final : public QTranslator
{
public:
    [[nodiscard]] QString translate(
        const char* context,
        const char* sourceText,
        const char* disambiguation = nullptr,
        const int n = -1
        ) const override
    {
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);
        if (!context || qstrcmp(context, "ClassAnalyticsPage") != 0
            || !sourceText)
        {
            return {};
        }

        if (qstrcmp(
                sourceText,
                "Class analytics data could not be loaded. Please try again."
                ) == 0)
        {
            return QStringLiteral(
                "수업 분석 데이터를 불러오지 못했습니다. 다시 시도해 주세요.");
        }
        if (qstrcmp(sourceText, "Retry") == 0)
            return QStringLiteral("다시 시도");
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
            QCoreApplication::removeTranslator(m_translator);
    }

    [[nodiscard]] bool installed() const { return m_installed; }

private:
    QTranslator* m_translator;
    bool m_installed;
};

class TestNameSemantics final : public ClassAnalyticsNameSemanticsPort
{
public:
    [[nodiscard]] std::u16string trimmed(
        const std::u16string_view value
        ) const override
    {
        return std::u16string(value);
    }

    [[nodiscard]] std::u16string normalizedEnglishIdentity(
        const std::u16string_view value
        ) const override
    {
        return std::u16string(value);
    }

    [[nodiscard]] std::u16string baseKoreanName(
        const std::u16string_view value
        ) const override
    {
        return std::u16string(value);
    }

    [[nodiscard]] bool equalsCaseInsensitive(
        const std::u16string_view left,
        const std::u16string_view right
        ) const override
    {
        if (left.size() != right.size()) return false;
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            const char16_t a = left[index] >= u'A' && left[index] <= u'Z'
                ? static_cast<char16_t>(left[index] + (u'a' - u'A'))
                : left[index];
            const char16_t b = right[index] >= u'A' && right[index] <= u'Z'
                ? static_cast<char16_t>(right[index] + (u'a' - u'A'))
                : right[index];
            if (a != b) return false;
        }
        return true;
    }

    [[nodiscard]] int compareEnglishNames(
        const std::u16string_view left,
        const std::u16string_view right
        ) const override
    {
        return left < right ? -1 : left > right ? 1 : 0;
    }
};

} // namespace

class ClassAnalyticsPageTests final : public QObject
{
    Q_OBJECT

private slots:
    void successfulEmptyDashboardShowsNoDataState();
    void rosterReadFailureShowsRetryAndRecovers();
    void evaluationBatchReadFailureShowsRetryAndRecovers();
    void failedRefreshDoesNotPresentPreviousClassResultsAsCurrent();
    void successfulDashboardMapsResultsIntoRankingModel();
};

namespace
{

void populateOneEvaluationResult(EmptyDashboardReadPort& readPort)
{
    readPort.roster.rowCount = ClassAnalyticsRosterRowCount{std::size_t{1}};
    readPort.roster.hasEnglishColumn = true;
    readPort.roster.hasKoreanColumn = true;
    readPort.roster.englishNames = { u"Alex Example" };
    readPort.roster.koreanNames = { u"\uAE40\uBBFC\uC218" };

    ClassAnalyticsEvaluationRow winterRow;
    winterRow.englishName = u"Alex Example";
    winterRow.koreanName = u"\uAE40\uBBFC\uC218";
    winterRow.scores.fill(u"A");
    readPort.evaluations[static_cast<std::size_t>(
        ClassAnalyticsEvaluation::Winter)].push_back(winterRow);
}

QLabel* labelWithText(const QWidget& widget, const QString& text)
{
    for (QLabel* const label : widget.findChildren<QLabel*>())
    {
        if (label->text() == text)
            return label;
    }
    return nullptr;
}

void verifyReadErrorPresentation(
    const ClassAnalyticsPage& page,
    const QString& rawErrorMessage
    )
{
    QLabel* const emptyLabel = page.findChild<QLabel*>(
        QStringLiteral("classAnalyticsEmptyLabel"));
    QVERIFY(emptyLabel);
    QVERIFY(!emptyLabel->isVisible());

    QWidget* const errorContainer = page.findChild<QWidget*>(
        QStringLiteral("classAnalyticsReadErrorContainer"));
    QVERIFY(errorContainer);
    QVERIFY(errorContainer->isVisible());
    QLabel* const errorLabel = page.findChild<QLabel*>(
        QStringLiteral("classAnalyticsReadErrorLabel"));
    QVERIFY(errorLabel);
    QVERIFY(errorLabel->isVisible());
    QCOMPARE(errorLabel->text(),
        QStringLiteral(
            "수업 분석 데이터를 불러오지 못했습니다. 다시 시도해 주세요."));
    QVERIFY(!errorLabel->text().isEmpty());
    QVERIFY(errorLabel->text() != emptyLabel->text());
    QVERIFY(!errorLabel->text().contains(rawErrorMessage));

    QPushButton* const retryButton = page.findChild<QPushButton*>(
        QStringLiteral("classAnalyticsRetryButton"));
    QVERIFY(retryButton);
    QVERIFY(retryButton->isVisible());
    QVERIFY(retryButton->isEnabled());
    QCOMPARE(retryButton->text(), QStringLiteral("다시 시도"));

    QVERIFY(QApplication::activeModalWidget() == nullptr);
    QVERIFY(page.findChildren<QDialog*>().isEmpty());
    for (QWidget* const topLevelWidget : QApplication::topLevelWidgets())
    {
        QDialog* const dialog = qobject_cast<QDialog*>(topLevelWidget);
        QVERIFY(!dialog || !dialog->isVisible());
    }
}

void verifyDashboardResultsVisible(const ClassAnalyticsPage& page)
{
    QLabel* const emptyLabel = page.findChild<QLabel*>(
        QStringLiteral("classAnalyticsEmptyLabel"));
    QVERIFY(emptyLabel);
    QVERIFY(!emptyLabel->isVisible());
    QWidget* const errorContainer = page.findChild<QWidget*>(
        QStringLiteral("classAnalyticsReadErrorContainer"));
    QVERIFY(errorContainer);
    QVERIFY(!errorContainer->isVisible());
    QTableView* const rankingTable = page.findChild<QTableView*>(
        QStringLiteral("classAnalyticsRankingTable"));
    QVERIFY(rankingTable);
    QVERIFY(rankingTable->isVisible());
    QVERIFY(rankingTable->model());
    QCOMPARE(rankingTable->model()->rowCount(), 1);
}

} // namespace

void ClassAnalyticsPageTests::successfulEmptyDashboardShowsNoDataState()
{
    ApplicationServices services;
    EmptyDashboardReadPort readPort;
    const TestNameSemantics names;
    ClassAnalyticsPage page(
        &services,
        { .readPort = &readPort, .nameSemantics = &names });
    page.loadClass(Classroom(QStringLiteral("Analytics"), 42));
    page.show();
    QCoreApplication::processEvents();

    QLabel* const emptyLabel = page.findChild<QLabel*>(
        QStringLiteral("classAnalyticsEmptyLabel"));
    QVERIFY(emptyLabel);
    QVERIFY(emptyLabel->isVisible());
    QWidget* const errorContainer = page.findChild<QWidget*>(
        QStringLiteral("classAnalyticsReadErrorContainer"));
    QVERIFY(errorContainer);
    QVERIFY(!errorContainer->isVisible());
    QCOMPARE(readPort.rosterReadCount, 1);
    QCOMPARE(readPort.evaluationBatchReadCount, 1);
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

void ClassAnalyticsPageTests::rosterReadFailureShowsRetryAndRecovers()
{
    ApplicationServices services;
    EmptyDashboardReadPort readPort;
    readPort.failRoster = true;
    const TestNameSemantics names;
    AnalyticsReadErrorTranslator translator;
    ScopedTranslatorInstallation translatorInstallation(translator);
    QVERIFY(translatorInstallation.installed());

    ClassAnalyticsPage page(
        &services,
        { .readPort = &readPort, .nameSemantics = &names });
    page.loadClass(Classroom(QStringLiteral("Analytics"), 42));
    page.show();
    QCoreApplication::processEvents();

    verifyReadErrorPresentation(page, QStringLiteral("roster read failed"));
    QCOMPARE(readPort.rosterReadCount, 1);
    QCOMPARE(readPort.evaluationBatchReadCount, 0);

    populateOneEvaluationResult(readPort);
    readPort.failRoster = false;
    page.findChild<QPushButton*>(QStringLiteral("classAnalyticsRetryButton"))
        ->click();
    QCoreApplication::processEvents();

    QCOMPARE(readPort.rosterReadCount, 2);
    QCOMPARE(readPort.evaluationBatchReadCount, 1);
    verifyDashboardResultsVisible(page);
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

void ClassAnalyticsPageTests::evaluationBatchReadFailureShowsRetryAndRecovers()
{
    ApplicationServices services;
    EmptyDashboardReadPort readPort;
    readPort.failEvaluationBatch = true;
    const TestNameSemantics names;
    AnalyticsReadErrorTranslator translator;
    ScopedTranslatorInstallation translatorInstallation(translator);
    QVERIFY(translatorInstallation.installed());

    ClassAnalyticsPage page(
        &services,
        { .readPort = &readPort, .nameSemantics = &names });
    page.loadClass(Classroom(QStringLiteral("Analytics"), 42));
    page.show();
    QCoreApplication::processEvents();

    verifyReadErrorPresentation(
        page, QStringLiteral("evaluation batch read failed"));
    QCOMPARE(readPort.rosterReadCount, 1);
    QCOMPARE(readPort.evaluationBatchReadCount, 1);

    readPort.failEvaluationBatch = false;
    page.findChild<QPushButton*>(QStringLiteral("classAnalyticsRetryButton"))
        ->click();
    QCoreApplication::processEvents();

    QCOMPARE(readPort.rosterReadCount, 2);
    QCOMPARE(readPort.evaluationBatchReadCount, 2);
    QLabel* const emptyLabel = page.findChild<QLabel*>(
        QStringLiteral("classAnalyticsEmptyLabel"));
    QVERIFY(emptyLabel);
    QVERIFY(emptyLabel->isVisible());
    QWidget* const errorContainer = page.findChild<QWidget*>(
        QStringLiteral("classAnalyticsReadErrorContainer"));
    QVERIFY(errorContainer);
    QVERIFY(!errorContainer->isVisible());
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

void ClassAnalyticsPageTests::
    failedRefreshDoesNotPresentPreviousClassResultsAsCurrent()
{
    ApplicationServices services;
    EmptyDashboardReadPort readPort;
    populateOneEvaluationResult(readPort);
    const TestNameSemantics names;
    AnalyticsReadErrorTranslator translator;
    ScopedTranslatorInstallation translatorInstallation(translator);
    QVERIFY(translatorInstallation.installed());

    ClassAnalyticsPage page(
        &services,
        { .readPort = &readPort, .nameSemantics = &names });
    page.loadClass(Classroom(QStringLiteral("Analytics"), 42));
    page.show();
    QCoreApplication::processEvents();

    verifyDashboardResultsVisible(page);
    const QString previousAverage = QStringLiteral("A · %1").arg(
        SpeakingAnalytics::formatAverage(4.0));
    QLabel* const averageValue = labelWithText(page, previousAverage);
    QVERIFY(averageValue);

    readPort.failEvaluationBatch = true;
    page.refresh();
    QCoreApplication::processEvents();

    verifyReadErrorPresentation(
        page, QStringLiteral("evaluation batch read failed"));
    QCOMPARE(averageValue->text(), QStringLiteral("—"));
    QTableView* const rankingTable = page.findChild<QTableView*>(
        QStringLiteral("classAnalyticsRankingTable"));
    QVERIFY(rankingTable);
    QVERIFY(rankingTable->model());
    QCOMPARE(rankingTable->model()->rowCount(), 0);
}

void ClassAnalyticsPageTests::successfulDashboardMapsResultsIntoRankingModel()
{
    ApplicationServices services;
    EmptyDashboardReadPort readPort;
    readPort.roster.rowCount = ClassAnalyticsRosterRowCount{std::size_t{1}};
    readPort.roster.hasEnglishColumn = true;
    readPort.roster.hasKoreanColumn = true;
    readPort.roster.englishNames = { u"Alex Example" };
    readPort.roster.koreanNames = { u"\uAE40\uBBFC\uC218" };

    ClassAnalyticsEvaluationRow winterRow;
    winterRow.englishName = u"Alex Example";
    winterRow.koreanName = u"\uAE40\uBBFC\uC218";
    winterRow.scores.fill(u"A");
    readPort.evaluations[static_cast<std::size_t>(
        ClassAnalyticsEvaluation::Winter)].push_back(winterRow);

    const TestNameSemantics names;
    ClassAnalyticsPage page(
        &services,
        { .readPort = &readPort, .nameSemantics = &names });
    page.loadClass(Classroom(QStringLiteral("Analytics"), 42));
    page.show();
    QCoreApplication::processEvents();

    QLabel* const emptyLabel = page.findChild<QLabel*>(
        QStringLiteral("classAnalyticsEmptyLabel"));
    QVERIFY(emptyLabel);
    QVERIFY(!emptyLabel->isVisible());

    QTableView* const rankingTable = page.findChild<QTableView*>(
        QStringLiteral("classAnalyticsRankingTable"));
    QVERIFY(rankingTable);
    const QAbstractItemModel* const model = rankingTable->model();
    QVERIFY(model);
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(model->index(0, 1).data(Qt::DisplayRole).toString(),
        QStringLiteral("Alex Example"));
    QCOMPARE(model->index(0, 2).data(Qt::DisplayRole).toString(),
        QString::fromStdU16String(u"\uAE40\uBBFC\uC218"));
    QCOMPARE(model->index(0, 3).data(Qt::DisplayRole).toString(),
        SpeakingAnalytics::formatAverage(4.0));
    QCOMPARE(model->index(0, 3).data(AnalyticsRankingRoles::Grade).toString(),
        QStringLiteral("A"));
    QCOMPARE(model->index(0, 4).data(AnalyticsRankingRoles::Grade).toString(),
        QStringLiteral("A"));
}

QTEST_MAIN(ClassAnalyticsPageTests)

#include "class_analytics_page_tests.moc"
