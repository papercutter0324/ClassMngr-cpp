#include "core/process_memory_snapshot.h"
#include "features/teacher/ui/upcoming_birthdays_dialog.h"
#include "features/teacher/upcoming_birthday_schedule.h"
#include "next/application/upcoming_birthday_schedule_use_case.h"
#include "next/ui/upcoming_birthdays_dialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDate>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QRegularExpression>
#include <QStyleFactory>
#include <QTemporaryDir>
#include <QTranslator>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_set>
#include <vector>

using namespace ClassMngr;

namespace
{

struct Surface final
{
    QSize size;
    QString title;
    QString todayTitle;
    QString thisWeekTitle;
    QString nextWeekTitle;
    QString thisWeekEmpty;
    QString nextWeekEmpty;
    QString dismissalText;
    QString closeText;
    QStringList visibleNames;
    int todayEntries = 0;
    int thisWeekEntries = 0;
    int nextWeekEntries = 0;
    bool todayVisible = false;
    bool thisWeekVisible = false;
    bool nextWeekVisible = false;
    bool dismissalChecked = true;
};

QString readLabel(const QWidget& widget, const QString& objectName)
{
    const auto* label = widget.findChild<QLabel*>(objectName);
    return label ? label->text() : QString{};
}

Surface legacyMetrics(const UpcomingBirthdaysDialog& dialog)
{
    Surface result;
    result.size = dialog.size();
    result.title = dialog.windowTitle();
    result.todayTitle = readLabel(dialog, QStringLiteral("upcomingBirthdaysTodayTitle"));
    result.thisWeekTitle = readLabel(dialog, QStringLiteral("upcomingBirthdaysThisWeekTitle"));
    result.nextWeekTitle = readLabel(dialog, QStringLiteral("upcomingBirthdaysNextWeekTitle"));
    result.thisWeekEmpty = readLabel(dialog, QStringLiteral("upcomingBirthdaysThisWeekEmpty"));
    result.nextWeekEmpty = readLabel(dialog, QStringLiteral("upcomingBirthdaysNextWeekEmpty"));
    result.todayVisible = !dialog.findChild<QWidget*>(
        QStringLiteral("upcomingBirthdaysTodaySection"))->isHidden();
    result.thisWeekVisible = !dialog.findChild<QWidget*>(
        QStringLiteral("upcomingBirthdaysThisWeekSection"))->isHidden();
    result.nextWeekVisible = !dialog.findChild<QWidget*>(
        QStringLiteral("upcomingBirthdaysNextWeekSection"))->isHidden();
    result.todayEntries = dialog.findChildren<QWidget*>(
        QRegularExpression(QStringLiteral("^upcomingBirthdaysTodayEntry\\d+$"))
        ).size();
    result.thisWeekEntries = dialog.findChildren<QWidget*>(
        QRegularExpression(QStringLiteral("^upcomingBirthdaysThisWeekEntry\\d+$"))
        ).size();
    result.nextWeekEntries = dialog.findChildren<QWidget*>(
        QRegularExpression(QStringLiteral("^upcomingBirthdaysNextWeekEntry\\d+$"))
        ).size();
    if (auto* check = dialog.findChild<QCheckBox*>(
            QStringLiteral("upcomingBirthdaysDismissForTodayCheck")))
    {
        result.dismissalText = check->text();
        result.dismissalChecked = check->isChecked();
    }
    if (auto* buttons = dialog.findChild<QDialogButtonBox*>())
    {
        if (auto* close = buttons->button(QDialogButtonBox::Close))
        {
            result.closeText = close->text();
        }
    }
    for (QLabel* label : dialog.findChildren<QLabel*>(
             QRegularExpression(QStringLiteral("^upcomingBirthdays.*Entry\\d+Name$"))))
    {
        result.visibleNames.push_back(label->text());
    }
    return result;
}

Surface nextMetrics(const Next::Ui::UpcomingBirthdaysDialog& dialog)
{
    Surface result;
    result.size = dialog.size();
    result.title = dialog.windowTitle();
    result.todayTitle = readLabel(dialog, QStringLiteral("nextUpcomingBirthdaysTodayTitle"));
    result.thisWeekTitle = readLabel(dialog, QStringLiteral("nextUpcomingBirthdaysThisWeekTitle"));
    result.nextWeekTitle = readLabel(dialog, QStringLiteral("nextUpcomingBirthdaysNextWeekTitle"));
    result.thisWeekEmpty = readLabel(dialog, QStringLiteral("nextUpcomingBirthdaysThisWeekEmpty"));
    result.nextWeekEmpty = readLabel(dialog, QStringLiteral("nextUpcomingBirthdaysNextWeekEmpty"));
    result.todayVisible = !dialog.findChild<QWidget*>(
        QStringLiteral("nextUpcomingBirthdaysTodaySection"))->isHidden();
    result.thisWeekVisible = !dialog.findChild<QWidget*>(
        QStringLiteral("nextUpcomingBirthdaysThisWeekSection"))->isHidden();
    result.nextWeekVisible = !dialog.findChild<QWidget*>(
        QStringLiteral("nextUpcomingBirthdaysNextWeekSection"))->isHidden();
    result.todayEntries = dialog.findChildren<QWidget*>(
        QRegularExpression(QStringLiteral("^nextUpcomingBirthdaysTodayEntry\\d+$"))
        ).size();
    result.thisWeekEntries = dialog.findChildren<QWidget*>(
        QRegularExpression(QStringLiteral("^nextUpcomingBirthdaysThisWeekEntry\\d+$"))
        ).size();
    result.nextWeekEntries = dialog.findChildren<QWidget*>(
        QRegularExpression(QStringLiteral("^nextUpcomingBirthdaysNextWeekEntry\\d+$"))
        ).size();
    if (auto* check = dialog.findChild<QCheckBox*>(
            QStringLiteral("nextUpcomingBirthdaysDismissForTodayCheck")))
    {
        result.dismissalText = check->text();
        result.dismissalChecked = check->isChecked();
    }
    if (auto* buttons = dialog.findChild<QDialogButtonBox*>())
    {
        if (auto* close = buttons->button(QDialogButtonBox::Close))
        {
            result.closeText = close->text();
        }
    }
    for (QLabel* label : dialog.findChildren<QLabel*>(
             QRegularExpression(QStringLiteral("^nextUpcomingBirthdays.*Entry\\d+Name$"))))
    {
        result.visibleNames.push_back(label->text());
    }
    return result;
}

QJsonObject surfaceJson(const Surface& surface)
{
    return {
        {QStringLiteral("size"), QJsonArray{surface.size.width(), surface.size.height()}},
        {QStringLiteral("title"), surface.title},
        {QStringLiteral("todayTitle"), surface.todayTitle},
        {QStringLiteral("thisWeekTitle"), surface.thisWeekTitle},
        {QStringLiteral("nextWeekTitle"), surface.nextWeekTitle},
        {QStringLiteral("thisWeekEmpty"), surface.thisWeekEmpty},
        {QStringLiteral("nextWeekEmpty"), surface.nextWeekEmpty},
        {QStringLiteral("todayVisible"), surface.todayVisible},
        {QStringLiteral("thisWeekVisible"), surface.thisWeekVisible},
        {QStringLiteral("nextWeekVisible"), surface.nextWeekVisible},
        {QStringLiteral("todayEntries"), surface.todayEntries},
        {QStringLiteral("thisWeekEntries"), surface.thisWeekEntries},
        {QStringLiteral("nextWeekEntries"), surface.nextWeekEntries},
        {QStringLiteral("visibleNames"), QJsonArray::fromStringList(surface.visibleNames)},
        {QStringLiteral("dismissalText"), surface.dismissalText},
        {QStringLiteral("dismissalChecked"), surface.dismissalChecked},
        {QStringLiteral("closeText"), surface.closeText}
    };
}

QJsonObject memoryJson(const ProcessMemorySnapshot& snapshot)
{
    return {
        {QStringLiteral("available"), snapshot.isAvailable},
        {QStringLiteral("platform"), snapshot.platform},
        {QStringLiteral("workingSetBytes"), static_cast<qint64>(snapshot.workingSetBytes)},
        {QStringLiteral("peakWorkingSetBytes"), static_cast<qint64>(snapshot.peakWorkingSetBytes)},
        {QStringLiteral("privateUsageBytes"), static_cast<qint64>(snapshot.privateUsageBytes)},
        {QStringLiteral("privateWorkingSetBytes"), static_cast<qint64>(snapshot.privateWorkingSetBytes)},
        {QStringLiteral("handleCount"), static_cast<qint64>(snapshot.handleCount)},
        {QStringLiteral("threadCount"), static_cast<qint64>(snapshot.threadCount)},
        {QStringLiteral("capturedAt"), snapshot.capturedAt.toString(Qt::ISODateWithMs)}
    };
}

bool hasVisibleContent(const QImage& image)
{
    if (image.isNull())
    {
        return false;
    }
    const QRgb background = image.pixel(0, 0);
    std::unordered_set<QRgb> colors;
    qsizetype contrastingPixels = 0;
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            const QRgb pixel = image.pixel(x, y);
            colors.insert(pixel);
            const int difference = std::abs(qRed(pixel) - qRed(background))
                + std::abs(qGreen(pixel) - qGreen(background))
                + std::abs(qBlue(pixel) - qBlue(background));
            if (difference >= 48)
            {
                ++contrastingPixels;
            }
        }
    }
    const qsizetype pixels = image.width() * image.height();
    return colors.size() >= 8
        && contrastingPixels >= std::max<qsizetype>(80, pixels / 100);
}

bool saveCapture(QWidget& widget, const QString& path)
{
    widget.show();
    QApplication::processEvents();
    QApplication::processEvents();
    const QImage image = widget.grab().toImage();
    widget.hide();
    return hasVisibleContent(image) && image.save(path, "PNG");
}

UpcomingBirthdaySchedule legacySchedule()
{
    Teacher koreanToday;
    koreanToday.teacherKr = QStringLiteral("\uAE40\uC624\uB298");
    koreanToday.preferredName = QStringLiteral("\uC624\uB298 \uC0DD\uC77C");
    koreanToday.birthday = QStringLiteral("07-10");

    Teacher koreanWeek;
    koreanWeek.teacherKr = QStringLiteral("\uAE40\uC8FC");
    koreanWeek.teacherEn = QStringLiteral("Week Teacher");
    koreanWeek.birthday = QStringLiteral("07-12");

    NativeEnglishTeacher nativeNext;
    nativeNext.name = QStringLiteral("Next Native");
    nativeNext.position = QStringLiteral("NET");
    nativeNext.birthday = QStringLiteral("07-13");

    GsTeamMember gsNext;
    gsNext.name = QStringLiteral("Next GS");
    gsNext.position = QStringLiteral("Coordinator");
    gsNext.birthday = QStringLiteral("07-19");

    return UpcomingBirthdaySchedule::build(
        {koreanToday, koreanWeek},
        {nativeNext},
        {gsNext},
        QDate(2026, 7, 10)
        );
}

int compareLocaleAwareText(
    std::u16string_view left,
    std::u16string_view right
    );

Next::Application::UpcomingBirthdaySchedule nextSchedule()
{
    using namespace Next::Application;
    std::vector<UpcomingBirthdayCandidate> candidates{
        {
            .birthdayMonthDay = UpcomingBirthdayMonthDayInput("07-10"),
            .displayName = u"\uC624\uB298 \uC0DD\uC77C",
            .group = UpcomingBirthdayStaffGroup::KoreanTeacher
        },
        {
            .birthdayMonthDay = UpcomingBirthdayMonthDayInput("07-12"),
            .displayName = u"Week Teacher",
            .group = UpcomingBirthdayStaffGroup::KoreanTeacher
        },
        {
            .birthdayMonthDay = UpcomingBirthdayMonthDayInput("07-13"),
            .displayName = u"Next Native",
            .position = u"NET",
            .group = UpcomingBirthdayStaffGroup::NativeEnglishTeacher
        },
        {
            .birthdayMonthDay = UpcomingBirthdayMonthDayInput("07-19"),
            .displayName = u"Next GS",
            .position = u"Coordinator",
            .group = UpcomingBirthdayStaffGroup::GsTeam
        }
    };
    const auto date = Next::Domain::GregorianDate::fromComponents(2026, 7, 10);
    return UpcomingBirthdayScheduleUseCase::build(
        candidates,
        date,
        compareLocaleAwareText
        );
}

int compareLocaleAwareText(
    const std::u16string_view left,
    const std::u16string_view right
    )
{
    return QString::localeAwareCompare(
        QString::fromUtf16(left.data(), static_cast<qsizetype>(left.size())),
        QString::fromUtf16(right.data(), static_cast<qsizetype>(right.size()))
        );
}

bool sameSurface(const Surface& left, const Surface& right)
{
    return left.size == QSize(520, 560)
        && right.size == QSize(520, 560)
        && left.title == right.title
        && left.todayTitle == right.todayTitle
        && left.thisWeekTitle == right.thisWeekTitle
        && left.nextWeekTitle == right.nextWeekTitle
        && left.thisWeekEmpty == right.thisWeekEmpty
        && left.nextWeekEmpty == right.nextWeekEmpty
        && left.todayVisible && right.todayVisible
        && left.thisWeekVisible && right.thisWeekVisible
        && left.nextWeekVisible && right.nextWeekVisible
        && left.todayEntries == 1 && right.todayEntries == 1
        && left.thisWeekEntries == 1 && right.thisWeekEntries == 1
        && left.nextWeekEntries == 2 && right.nextWeekEntries == 2
        && left.visibleNames == right.visibleNames
        && left.dismissalText == right.dismissalText
        && !left.dismissalChecked && !right.dismissalChecked
        && left.closeText == right.closeText;
}

} // namespace

int main(int argc, char* argv[])
{
    QTemporaryDir settingsRoot;
    if (!settingsRoot.isValid()
        || !qputenv("CLASSMNGR_SETTINGS_ROOT", settingsRoot.path().toUtf8()))
    {
        qCritical("Could not create isolated capture preferences");
        return 1;
    }
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("ClassMngrNextBirthdayCapture"));
    if (argc != 4)
    {
        qCritical("Usage: capture <output-dir> <locale> <theme>");
        return 2;
    }

    const QString outputDirectory = QString::fromLocal8Bit(argv[1]);
    const QString locale = QString::fromLocal8Bit(argv[2]);
    const QString theme = QString::fromLocal8Bit(argv[3]);
    QLocale::setDefault(QLocale(locale));
    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    const QString resourceRoot = QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
    const QString generatedRoot = QFileInfo(
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG)).absolutePath();
    QTranslator translator;
    if (!translator.load(QDir(generatedRoot).filePath(
            QStringLiteral("assets/translations/ClassMngr_%1.qm").arg(locale))))
    {
        qCritical().noquote() << "Could not load translation:" << locale;
        return 1;
    }
    application.installTranslator(&translator);

    const bool korean = locale.startsWith(QStringLiteral("ko"));
    for (const auto& [name, family] : {
             std::pair{QStringLiteral("Inter.ttc"), QStringLiteral("Inter")},
             std::pair{QStringLiteral("PretendardVariable.ttf"), QStringLiteral("Pretendard")}
         })
    {
        const QString fontPath = QDir(resourceRoot).filePath(
            QStringLiteral("assets/fonts/") + name);
        const int fontId = QFontDatabase::addApplicationFont(fontPath);
        if (fontId < 0)
        {
            qCritical().noquote() << "Could not load visual font:" << fontPath;
            return 1;
        }
        static_cast<void>(family);
    }
    QFile stylesheet(QDir(resourceRoot).filePath(
        QStringLiteral("assets/styles/%1.qss").arg(theme)));
    if (!stylesheet.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qCritical("Could not load capture theme stylesheet");
        return 1;
    }
    application.setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
    QFont applicationFont(korean ? QStringLiteral("Pretendard") : QStringLiteral("Inter"), 11);
    application.setFont(applicationFont);

    QDir().mkpath(outputDirectory);
    const QString legacyPath = QDir(outputDirectory).filePath(QStringLiteral("legacy.png"));
    const QString nextPath = QDir(outputDirectory).filePath(QStringLiteral("next.png"));
    const ProcessMemorySnapshot before = PlatformProcessMemorySnapshotProvider().snapshot();
    Surface legacy;
    Surface next;
    ProcessMemorySnapshot afterLegacy;
    ProcessMemorySnapshot afterNext;
    {
        UpcomingBirthdaysDialog dialog(legacySchedule());
        if (!saveCapture(dialog, legacyPath))
        {
            qCritical("The legacy birthday dialog capture was blank or could not be saved");
            return 1;
        }
        legacy = legacyMetrics(dialog);
        afterLegacy = PlatformProcessMemorySnapshotProvider().snapshot();
    }
    {
        Next::Ui::UpcomingBirthdaysDialog dialog(nextSchedule());
        if (!saveCapture(dialog, nextPath))
        {
            qCritical("The Next birthday dialog capture was blank or could not be saved");
            return 1;
        }
        next = nextMetrics(dialog);
        afterNext = PlatformProcessMemorySnapshotProvider().snapshot();
    }
    const ProcessMemorySnapshot afterCleanup =
        PlatformProcessMemorySnapshotProvider().snapshot();

    Surface emptyLegacy;
    Surface emptyNext;
    {
        const UpcomingBirthdaySchedule emptySchedule;
        UpcomingBirthdaysDialog dialog(emptySchedule);
        emptyLegacy = legacyMetrics(dialog);
    }
    {
        const Next::Application::UpcomingBirthdaySchedule emptySchedule;
        Next::Ui::UpcomingBirthdaysDialog dialog(emptySchedule);
        emptyNext = nextMetrics(dialog);
    }

    if (!sameSurface(legacy, next))
    {
        qCritical().noquote() << "Legacy and Next birthday dialog surfaces differ."
            << "legacy size" << legacy.size << "Next size" << next.size
            << "legacy names" << legacy.visibleNames
            << "Next names" << next.visibleNames;
        return 1;
    }
    const QString noThisWeek = QCoreApplication::translate(
        "NextUpcomingBirthdaysDialog", "No birthdays this week.");
    const QString noNextWeek = QCoreApplication::translate(
        "NextUpcomingBirthdaysDialog", "No birthdays next week.");
    if (emptyLegacy.todayVisible || emptyNext.todayVisible
        || !emptyLegacy.thisWeekVisible || !emptyNext.thisWeekVisible
        || !emptyLegacy.nextWeekVisible || !emptyNext.nextWeekVisible
        || emptyLegacy.thisWeekEmpty != noThisWeek
        || emptyNext.thisWeekEmpty != noThisWeek
        || emptyLegacy.nextWeekEmpty != noNextWeek
        || emptyNext.nextWeekEmpty != noNextWeek
        || emptyLegacy.dismissalChecked || emptyNext.dismissalChecked)
    {
        qCritical("Empty-schedule dialog sections do not match the expected legacy behavior");
        return 1;
    }

    const QJsonObject manifest{
        {QStringLiteral("task"), QStringLiteral("phase7_020_upcoming_birthdays_implementation")},
        {QStringLiteral("locale"), locale},
        {QStringLiteral("theme"), theme},
        {QStringLiteral("referenceDate"), QStringLiteral("2026-07-10")},
        {QStringLiteral("viewport"), QJsonArray{1024, 768}},
        {QStringLiteral("expectedDialogSize"), QJsonArray{520, 560}},
        {QStringLiteral("legacySurface"), surfaceJson(legacy)},
        {QStringLiteral("nextSurface"), surfaceJson(next)},
        {QStringLiteral("emptyScheduleAssertions"), QJsonObject{
             {QStringLiteral("legacy"), surfaceJson(emptyLegacy)},
             {QStringLiteral("next"), surfaceJson(emptyNext)}}},
        {QStringLiteral("processMemory"), QJsonObject{
             {QStringLiteral("before"), memoryJson(before)},
             {QStringLiteral("afterLegacyCapture"), memoryJson(afterLegacy)},
             {QStringLiteral("afterNextCapture"), memoryJson(afterNext)},
             {QStringLiteral("afterDialogCleanup"), memoryJson(afterCleanup)}}},
        {QStringLiteral("legacyCapture"), QFileInfo(legacyPath).fileName()},
        {QStringLiteral("nextCapture"), QFileInfo(nextPath).fileName()}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(QStringLiteral("manifest.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented)) < 0)
    {
        qCritical("Could not write birthday visual capture manifest");
        return 1;
    }
    return 0;
}
