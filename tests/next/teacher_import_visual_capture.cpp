#include "core/process_memory_snapshot.h"
#include "data/database/workspace_store.h"
#include "features/teacher/import/teacher_import_file_validator.h"
#include "features/teacher/ui/teacher_import_dialog.h"
#include "next/application/teacher_import_use_case.h"
#include "next/platform/workspace_store_gateway.h"
#include "next/ui/initial_setup_teacher_import_dialog.h"
#include "next/ui/qt_user_prompt_presenter.h"
#include "next/ui/teacher_import_request_mapper.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QApplication>
#include <QAbstractButton>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QRect>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStyleFactory>
#include <QScrollArea>
#include <QScrollBar>
#include <QSize>
#include <QThread>
#include <QTimer>
#include <QTranslator>
#include <QTemporaryDir>
#include <QUuid>

#include <functional>
#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace ClassMngr;

namespace
{

constexpr auto FixturePath =
    "tests/fixtures/teacher_import/sectioned_review_large.xlsx";

struct Surface final
{
    QString title;
    QString status;
    QString filePath;
    QString templateName;
    QString sourceDate;
    QString importAction;
    int candidateCount = 0;
    int checkedCandidateCount = 0;
    QStringList levels;
    QStringList candidates;
    struct Layout final
    {
        QSize dialogSize;
        QRect pathField;
        QRect browseButton;
        QRect actionBox;
        bool actionBoxVisible = false;
        bool hasCandidateScrollArea = false;
        int horizontalScrollMaximum = -1;
        bool horizontalScrollBarVisible = false;
        int candidateContentWidth = -1;
        int candidateViewportWidth = -1;
    } selectionLayout;
    Layout reviewLayout;
};

struct PromptSurface final
{
    QString title;
    QString text;
    QString acceptText;
    QString rejectText;
};

struct Memory final
{
    ProcessMemorySnapshot beforeParse;
    ProcessMemorySnapshot afterParse;
    ProcessMemorySnapshot afterReview;
    ProcessMemorySnapshot afterCancelCleanup;
    ProcessMemorySnapshot afterFailureApply;
    ProcessMemorySnapshot afterFailureCleanup;
    ProcessMemorySnapshot afterSuccessApply;
    ProcessMemorySnapshot afterSuccessCleanup;
};

QString errorText(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

bool waitFor(const std::function<bool()>& predicate, const int timeoutMilliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMilliseconds)
    {
        QApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    QApplication::processEvents(QEventLoop::AllEvents, 20);
    return predicate();
}

bool saveWidget(QWidget& widget, const QString& path)
{
    widget.adjustSize();
    QApplication::processEvents();
    return widget.grab().toImage().save(path, "PNG");
}

bool hasVisiblePromptContent(const QImage& image)
{
    if (image.isNull() || image.width() <= 0 || image.height() <= 0)
    {
        return false;
    }

    const QRgb background = image.pixel(0, 0);
    std::unordered_set<QRgb> colors;
    colors.reserve(32);
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

    const qsizetype pixelCount = image.width() * image.height();
    return colors.size() >= 8
        && contrastingPixels >= std::max<qsizetype>(80, pixelCount / 100);
}

Surface::Layout layoutMetrics(
    QWidget& dialog,
    const QString& pathObjectName,
    const QString& browseObjectName,
    const QString& scrollAreaObjectName
    )
{
    Surface::Layout metrics;
    metrics.dialogSize = dialog.size();
    if (auto* const path = dialog.findChild<QLineEdit*>(pathObjectName))
    {
        metrics.pathField = path->geometry();
    }
    if (auto* const browse = dialog.findChild<QPushButton*>(browseObjectName))
    {
        metrics.browseButton = browse->geometry();
    }
    if (auto* const actions = dialog.findChild<QDialogButtonBox*>())
    {
        metrics.actionBox = actions->geometry();
        metrics.actionBoxVisible = actions->isVisible();
    }
    if (auto* const scroll = dialog.findChild<QScrollArea*>(scrollAreaObjectName))
    {
        metrics.hasCandidateScrollArea = true;
        metrics.horizontalScrollMaximum = scroll->horizontalScrollBar()->maximum();
        metrics.horizontalScrollBarVisible =
            scroll->horizontalScrollBar()->isVisible();
        if (scroll->widget())
        {
            metrics.candidateContentWidth = scroll->widget()->width();
            metrics.candidateViewportWidth = scroll->viewport()->width();
        }
    }
    return metrics;
}

bool waitForLegacyReview(const TeacherImportDialog& dialog)
{
    const QString validState = QCoreApplication::translate(
        "TeacherImportDialog", "Status: Valid File");
    return waitFor([&dialog, &validState] {
        const auto* const status = dialog.findChild<QLabel*>(
            QStringLiteral("teacherImportValidationStatus"));
        return status && status->text() == validState;
    }, 30000);
}

bool waitForNextReview(const Next::Ui::InitialSetupTeacherImportDialog& dialog)
{
    const QString readyState = QCoreApplication::translate(
        "InitialSetupTeacherImportDialog",
        "Review the workbook, then apply the import.");
    return waitFor([&dialog, &readyState] {
        const auto* const status = dialog.findChild<QLabel*>(
            QStringLiteral("classMngrNextTeacherImportStatus"));
        return status && status->text() == readyState;
    }, 30000);
}

QString templateNameFrom(const QString& text)
{
    const QString line = text.section(QLatin1Char('\n'), 0, 0);
    return line.section(QStringLiteral(": "), 1);
}

QString dateFrom(const QString& text)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString& line : lines)
    {
        if (line.contains(QStringLiteral("202")))
        {
            const int separator = line.lastIndexOf(QStringLiteral(": "));
            if (separator >= 0)
            {
                return line.mid(separator + 2).trimmed();
            }
        }
    }
    return {};
}

bool collectLegacySurface(
    TeacherImportDialog& dialog,
    const TeacherImportPreview& preview,
    Surface* surface,
    QString* error
    )
{
    auto* const status = dialog.findChild<QLabel*>(
        QStringLiteral("teacherImportValidationStatus"));
    auto* const path = dialog.findChild<QLineEdit*>(
        QStringLiteral("teacherImportFilePath"));
    auto* const templateLabel = dialog.findChild<QLabel*>(
        QStringLiteral("teacherImportTemplateName"));
    auto* const automatic = dialog.findChild<QLabel*>(
        QStringLiteral("teacherImportAutomaticCounts"));
    auto* const import = dialog.findChild<QPushButton*>(
        QStringLiteral("teacherImportAcceptButton"));
    if (!status || !path || !templateLabel || !automatic || !import
        || !import->isEnabled())
    {
        if (error) *error = QStringLiteral("Legacy review is missing required fields.");
        return false;
    }

    surface->title = dialog.windowTitle();
    surface->status = status->text();
    surface->filePath = path->text();
    surface->templateName = templateNameFrom(templateLabel->text());
    surface->sourceDate = dateFrom(templateLabel->text());
    surface->importAction = import->text();

    for (int groupIndex = 0; groupIndex < preview.koreanGroups.size(); ++groupIndex)
    {
        const KoreanTeacherImportGroup& group =
            preview.koreanGroups.at(groupIndex);
        const QString level = group.level;
        const QString levelName = QStringLiteral("teacherImportLevel_%1").arg(level);
        auto* const levelLabel = dialog.findChild<QLabel*>(levelName);
        auto* const all = dialog.findChild<QRadioButton*>(
            QStringLiteral("teacherImportAll_%1").arg(level));
        auto* const select = dialog.findChild<QRadioButton*>(
            QStringLiteral("teacherImportSelect_%1").arg(level));
        auto* const none = dialog.findChild<QRadioButton*>(
            QStringLiteral("teacherImportNone_%1").arg(level));
        if (!levelLabel || !all || !select || !none || !all->isChecked())
        {
            if (error) *error = QStringLiteral("Legacy level choices differ from parser groups.");
            return false;
        }
        surface->levels.append(levelLabel->text());
        if (groupIndex == 0) select->setChecked(true);
        for (int candidateIndex = 0;
             candidateIndex < group.candidates.size();
             ++candidateIndex)
        {
            const QString name = QStringLiteral("teacherImportCandidateName_%1_%2")
                .arg(groupIndex).arg(candidateIndex);
            const QString room = QStringLiteral("teacherImportCandidateRoom_%1_%2")
                .arg(groupIndex).arg(candidateIndex);
            const QString checkboxName = QStringLiteral("teacherImportCandidate_%1_%2")
                .arg(groupIndex).arg(candidateIndex);
            auto* const candidateName = dialog.findChild<QLabel*>(name);
            auto* const roomLabel = dialog.findChild<QLabel*>(room);
            auto* const checkbox = dialog.findChild<QCheckBox*>(checkboxName);
            const KoreanTeacherImportCandidate& candidate =
                group.candidates.at(candidateIndex);
            if (!candidateName || !roomLabel || !checkbox
                || candidateName->text() != candidate.teacher.teacherKr
                || roomLabel->text() != candidate.teacher.roomNumber
                || checkbox->isChecked() != candidate.selectedByDefault)
            {
                if (error) *error = QStringLiteral("Legacy candidate fields differ from parser output.");
                return false;
            }
            ++surface->candidateCount;
            surface->checkedCandidateCount += checkbox->isChecked() ? 1 : 0;
            surface->candidates.append(candidateName->text()
                + QStringLiteral("|") + roomLabel->text());
        }
    }
    QApplication::processEvents();
    surface->reviewLayout = layoutMetrics(
        dialog,
        QStringLiteral("teacherImportFilePath"),
        QStringLiteral("teacherImportBrowseButton"),
        QStringLiteral("teacherImportCandidateScrollArea"));
    return true;
}

bool collectNextSurface(
    Next::Ui::InitialSetupTeacherImportDialog& dialog,
    const TeacherImportPreview& preview,
    Surface* surface,
    QString* error
    )
{
    auto* const status = dialog.findChild<QLabel*>(
        QStringLiteral("classMngrNextTeacherImportStatus"));
    auto* const path = dialog.findChild<QLineEdit*>(
        QStringLiteral("classMngrNextTeacherImportFilePath"));
    auto* const templateLabel = dialog.findChild<QLabel*>(
        QStringLiteral("classMngrNextTeacherImportPreview"));
    auto* const automatic = dialog.findChild<QLabel*>(
        QStringLiteral("classMngrNextTeacherImportAutomaticCounts"));
    auto* const apply = dialog.findChild<QPushButton*>(
        QStringLiteral("classMngrNextTeacherImportApply"));
    if (!status || !path || !templateLabel || !automatic || !apply
        || !apply->isEnabled())
    {
        if (error) *error = QStringLiteral("Next review is missing required fields.");
        return false;
    }

    surface->title = dialog.windowTitle();
    surface->status = status->text();
    surface->filePath = path->text();
    surface->templateName = templateNameFrom(templateLabel->text());
    surface->sourceDate = dateFrom(templateLabel->text());
    surface->importAction = apply->text();

    for (int groupIndex = 0; groupIndex < preview.koreanGroups.size(); ++groupIndex)
    {
        const KoreanTeacherImportGroup& group =
            preview.koreanGroups.at(groupIndex);
        const QString level = QString::number(groupIndex);
        const QString levelName = QStringLiteral("classMngrNextTeacherImportLevel_%1")
            .arg(level);
        auto* const levelLabel = dialog.findChild<QLabel*>(levelName);
        auto* const all = dialog.findChild<QRadioButton*>(
            QStringLiteral("classMngrNextTeacherImportAll_%1").arg(level));
        auto* const select = dialog.findChild<QRadioButton*>(
            QStringLiteral("classMngrNextTeacherImportSelect_%1").arg(level));
        auto* const none = dialog.findChild<QRadioButton*>(
            QStringLiteral("classMngrNextTeacherImportNone_%1").arg(level));
        if (!levelLabel || levelLabel->text() != group.level || !all || !select
            || !none || !all->isChecked())
        {
            if (error) *error = QStringLiteral("Next level choices differ from parser groups.");
            return false;
        }
        surface->levels.append(levelLabel->text());
        if (groupIndex == 0) select->setChecked(true);
        for (int candidateIndex = 0;
             candidateIndex < group.candidates.size();
             ++candidateIndex)
        {
            const QString name = QStringLiteral(
                "classMngrNextTeacherImportCandidateName_%1_%2")
                .arg(groupIndex).arg(candidateIndex);
            const QString room = QStringLiteral(
                "classMngrNextTeacherImportCandidateRoom_%1_%2")
                .arg(groupIndex).arg(candidateIndex);
            const QString checkboxName = QStringLiteral(
                "classMngrNextTeacherImportCandidate_%1_%2")
                .arg(groupIndex).arg(candidateIndex);
            auto* const candidateName = dialog.findChild<QLabel*>(name);
            auto* const roomLabel = dialog.findChild<QLabel*>(room);
            auto* const checkbox = dialog.findChild<QCheckBox*>(checkboxName);
            const KoreanTeacherImportCandidate& candidate =
                group.candidates.at(candidateIndex);
            if (!candidateName || !roomLabel || !checkbox
                || candidateName->text() != candidate.teacher.teacherKr
                || roomLabel->text() != candidate.teacher.roomNumber
                || checkbox->isChecked() != candidate.selectedByDefault)
            {
                if (error) *error = QStringLiteral("Next candidate fields differ from parser output.");
                return false;
            }
            ++surface->candidateCount;
            surface->checkedCandidateCount += checkbox->isChecked() ? 1 : 0;
            surface->candidates.append(candidateName->text()
                + QStringLiteral("|") + roomLabel->text());
        }
    }
    QApplication::processEvents();
    surface->reviewLayout = layoutMetrics(
        dialog,
        QStringLiteral("classMngrNextTeacherImportFilePath"),
        QStringLiteral("classMngrNextTeacherImportBrowse"),
        QStringLiteral("classMngrNextTeacherImportCandidateScrollArea"));
    return true;
}

bool executeSql(const QString& path, const QString& sql, QString* error)
{
    const QString connectionName = QStringLiteral("teacher-import-capture-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool succeeded = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(path);
        if (!db.open())
        {
            if (error) *error = db.lastError().text();
        }
        else
        {
            QSqlQuery query(db);
            succeeded = query.exec(sql);
            if (!succeeded && error) *error = query.lastError().text();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return succeeded;
}

bool seedWorkspace(const QString& path, QString* error)
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.open(path);
    if (!opened)
    {
        if (error) *error = opened.error().diagnostic;
        return false;
    }
    store.close();
    return true;
}

QJsonObject memoryJson(const ProcessMemorySnapshot& snapshot)
{
    return {
        {QStringLiteral("available"), snapshot.isAvailable},
        {QStringLiteral("platform"), snapshot.platform},
        {QStringLiteral("capturedAt"), snapshot.capturedAt.toString(Qt::ISODateWithMs)},
        {QStringLiteral("residentBytes"), static_cast<double>(snapshot.workingSetBytes)},
        {QStringLiteral("peakResidentBytes"), static_cast<double>(snapshot.peakWorkingSetBytes)},
        {QStringLiteral("privateUsageBytes"), static_cast<double>(snapshot.privateUsageBytes)},
        {QStringLiteral("threadCount"), static_cast<int>(snapshot.threadCount)}
    };
}

QJsonObject surfaceJson(const Surface& surface)
{
    const auto layoutJson = [](const Surface::Layout& layout) {
        return QJsonObject{
            {QStringLiteral("dialogWidth"), layout.dialogSize.width()},
            {QStringLiteral("dialogHeight"), layout.dialogSize.height()},
            {QStringLiteral("pathFieldX"), layout.pathField.x()},
            {QStringLiteral("pathFieldY"), layout.pathField.y()},
            {QStringLiteral("pathFieldWidth"), layout.pathField.width()},
            {QStringLiteral("browseX"), layout.browseButton.x()},
            {QStringLiteral("browseY"), layout.browseButton.y()},
            {QStringLiteral("browseWidth"), layout.browseButton.width()},
            {QStringLiteral("actionBoxY"), layout.actionBox.y()},
            {QStringLiteral("actionBoxHeight"), layout.actionBox.height()},
            {QStringLiteral("actionBoxVisible"), layout.actionBoxVisible},
            {QStringLiteral("hasCandidateScrollArea"), layout.hasCandidateScrollArea},
            {QStringLiteral("horizontalScrollMaximum"), layout.horizontalScrollMaximum},
            {QStringLiteral("horizontalScrollBarVisible"), layout.horizontalScrollBarVisible},
            {QStringLiteral("candidateContentWidth"), layout.candidateContentWidth},
            {QStringLiteral("candidateViewportWidth"), layout.candidateViewportWidth}
        };
    };
    return {
        {QStringLiteral("title"), surface.title},
        {QStringLiteral("validationState"), surface.status},
        {QStringLiteral("filePath"), surface.filePath},
        {QStringLiteral("templateName"), surface.templateName},
        {QStringLiteral("sourceDate"), surface.sourceDate},
        {QStringLiteral("importAction"), surface.importAction},
        {QStringLiteral("levels"), QJsonArray::fromStringList(surface.levels)},
        {QStringLiteral("candidates"), QJsonArray::fromStringList(surface.candidates)},
        {QStringLiteral("candidateCount"), surface.candidateCount},
        {QStringLiteral("checkedCandidateCount"), surface.checkedCandidateCount},
        {QStringLiteral("selectionLayout"), layoutJson(surface.selectionLayout)},
        {QStringLiteral("reviewLayout"), layoutJson(surface.reviewLayout)}
    };
}

QJsonObject promptJson(const PromptSurface& prompt)
{
    return {
        {QStringLiteral("title"), prompt.title},
        {QStringLiteral("text"), prompt.text},
        {QStringLiteral("acceptText"), prompt.acceptText},
        {QStringLiteral("rejectText"), prompt.rejectText}
    };
}

QJsonObject memoryManifest(const Memory& memory)
{
    return {
        {QStringLiteral("beforeParse"), memoryJson(memory.beforeParse)},
        {QStringLiteral("afterParse"), memoryJson(memory.afterParse)},
        {QStringLiteral("afterReview"), memoryJson(memory.afterReview)},
        {QStringLiteral("afterCancelCleanup"), memoryJson(memory.afterCancelCleanup)},
        {QStringLiteral("afterFailureApply"), memoryJson(memory.afterFailureApply)},
        {QStringLiteral("afterFailureCleanup"), memoryJson(memory.afterFailureCleanup)},
        {QStringLiteral("afterSuccessApply"), memoryJson(memory.afterSuccessApply)},
        {QStringLiteral("afterSuccessCleanup"), memoryJson(memory.afterSuccessCleanup)},
        {QStringLiteral("scopeNotes"), QStringLiteral(
            "Whole-process snapshots include Qt, SQLite, parser workers, and shared runtime state. "
            "They are observations for this run, not per-import allocation deltas or a budget.")}
    };
}

bool captureLegacyVersionPrompt(
    const QString& path,
    PromptSurface* surface,
    QString* error
    )
{
    bool captured = false;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, &timer, [&] {
        for (QWidget* widget : QApplication::topLevelWidgets())
        {
            auto* const prompt = qobject_cast<QMessageBox*>(widget);
            if (!prompt || prompt->objectName() != QStringLiteral("classmngrUserPrompt"))
            {
                continue;
            }
            auto* const accept = prompt->findChild<QPushButton*>(
                QStringLiteral("promptAcceptButton"));
            auto* const reject = prompt->findChild<QPushButton*>(
                QStringLiteral("promptRejectButton"));
            if (!accept || !reject)
            {
                continue;
            }
            surface->title = prompt->windowTitle();
            surface->text = prompt->text();
            surface->acceptText = accept->text();
            surface->rejectText = reject->text();
            if (!saveWidget(*prompt, path))
            {
                if (error) *error = QStringLiteral("Could not save version prompt capture.");
                return;
            }
            if (!hasVisiblePromptContent(QImage(path)))
            {
                if (error) *error = QStringLiteral(
                    "Legacy version prompt capture is blank or nearly uniform.");
                reject->click();
                timer.stop();
                return;
            }
            captured = true;
            reject->click();
            timer.stop();
            return;
        }
    });
    timer.start();
    QtUserPromptService presenter;
    const auto result = presenter.confirm({
        .title = QCoreApplication::translate("SidebarController", "Import Teachers"),
        .message = QCoreApplication::translate(
            "SidebarController",
            "The selected file appears to be older version than the current data. Do you wish to continue?\n"
            "    File Version: %1\n"
            "    Current Version: %2")
            .arg(QStringLiteral("2026-09-01"), QStringLiteral("2026-10-01")),
        .severity = PromptSeverity::Warning,
        .acceptText = QCoreApplication::translate("QtUserPromptService", "Continue"),
        .rejectText = QCoreApplication::translate("QtUserPromptService", "Cancel")
    });
    timer.stop();
    if (result != PromptChoice::Rejected || !captured
        || !surface->text.contains(QStringLiteral("2026-09-01"))
        || !surface->text.contains(QStringLiteral("2026-10-01")))
    {
        if (error && error->isEmpty())
        {
            *error = QStringLiteral("Version prompt state did not render with both dates.");
        }
        return false;
    }
    return true;
}

bool captureNextVersionPrompt(
    const QString& path,
    PromptSurface* surface,
    QString* error
    )
{
    bool captured = false;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, &timer, [&] {
        for (QWidget* widget : QApplication::topLevelWidgets())
        {
            auto* const prompt = qobject_cast<QMessageBox*>(widget);
            if (!prompt || prompt->property("classmngrNextPromptKind").toInt() != 1)
            {
                continue;
            }
            if (!prompt->isVisible())
            {
                continue;
            }
            surface->title = prompt->windowTitle();
            surface->text = prompt->text();
            QAbstractButton* reject = nullptr;
            for (QAbstractButton* button : prompt->buttons())
            {
                if (button->property("classmngrNextPromptCommand").toString()
                    == QStringLiteral("accept"))
                {
                    surface->acceptText = button->text();
                }
                else if (button->property("classmngrNextPromptCommand").toString()
                         == QStringLiteral("decline"))
                {
                    surface->rejectText = button->text();
                    reject = button;
                }
            }
            if (!reject || surface->acceptText.isEmpty())
            {
                continue;
            }
            timer.stop();
            const QPointer<QMessageBox> guardedPrompt(prompt);
            const QPointer<QAbstractButton> guardedReject(reject);
            QTimer::singleShot(0, &timer, [&, guardedPrompt, guardedReject] {
                if (!guardedPrompt)
                {
                    if (error) *error = QStringLiteral(
                        "Next version prompt closed before capture.");
                    return;
                }

                guardedPrompt->ensurePolished();
                if (QLayout* const promptLayout = guardedPrompt->layout())
                {
                    promptLayout->activate();
                }
                guardedPrompt->repaint();
                QApplication::processEvents(
                    QEventLoop::AllEvents | QEventLoop::ExcludeUserInputEvents);
                const QImage image = guardedPrompt->grab().toImage();
                if (image.isNull() || !image.save(path, "PNG"))
                {
                    if (error) *error = QStringLiteral(
                        "Could not save Next version prompt capture.");
                }
                else if (!hasVisiblePromptContent(QImage(path)))
                {
                    if (error) *error = QStringLiteral(
                        "Next version prompt capture is blank or nearly uniform.");
                }
                else
                {
                    captured = true;
                }

                if (guardedReject)
                {
                    guardedReject->click();
                }
            });
            return;
        }
    });
    timer.start();
    Next::Ui::QtUserPromptPresenter presenter;
    const auto result = presenter.confirm({
        .title = QCoreApplication::translate("SidebarController", "Import Teachers")
            .toUtf8().toStdString(),
        .message = QCoreApplication::translate(
            "SidebarController",
            "The selected file appears to be older version than the current data. Do you wish to continue?\n"
            "    File Version: %1\n"
            "    Current Version: %2")
            .arg(QStringLiteral("2026-09-01"), QStringLiteral("2026-10-01"))
            .toUtf8().toStdString(),
        .severity = Next::Application::PromptSeverity::Warning,
        .acceptText = QCoreApplication::translate(
            "QtUserPromptPresenter", "Continue").toUtf8().toStdString(),
        .declineText = QCoreApplication::translate(
            "QtUserPromptPresenter", "Cancel").toUtf8().toStdString(),
        .defaultAction = Next::Application::ConfirmationAction::Decline,
        .escapeAction = Next::Application::ConfirmationAction::Decline
    });
    timer.stop();
    if (!result || result.value() != Next::Application::ConfirmationResponse::Declined
        || !captured || !surface->text.contains(QStringLiteral("2026-09-01"))
        || !surface->text.contains(QStringLiteral("2026-10-01")))
    {
        if (error && error->isEmpty())
        {
            *error = QStringLiteral("Next version prompt state did not render with both dates.");
        }
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    if (application.arguments().size() != 4)
    {
        qCritical("usage: teacher-import-capture <output-dir> <en_US|ko_KR> <light|dark>");
        return 2;
    }
    const QString outputDirectory = application.arguments().at(1);
    const QString locale = application.arguments().at(2);
    const QString theme = application.arguments().at(3);
    if (!((locale == QStringLiteral("en_US") && theme == QStringLiteral("light"))
          || (locale == QStringLiteral("ko_KR") && theme == QStringLiteral("dark"))))
    {
        qCritical("capture accepts English/light or Korean/dark");
        return 2;
    }

    QLocale::setDefault(QLocale(locale));
    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    const QString resourceRoot = QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
    const QString generatedRoot = QFileInfo(
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG)).absolutePath();
    QTranslator translator;
    if (!translator.load(QDir(generatedRoot).filePath(
            QStringLiteral("assets/translations/ClassMngr_%1.qm").arg(locale))))
    {
        qCritical().noquote() << "Could not load translation catalog for" << locale;
        return 1;
    }
    application.installTranslator(&translator);

    QFile stylesheet(QDir(resourceRoot).filePath(
        QStringLiteral("assets/styles/%1.qss").arg(theme)));
    if (!stylesheet.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qCritical("Could not load capture theme stylesheet");
        return 1;
    }
    application.setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create capture output directory");
        return 1;
    }

    QString error;
    Memory memory;
    PlatformProcessMemorySnapshotProvider memoryProvider;
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
    {
        qCritical("Could not create capture workspace");
        return 1;
    }
    const QString databasePath = temporaryDirectory.filePath(
        QStringLiteral("teacher-import-capture.tps"));
    if (!seedWorkspace(databasePath, &error))
    {
        qCritical().noquote() << error;
        return 1;
    }
    Next::Platform::WorkspaceStoreGateway gateway;
    const auto opened = gateway.openWorkspace({
        .location = Next::Application::WorkspaceLocation(
            databasePath.toUtf8().toStdString())
    });
    if (!opened)
    {
        qCritical().noquote() << errorText(opened.error().message);
        return 1;
    }

    memory.beforeParse = memoryProvider.snapshot();
    const QString workbookPath = QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR))
        .filePath(QString::fromLatin1(FixturePath));
    TeacherImportFileValidation parsed = validateTeacherImportFile(workbookPath);
    if (!parsed.isValid())
    {
        qCritical().noquote() << "Capture workbook did not validate:"
                              << parsed.diagnostics.join(QLatin1Char('\n'));
        return 1;
    }
    memory.afterParse = memoryProvider.snapshot();
    Surface legacy;
    Surface next;
    const QString legacySelectionPath = QDir(outputDirectory).filePath(
        QStringLiteral("legacy_selection.png"));
    const QString legacyReviewPath = QDir(outputDirectory).filePath(
        QStringLiteral("legacy_review.png"));
    {
        TeacherImportDialog dialog;
        dialog.show();
        QApplication::processEvents();
        if (!saveWidget(dialog, legacySelectionPath))
        {
            qCritical("Could not save legacy selection surface");
            return 1;
        }
        legacy.selectionLayout = layoutMetrics(
            dialog,
            QStringLiteral("teacherImportFilePath"),
            QStringLiteral("teacherImportBrowseButton"),
            QStringLiteral("teacherImportCandidateScrollArea"));
        dialog.setFilePath(workbookPath);
        if (!waitForLegacyReview(dialog))
        {
            qCritical("Legacy importer did not reach a valid workbook review");
            return 1;
        }
        if (!collectLegacySurface(dialog, parsed.preview, &legacy, &error))
        {
            qCritical().noquote() << error;
            return 1;
        }
        if (!saveWidget(dialog, legacyReviewPath))
        {
            qCritical("Could not save legacy review surface");
            return 1;
        }
    }
    QApplication::processEvents();

    const QString nextSelectionPath = QDir(outputDirectory).filePath(
        QStringLiteral("next_selection.png"));
    const QString nextReviewPath = QDir(outputDirectory).filePath(
        QStringLiteral("next_review.png"));
    Next::Ui::InitialSetupTeacherImportActions actions;
    actions.returnReviewRequestOnly = true;
    actions.validationTimeoutMilliseconds = [] { return 30000; };
    {
        QPointer<Next::Ui::InitialSetupTeacherImportDialog> canceledReview;
        {
            auto dialog = std::make_unique<Next::Ui::InitialSetupTeacherImportDialog>(actions);
            canceledReview = dialog.get();
            dialog->show();
            dialog->setFilePath(workbookPath);
            if (!waitForNextReview(*dialog))
            {
                qCritical("Cancel lifecycle review did not load");
                return 1;
            }
            dialog->reject();
        }
        QApplication::processEvents();
        if (!canceledReview.isNull())
        {
            qCritical("Canceled workbook review dialog was not released");
            return 1;
        }
        memory.afterCancelCleanup = memoryProvider.snapshot();
    }

    std::optional<Next::Application::TeacherImportUseCaseRequest> failedRequest;
    std::optional<Next::Application::TeacherImportUseCaseRequest> successRequest;
    QPointer<Next::Ui::InitialSetupTeacherImportDialog> failedReview;
    {
        auto dialog = std::make_unique<Next::Ui::InitialSetupTeacherImportDialog>(actions);
        failedReview = dialog.get();
        dialog->show();
        dialog->setFilePath(workbookPath);
        if (!waitForNextReview(*dialog))
        {
            qCritical("Failure lifecycle review did not load");
            return 1;
        }
        auto* const apply = dialog->findChild<QPushButton*>(
            QStringLiteral("classMngrNextTeacherImportApply"));
        if (!apply || !apply->isEnabled())
        {
            qCritical("Failure lifecycle review has no enabled Apply action");
            return 1;
        }
        apply->click();
        if (dialog->result() != QDialog::Accepted
            || !dialog->reviewedRequest().has_value())
        {
            qCritical("Normal review did not return a failed-import request");
            return 1;
        }
        failedRequest = *dialog->reviewedRequest();
    }
    if (!failedReview.isNull())
    {
        qCritical("Failed-import review dialog was not released");
        return 1;
    }

    QPointer<Next::Ui::InitialSetupTeacherImportDialog> successfulReview;
    {
        auto dialog = std::make_unique<Next::Ui::InitialSetupTeacherImportDialog>(actions);
        successfulReview = dialog.get();
        dialog->show();
        QApplication::processEvents();
        if (!saveWidget(*dialog, nextSelectionPath))
        {
            qCritical("Could not save Next selection surface");
            return 1;
        }
        next.selectionLayout = layoutMetrics(
            *dialog,
            QStringLiteral("classMngrNextTeacherImportFilePath"),
            QStringLiteral("classMngrNextTeacherImportBrowse"),
            QStringLiteral("classMngrNextTeacherImportCandidateScrollArea"));
        dialog->setFilePath(workbookPath);
        if (!waitForNextReview(*dialog))
        {
            qCritical("Next importer did not reach workbook review");
            return 1;
        }
        if (!collectNextSurface(*dialog, parsed.preview, &next, &error))
        {
            qCritical().noquote() << error;
            return 1;
        }
        if (!saveWidget(*dialog, nextReviewPath))
        {
            qCritical("Could not save Next review surface");
            return 1;
        }
        memory.afterReview = memoryProvider.snapshot();
        auto* const apply = dialog->findChild<QPushButton*>(
            QStringLiteral("classMngrNextTeacherImportApply"));
        if (!apply || !apply->isEnabled())
        {
            qCritical("Captured Next review has no enabled Apply action");
            return 1;
        }
        apply->click();
        if (dialog->result() != QDialog::Accepted
            || !dialog->reviewedRequest().has_value())
        {
            qCritical("Normal review did not return a success request");
            return 1;
        }
        successRequest = *dialog->reviewedRequest();
    }
    if (!successfulReview.isNull())
    {
        qCritical("Successful-import review dialog was not released");
        return 1;
    }

    if (legacy.templateName != next.templateName
        || legacy.sourceDate != next.sourceDate
        || legacy.filePath != next.filePath
        || legacy.levels != next.levels
        || legacy.candidates != next.candidates
        || legacy.candidateCount != next.candidateCount
        || legacy.checkedCandidateCount != next.checkedCandidateCount
        || legacy.title != next.title
        || legacy.importAction != next.importAction
        || legacy.templateName != parsed.templateName
        || legacy.sourceDate != parsed.sourceDate.toString(Qt::ISODate)
        || legacy.candidateCount
            != [&parsed] {
                int count = 0;
                for (const auto& group : parsed.preview.koreanGroups)
                    count += group.candidates.size();
                return count;
            }())
    {
        qCritical("Legacy and Next importer field/state parity check failed");
        return 1;
    }

    const QSize expectedReviewSize(
        460,
        locale == QStringLiteral("ko_KR") ? 477 : 501);
    const auto validPathLayout = [](const Surface::Layout& layout) {
        return layout.pathField.width() >= 250
            && layout.pathField.center().y() == layout.browseButton.center().y()
            && layout.pathField.right() < layout.browseButton.left()
            && layout.browseButton.right() <= layout.dialogSize.width() - 15;
    };
    const auto scrollAreaUnavailableHorizontally = [](
        const Surface::Layout& layout) {
        return layout.hasCandidateScrollArea
            && !layout.horizontalScrollBarVisible
            && layout.horizontalScrollMaximum == 0
            && layout.candidateContentWidth <= layout.candidateViewportWidth;
    };
    const QSize expectedSelectionSize(
        460,
        locale == QStringLiteral("ko_KR") ? 212 : 218);
    if (legacy.selectionLayout.dialogSize != expectedSelectionSize
        || next.selectionLayout.dialogSize != expectedSelectionSize
        || legacy.reviewLayout.dialogSize != expectedReviewSize
        || next.reviewLayout.dialogSize != expectedReviewSize
        || !validPathLayout(legacy.selectionLayout)
        || !validPathLayout(next.selectionLayout)
        || !validPathLayout(legacy.reviewLayout)
        || !validPathLayout(next.reviewLayout)
        || legacy.selectionLayout.pathField.x()
            != next.selectionLayout.pathField.x()
        || legacy.selectionLayout.pathField
            != next.selectionLayout.pathField
        || legacy.reviewLayout.pathField.width()
            != next.reviewLayout.pathField.width()
        || legacy.reviewLayout.pathField
            != next.reviewLayout.pathField
        || legacy.selectionLayout.browseButton
            != next.selectionLayout.browseButton
        || legacy.reviewLayout.browseButton
            != next.reviewLayout.browseButton
        || legacy.selectionLayout.actionBox
            != next.selectionLayout.actionBox
        || legacy.reviewLayout.actionBox
            != next.reviewLayout.actionBox
        || legacy.selectionLayout.browseButton.width()
            != next.selectionLayout.browseButton.width()
        || !legacy.selectionLayout.actionBoxVisible
        || !next.selectionLayout.actionBoxVisible
        || legacy.selectionLayout.actionBox.bottom()
            > legacy.selectionLayout.dialogSize.height()
        || next.selectionLayout.actionBox.bottom()
            > next.selectionLayout.dialogSize.height()
        || !scrollAreaUnavailableHorizontally(legacy.reviewLayout)
        || !scrollAreaUnavailableHorizontally(next.reviewLayout))
    {
        qCritical().noquote()
            << "Importer dialog geometry/layout parity failed. Expected selection"
            << expectedSelectionSize << "and review" << expectedReviewSize
            << "but received legacy selection"
            << legacy.selectionLayout.dialogSize << "Next selection"
            << next.selectionLayout.dialogSize << "legacy review"
            << legacy.reviewLayout.dialogSize << "Next review"
            << next.reviewLayout.dialogSize << "legacy path width"
            << legacy.reviewLayout.pathField.width() << "Next path width"
            << next.reviewLayout.pathField.width() << "legacy horizontal range"
            << legacy.reviewLayout.horizontalScrollMaximum << "Next horizontal range"
            << next.reviewLayout.horizontalScrollMaximum << "legacy content/viewport"
            << legacy.reviewLayout.candidateContentWidth << "/"
            << legacy.reviewLayout.candidateViewportWidth << "Next content/viewport"
            << next.reviewLayout.candidateContentWidth << "/"
            << next.reviewLayout.candidateViewportWidth << "selection path y"
            << legacy.selectionLayout.pathField.y() << "/"
            << next.selectionLayout.pathField.y() << "selection browse y"
            << legacy.selectionLayout.browseButton.y() << "/"
            << next.selectionLayout.browseButton.y() << "action visible"
            << legacy.selectionLayout.actionBoxVisible << "/"
            << next.selectionLayout.actionBoxVisible << "action bottom"
            << legacy.selectionLayout.actionBox.bottom() << "/"
            << next.selectionLayout.actionBox.bottom() << "selection path x/width"
            << legacy.selectionLayout.pathField.x() << "/"
            << legacy.selectionLayout.pathField.width() << "vs"
            << next.selectionLayout.pathField.x() << "/"
            << next.selectionLayout.pathField.width() << "browse width"
            << legacy.selectionLayout.browseButton.width() << "/"
            << next.selectionLayout.browseButton.width() << "review path x/y/width"
            << legacy.reviewLayout.pathField.x() << "/"
            << legacy.reviewLayout.pathField.y() << "/"
            << legacy.reviewLayout.pathField.width() << "vs"
            << next.reviewLayout.pathField.x() << "/"
            << next.reviewLayout.pathField.y() << "/"
            << next.reviewLayout.pathField.width() << "valid path layouts"
            << validPathLayout(legacy.selectionLayout) << "/"
            << validPathLayout(next.selectionLayout) << "/"
            << validPathLayout(legacy.reviewLayout) << "/"
            << validPathLayout(next.reviewLayout) << "scroll validity"
            << scrollAreaUnavailableHorizontally(legacy.reviewLayout) << "/"
            << scrollAreaUnavailableHorizontally(next.reviewLayout);
        return 1;
    }

    // Normal mode returns the review request. A synthetic storage trigger
    // forces a transactional failure, then a separately reviewed request is
    // applied successfully after the trigger is removed.
    if (!executeSql(databasePath,
            QStringLiteral("CREATE TRIGGER fail_capture_teacher_import_gs "
                           "BEFORE INSERT ON gs_team BEGIN "
                           "SELECT RAISE(ABORT, 'capture failure'); END"),
            &error))
    {
        qCritical().noquote() << error;
        return 1;
    }
    const auto failedApply = gateway.applyTeacherImportUseCase(*failedRequest);
    if (failedApply)
    {
        qCritical("Failure injection unexpectedly allowed import to apply");
        return 1;
    }
    memory.afterFailureApply = memoryProvider.snapshot();
    failedRequest.reset();
    if (!executeSql(databasePath,
            QStringLiteral("DROP TRIGGER fail_capture_teacher_import_gs"), &error))
    {
        qCritical().noquote() << error;
        return 1;
    }
    memory.afterFailureCleanup = memoryProvider.snapshot();
    const auto successfulApply = gateway.applyTeacherImportUseCase(*successRequest);
    if (!successfulApply)
    {
        qCritical().noquote() << "Successful apply retry failed:"
                              << QString::fromStdU16String(
                                     successfulApply.error().persistenceFailure.message);
        return 1;
    }
    memory.afterSuccessApply = memoryProvider.snapshot();
    successRequest.reset();
    const QString validatedTemplateName = parsed.templateName;
    const QString validatedSourceDate = parsed.sourceDate.toString(Qt::ISODate);
    const int automaticNativeCount = parsed.previewCounts.nativeEnglishTeachers;
    const int automaticGsCount = parsed.previewCounts.gsTeamMembers;
    parsed = {};
    QApplication::processEvents();
    memory.afterSuccessCleanup = memoryProvider.snapshot();

    PromptSurface legacyVersionPrompt;
    if (!captureLegacyVersionPrompt(
            QDir(outputDirectory).filePath(
                QStringLiteral("legacy_version_prompt.png")),
            &legacyVersionPrompt,
            &error))
    {
        qCritical().noquote() << error;
        return 1;
    }
    PromptSurface nextVersionPrompt;
    if (!captureNextVersionPrompt(
            QDir(outputDirectory).filePath(
                QStringLiteral("next_version_prompt.png")),
            &nextVersionPrompt,
            &error))
    {
        qCritical().noquote() << error;
        return 1;
    }
    if (legacyVersionPrompt.title != nextVersionPrompt.title
        || legacyVersionPrompt.text != nextVersionPrompt.text
        || legacyVersionPrompt.acceptText != nextVersionPrompt.acceptText
        || legacyVersionPrompt.rejectText != nextVersionPrompt.rejectText)
    {
        qCritical("Legacy and Next version confirmation prompts differ");
        return 1;
    }

    const auto closed = gateway.closeWorkspace({
        .session = opened.value()
    });
    if (!closed)
    {
        qCritical().noquote() << errorText(closed.error().message);
        return 1;
    }

    const QJsonObject manifest{
        {QStringLiteral("task"), QStringLiteral("phase7_019_normal_teacher_import_implementation")},
        {QStringLiteral("locale"), locale},
        {QStringLiteral("theme"), theme},
        {QStringLiteral("fixture"), QString::fromLatin1(FixturePath)},
        {QStringLiteral("validatedTemplate"), validatedTemplateName},
        {QStringLiteral("sourceDate"), validatedSourceDate},
        {QStringLiteral("automaticNativeCount"), automaticNativeCount},
        {QStringLiteral("automaticGsCount"), automaticGsCount},
        {QStringLiteral("legacy"), surfaceJson(legacy)},
        {QStringLiteral("next"), surfaceJson(next)},
        {QStringLiteral("legacyVersionPrompt"), promptJson(legacyVersionPrompt)},
        {QStringLiteral("nextVersionPrompt"), promptJson(nextVersionPrompt)},
        {QStringLiteral("fieldStateParityAssertions"), QJsonArray{
            QStringLiteral("template name and source date equal parser output"),
            QStringLiteral("file path, Korean levels, candidates, rooms, and default selections match"),
            QStringLiteral("all review fields and candidate counts match between legacy and Next"),
            QStringLiteral("normal dialog title and import action match legacy localization"),
            QStringLiteral("both dialogs expose an enabled import/apply action for the valid workbook"),
            QStringLiteral("selection and review dialog dimensions equal legacy extents: 460x218 English and 460x212 Korean selection; 460x501 English and 460x477 Korean review"),
            QStringLiteral("path field, browse button, and action-box rectangles exactly match legacy in selection and review states; the path field is at least 250 pixels wide"),
            QStringLiteral("candidate scroll area has no horizontal range or visible horizontal scrollbar and content fits its viewport"),
            QStringLiteral("legacy and Next version confirmation prompts match in title, message, dates, and button labels")
        }},
        {QStringLiteral("memorySnapshots"), memoryManifest(memory)},
        {QStringLiteral("memoryLifecycle"), QJsonObject{
            {QStringLiteral("failureInjectedAndRolledBack"), true},
            {QStringLiteral("successRetriedAfterFailure"), true},
            {QStringLiteral("reviewDialogsDestroyedBeforeCleanupSnapshots"), true}
        }},
        {QStringLiteral("captures"), QJsonArray{
            QStringLiteral("legacy_selection.png"), QStringLiteral("legacy_review.png"),
            QStringLiteral("next_selection.png"), QStringLiteral("next_review.png"),
            QStringLiteral("legacy_version_prompt.png"), QStringLiteral("next_version_prompt.png")
        }},
        {QStringLiteral("memoryLimitations"), QStringLiteral(
            "The platform snapshot provider reports whole-process resident/private values. "
            "Qt, SQLite, the workbook parser, and fixture setup contribute to each sample; "
            "these values do not isolate importer memory and do not establish a budget.")}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(QStringLiteral("manifest.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented)) < 0)
    {
        qCritical("Could not write import capture manifest");
        return 1;
    }
    return 0;
}
