#include "next/ui/roster_print_dialog.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QFile>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QSaveFile>
#include <QVBoxLayout>
#include <algorithm>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{
DialogGeometryPreferencesCallbacks requireNextRosterGeometryPreferences(
    DialogGeometryPreferencesCallbacks geometryPreferences
    )
{
    if (!geometryPreferences.read || !geometryPreferences.write)
    {
        throw std::invalid_argument(
            "Next RosterPrintDialog requires paired geometry preference callbacks"
            );
    }
    return geometryPreferences;
}

QString text(const char* source)
{
    return QCoreApplication::translate("NextRosterOutput", source);
}

QPushButton* button(
    const char* label,
    const char* name,
    QHBoxLayout* layout,
    QWidget* parent
    )
{
    auto* result = new QPushButton(text(label), parent);
    result->setObjectName(QString::fromLatin1(name));
    layout->addWidget(result);
    return result;
}

QString withPdfSuffix(QString path)
{
    if (!path.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
    {
        path += QStringLiteral(".pdf");
    }
    return path;
}

QString fromUtf8(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

std::optional<QString> requestSavePath(
    const RosterOutputActions& actions,
    QWidget* parent,
    QString* failureMessage
    )
{
    using namespace Application;
    if (!actions.saveFileDialog)
    {
        *failureMessage = text("The roster PDF chooser is unavailable.");
        return std::nullopt;
    }

    const SaveFileDialogRequest request{
        .purpose = FileDialogPurpose::GeneratedPdf,
        .title = "Save Rosters as PDF",
        .suggestedFileName = "Rosters.pdf",
        .nameFilters = {"PDF Files (*.pdf)"},
        .defaultSuffix = "pdf"
    };

    try
    {
        const auto response = actions.saveFileDialog(parent, request);
        if (!response)
        {
            *failureMessage = fromUtf8(response.error().message);
            if (failureMessage->isEmpty())
            {
                *failureMessage = text("Unable to choose a roster PDF destination.");
            }
            return std::nullopt;
        }
        if (!response.value().selection
            || response.value().selection->path.empty())
        {
            return std::nullopt;
        }
        return withPdfSuffix(fromUtf8(response.value().selection->path.value()));
    }
    catch (const std::exception&)
    {
        *failureMessage = text("Unable to choose a roster PDF destination.");
        return std::nullopt;
    }
    catch (...)
    {
        *failureMessage = text("Unable to choose a roster PDF destination.");
        return std::nullopt;
    }
}

DocumentOutputResult copyPdf(const QString& source, const QString& target)
{
    QFile input(source);
    QSaveFile output(withPdfSuffix(target));
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
    {
        return {
            DocumentOutputStatus::Failed,
            text("Unable to create the roster print file.")
        };
    }

    while (!input.atEnd())
    {
        const QByteArray bytes = input.read(65536);
        if (bytes.isEmpty() && input.error() != QFileDevice::NoError)
        {
            return {
                DocumentOutputStatus::Failed,
                text("The roster print file could not be completed.")
            };
        }
        if (output.write(bytes) != bytes.size())
        {
            return {
                DocumentOutputStatus::Failed,
                text("The roster print file could not be completed.")
            };
        }
    }
    if (!output.commit())
    {
        return {
            DocumentOutputStatus::Failed,
            text("The roster print file could not be completed.")
        };
    }
    return {DocumentOutputStatus::Sent, text("Roster PDF created.")};
}

std::unique_ptr<Platform::RosterPdfPreviewSession> createRosterPdfPreviewSession(
    const RosterOutputActions& actions
    )
{
    if (!actions.pdfPreviewSessionFactory)
    {
        return {};
    }
    try
    {
        return actions.pdfPreviewSessionFactory();
    }
    catch (...)
    {
        return {};
    }
}

DocumentOutputResult failed(const QString& message)
{
    return {DocumentOutputStatus::Failed, message};
}

DocumentOutputResult printSession(
    const RosterOutputActions& actions,
    QWidget* parent,
    Platform::RosterPdfPreviewSession& session,
    QPageLayout::Orientation orientation,
    const DialogGeometryPreferencesCallbacks& geometryPreferences
    )
{
    if (!actions.printOperationFactory)
    {
        return failed(text("Unable to create the roster print operation."));
    }

    try
    {
        std::unique_ptr<Platform::PdfPrintOperation> operation =
            actions.printOperationFactory();
        if (!operation)
        {
            return failed(text("Unable to create the roster print operation."));
        }

        const Platform::PdfPrintResult result = session.printReadyDocument(
            *operation,
            {
                .parent = parent,
                .dialogTitle = text("Print Rosters"),
                .pageOrientation = orientation,
                .fitToPageByDefault = false,
                .preferredPageSize = QPageSize::A4,
                .lockPreferredPageSize = true,
                .uiFont = actions.fonts.ui,
                .dialogGeometryPreferences = {
                    geometryPreferences.read,
                    geometryPreferences.write
                }
            });
        switch (result.status)
        {
        case Platform::PdfPrintStatus::Sent:
            return {DocumentOutputStatus::Sent, result.message};
        case Platform::PdfPrintStatus::Canceled:
            return {DocumentOutputStatus::Canceled, {}};
        case Platform::PdfPrintStatus::Failed:
        default:
            return failed(result.message);
        }
    }
    catch (const std::exception&)
    {
        return failed(text("The roster print operation failed."));
    }
    catch (...)
    {
        return failed(text("The roster print operation failed."));
    }
}

bool renderPreviewPage(
    Platform::RosterPdfPreviewSession& session,
    int page,
    QLabel* image
    )
{
    const QImage rendered = session.renderPage(page, QSize(820, 630));
    if (rendered.isNull())
    {
        return false;
    }
    image->setPixmap(QPixmap::fromImage(rendered));
    return true;
}
}

RosterPrintDialog::RosterPrintDialog(
    QString databasePath,
    Domain::ClassId current,
    bool testing,
    Output::RosterOutputChoices choices,
    RosterOutputActions actions,
    DialogGeometryPreferencesCallbacks geometryPreferences,
    QWidget* parent
    )
    : DialogShell(
          QStringLiteral("nextRosterPrint"),
          parent,
          requireNextRosterGeometryPreferences(geometryPreferences)
          ),
      m_geometryPreferences(std::move(geometryPreferences)),
      m_path(std::move(databasePath)),
      m_current(std::move(current)),
      m_testing(testing),
      m_choices(std::move(choices)),
      m_actions(std::move(actions))
{
    setWindowTitle(text("Roster Templates and Printing"));
    resize(680, 500);
    auto* root = contentLayout();
    auto* title = new QLabel(
        text("Choose a template and classes for your roster."), this);
    title->setWordWrap(true);
    root->addWidget(title);
    root->addWidget(new QLabel(text("Template"), this));
    m_template = new QComboBox(this);
    m_template->setObjectName(QStringLiteral("nextRosterTemplate"));
    if (!testing)
    {
        m_template->addItem(text("By Day"), 0);
        m_template->addItem(text("Daily"), 1);
    }
    m_template->addItem(text("Per Class with Extra Info"), 2);
    root->addWidget(m_template);
    root->addWidget(new QLabel(text("Classes"), this));
    m_scope = new QComboBox(this);
    m_scope->setObjectName(QStringLiteral("nextRosterScope"));
    if (!testing)
    {
        m_scope->addItem(text("All Classes"), 0);
    }
    m_scope->addItem(text("Current Class"), 1);
    if (!testing)
    {
        m_scope->addItem(text("Selected Classes"), 2);
    }
    root->addWidget(m_scope);
    m_classes = new QListWidget(this);
    m_classes->setObjectName(QStringLiteral("nextRosterOutputClasses"));
    for (const auto& item : m_choices.classes)
    {
        auto* row = new QListWidgetItem(item.name, m_classes);
        row->setFlags(row->flags() | Qt::ItemIsUserCheckable);
        row->setCheckState(
            item.classId == m_current ? Qt::Checked : Qt::Unchecked);
    }
    root->addWidget(m_classes);
    m_orientation = new QComboBox(this);
    m_orientation->setObjectName(QStringLiteral("nextRosterOrientation"));
    m_orientation->addItem(text("Portrait"));
    m_orientation->addItem(text("Landscape"));
    root->addWidget(m_orientation);
    m_extra = new QListWidget(this);
    m_extra->setObjectName(QStringLiteral("nextRosterExtraColumns"));
    for (const auto& column : m_choices.extraColumns)
    {
        auto* item = new QListWidgetItem(column, m_extra);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    root->addWidget(m_extra);
    root->addStretch(1);
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("nextRosterOutputStatus"));
    m_status->setWordWrap(true);
    root->addWidget(m_status);
    auto* toolbar = new QHBoxLayout();
    root->addLayout(toolbar);
    connect(button("Preview", "nextRosterPreview", toolbar, this),
        &QPushButton::clicked, this, [this] { preview(); });
    toolbar->addStretch();
    connect(button("Save As PDF", "nextRosterSavePdf", toolbar, this),
        &QPushButton::clicked, this, [this] { save(); });
    connect(button("Print", "nextRosterPrint", toolbar, this),
        &QPushButton::clicked, this, [this] { print(); });
    connect(button("Close", "nextRosterOutputClose", toolbar, this),
        &QPushButton::clicked, this, &QDialog::reject);
    connect(m_template, &QComboBox::currentIndexChanged, this,
        [this] { refreshOptions(); });
    connect(m_scope, &QComboBox::currentIndexChanged, this,
        [this] { refreshOptions(); });
    connect(m_orientation, &QComboBox::currentIndexChanged, this,
        [this] { refreshOptions(); });
    connect(m_classes, &QListWidget::itemChanged, this,
        [this] { refreshOptions(); });
    refreshOptions();
}

Output::RosterOutputRequest RosterPrintDialog::request() const
{
    Output::RosterOutputRequest result{m_current};
    result.testingClass = m_testing;
    result.templateId = static_cast<RosterTemplateRenderer::TemplateId>(
        m_template->currentData().toInt());
    result.scope = static_cast<Output::RosterOutputScope>(
        m_scope->currentData().toInt());
    result.orientation = m_orientation->currentIndex() == 0
        ? QPageLayout::Portrait
        : QPageLayout::Landscape;
    for (int index = 0; index < m_classes->count(); ++index)
    {
        if (m_classes->item(index)->checkState() == Qt::Checked)
        {
            result.selectedClassIds.push_back(m_choices.classes[index].classId);
        }
    }
    if (result.templateId == RosterTemplateRenderer::TemplateId::PerClassWithExtraInfo)
    {
        for (int index = 0; index < m_extra->count(); ++index)
        {
            if (m_extra->item(index)->checkState() == Qt::Checked)
            {
                result.extraColumns.append(m_extra->item(index)->text());
            }
        }
    }
    return result;
}

void RosterPrintDialog::refreshOptions()
{
    const auto selection = request();
    m_classes->setVisible(
        selection.scope == Output::RosterOutputScope::SelectedClasses);
    const bool extra = selection.templateId
        == RosterTemplateRenderer::TemplateId::PerClassWithExtraInfo;
    m_orientation->setVisible(extra);
    m_extra->setVisible(extra);
    QStringList available;
    for (const auto& item : m_choices.classes)
    {
        const bool included = selection.scope == Output::RosterOutputScope::AllClasses
            || (selection.scope == Output::RosterOutputScope::CurrentClass
                && item.classId == m_current)
            || (selection.scope == Output::RosterOutputScope::SelectedClasses
                && std::find(
                    selection.selectedClassIds.cbegin(),
                    selection.selectedClassIds.cend(),
                    item.classId)
                    != selection.selectedClassIds.cend());
        if (included)
        {
            for (const auto& column : item.extraColumns)
            {
                if (!available.contains(column, Qt::CaseInsensitive))
                {
                    available.append(column);
                }
            }
        }
    }
    m_extra->clear();
    for (const auto& column : available)
    {
        auto* item = new QListWidgetItem(column, m_extra);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(
            selection.extraColumns.contains(column, Qt::CaseInsensitive)
                ? Qt::Checked
                : Qt::Unchecked);
    }
    m_extra->setAccessibleName(text("Extra Columns"));
    if (extra)
    {
        m_status->setText(text("Select up to %1 extra columns.").arg(
            RosterTemplateRenderer::perClassExtraInfoMaxExtraColumns(
                selection.orientation)));
    }
    else
    {
        m_status->clear();
    }
}

void RosterPrintDialog::showResult(const DocumentOutputResult& result)
{
    if (result.status != DocumentOutputStatus::Canceled)
    {
        m_status->setText(result.message);
    }
}

bool RosterPrintDialog::prepareRequest(
    Output::RosterOutputRequest* selected
    )
{
    *selected = request();
    if (selected->extraColumns.size()
        > RosterTemplateRenderer::perClassExtraInfoMaxExtraColumns(
            selected->orientation))
    {
        showResult({
            DocumentOutputStatus::Failed,
            text("Too many extra columns for the selected orientation.")
        });
        return false;
    }

    if (m_actions.inspectTruncation)
    {
        const auto summary = m_actions.inspectTruncation(m_path, *selected);
        if (!summary)
        {
            showResult({DocumentOutputStatus::Failed, summary.error()});
            return false;
        }
        if (summary->requiresConfirmation()
            && DialogServices::confirm(
                this,
                text("Confirm Roster Output"),
                text("This layout can include %1 of %2 students across the selected rosters. %3 students will not appear in the output. Continue?")
                    .arg(summary->includedStudents)
                    .arg(summary->totalStudents)
                    .arg(summary->omittedStudents()),
                text("Proceed"),
                text("Cancel")) != PromptChoice::Accepted)
        {
            return false;
        }
    }
    return true;
}

DocumentOutputResult RosterPrintDialog::generate(
    const Output::RosterOutputRequest& selected,
    const QString& outputPath
    ) const
{
    if (!m_actions.savePdf)
    {
        return failed(text("Roster data is not available."));
    }
    return m_actions.savePdf(m_path, selected, outputPath, m_actions.fonts);
}

std::optional<QString> RosterPrintDialog::chooseSavePath(
    QWidget* parent,
    QString* failureMessage
    ) const
{
    return requestSavePath(m_actions, parent, failureMessage);
}

void RosterPrintDialog::save()
{
    QString chooserFailure;
    const auto target = chooseSavePath(this, &chooserFailure);
    if (!chooserFailure.isEmpty())
    {
        showResult(failed(chooserFailure));
        return;
    }
    if (!target)
    {
        return;
    }

    Output::RosterOutputRequest selected{m_current};
    if (!prepareRequest(&selected))
    {
        return;
    }
    showResult(generate(selected, *target));
}

void RosterPrintDialog::print()
{
    std::unique_ptr<Platform::RosterPdfPreviewSession> session =
        createRosterPdfPreviewSession(m_actions);
    if (!session || !session->isValid())
    {
        showResult(failed(text("Unable to create a temporary roster PDF.")));
        return;
    }

    Output::RosterOutputRequest selected{m_current};
    if (!prepareRequest(&selected))
    {
        return;
    }
    const DocumentOutputResult generated = generate(selected, session->path());
    if (generated.status != DocumentOutputStatus::Sent)
    {
        showResult(generated);
        return;
    }
    if (!session->loadGeneratedPdf())
    {
        showResult(failed(text("Unable to load the generated roster PDF.")));
        return;
    }

    const QPageLayout::Orientation orientation =
        selected.templateId
                == RosterTemplateRenderer::TemplateId::PerClassWithExtraInfo
            ? selected.orientation
            : RosterTemplateRenderer::templateOrientation(selected.templateId);
    const DocumentOutputResult result = printSession(
        m_actions,
        this,
        *session,
        orientation,
        m_geometryPreferences);
    session->release();
    showResult(result);
}

void RosterPrintDialog::preview()
{
    std::unique_ptr<Platform::RosterPdfPreviewSession> session =
        createRosterPdfPreviewSession(m_actions);
    if (!session || !session->isValid())
    {
        showResult(failed(text("Unable to create a temporary roster PDF.")));
        return;
    }

    Output::RosterOutputRequest selected{m_current};
    if (!prepareRequest(&selected))
    {
        return;
    }
    const DocumentOutputResult generated = generate(selected, session->path());
    if (generated.status != DocumentOutputStatus::Sent)
    {
        showResult(generated);
        return;
    }
    if (!session->loadGeneratedPdf())
    {
        showResult(failed(text("Unable to load the generated roster PDF.")));
        return;
    }

    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("nextRosterPreviewDialog"));
    dialog.setWindowTitle(text("Roster Preview"));
    dialog.resize(900, 760);
    auto* root = new QVBoxLayout(&dialog);
    auto* image = new QLabel(&dialog);
    image->setObjectName(QStringLiteral("nextRosterPreviewPage"));
    image->setAlignment(Qt::AlignCenter);
    root->addWidget(image, 1);
    auto* status = new QLabel(&dialog);
    status->setObjectName(QStringLiteral("nextRosterPreviewStatus"));
    status->setWordWrap(true);
    root->addWidget(status);
    auto* navigation = new QHBoxLayout();
    root->addLayout(navigation);
    int page = 0;
    auto* previous = button("Previous", "nextRosterPreviousPage", navigation, &dialog);
    auto* count = new QLabel(&dialog);
    navigation->addWidget(count);
    auto* next = button("Next", "nextRosterNextPage", navigation, &dialog);
    navigation->addStretch();
    const auto refresh = [&] {
        if (!renderPreviewPage(*session, page, image))
        {
            status->setText(text("Unable to render the roster preview page."));
        }
        else
        {
            status->clear();
        }
        count->setText(
            text("Page %1 of %2").arg(page + 1).arg(session->pageCount()));
        previous->setEnabled(page > 0);
        next->setEnabled(page + 1 < session->pageCount());
    };
    if (!renderPreviewPage(*session, page, image))
    {
        showResult(failed(text("Unable to render the roster preview page.")));
        return;
    }
    count->setText(
        text("Page %1 of %2").arg(page + 1).arg(session->pageCount()));
    previous->setEnabled(false);
    next->setEnabled(session->pageCount() > 1);
    connect(previous, &QPushButton::clicked, &dialog, [&] {
        --page;
        refresh();
    });
    connect(next, &QPushButton::clicked, &dialog, [&] {
        ++page;
        refresh();
    });
    connect(button("Save As PDF", "nextRosterPreviewSavePdf", navigation, &dialog),
        &QPushButton::clicked, &dialog, [&] {
            QString chooserFailure;
            const auto target = chooseSavePath(&dialog, &chooserFailure);
            if (!chooserFailure.isEmpty())
            {
                status->setText(chooserFailure);
                return;
            }
            if (!target)
            {
                return;
            }
            const auto result = copyPdf(session->path(), *target);
            if (result.status != DocumentOutputStatus::Canceled)
            {
                status->setText(result.message);
            }
        });
    const QPageLayout::Orientation orientation =
        selected.templateId
                == RosterTemplateRenderer::TemplateId::PerClassWithExtraInfo
            ? selected.orientation
            : RosterTemplateRenderer::templateOrientation(selected.templateId);
    connect(button("Print", "nextRosterPreviewPrint", navigation, &dialog),
        &QPushButton::clicked, &dialog, [&] {
            const auto result = printSession(
                m_actions,
                &dialog,
                *session,
                orientation,
                m_geometryPreferences);
            if (result.status != DocumentOutputStatus::Canceled)
            {
                status->setText(result.message);
            }
        });
    connect(button("Close", "nextRosterPreviewClose", navigation, &dialog),
        &QPushButton::clicked, &dialog, &QDialog::reject);
    dialog.exec();
}
}
