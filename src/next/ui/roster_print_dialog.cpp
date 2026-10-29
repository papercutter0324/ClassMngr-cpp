#include "next/ui/roster_print_dialog.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPdfDocument>
#include <QPushButton>
#include <QSaveFile>
#include <QVBoxLayout>
#include <algorithm>

namespace ClassMngr::Next::Ui
{
namespace
{
QString text(const char* source) { return QCoreApplication::translate("NextRosterOutput", source); }
QPushButton* button(const char* label, const char* name, QHBoxLayout* layout, QWidget* parent)
{
    auto* result = new QPushButton(text(label), parent);
    result->setObjectName(QString::fromLatin1(name)); layout->addWidget(result); return result;
}
QString choosePath(const RosterOutputActions& actions, QWidget* parent)
{
    return actions.chooseSavePath ? actions.chooseSavePath(parent) : QFileDialog::getSaveFileName(parent,
        text("Save Rosters as PDF"), QStringLiteral("Rosters.pdf"), text("PDF Files (*.pdf)"));
}
DocumentOutputResult printFile(const RosterOutputActions& actions, QWidget* parent, const QString& path,
    QPageLayout::Orientation orientation)
{
    if (actions.printPdf) return actions.printPdf(parent, path, orientation);
    QPdfDocument document;
    if (document.load(path) != QPdfDocument::Error::None || document.pageCount() <= 0)
        return {DocumentOutputStatus::Failed, text("Unable to load the generated roster PDF.")};
    return PdfPrintService::printPdfDocument({parent, &document, path, 0,
        text("Print Rosters"), orientation, false, QPageSize::A4, true, actions.fonts.ui});
}
DocumentOutputResult copyPdf(const QString& source, QString target)
{
    if (target.isEmpty()) return {DocumentOutputStatus::Canceled, {}};
    if (!target.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) target += QStringLiteral(".pdf");
    QFile input(source); QSaveFile output(target);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
        return {DocumentOutputStatus::Failed, text("Unable to create the roster print file.")};
    while (!input.atEnd())
    {
        const QByteArray bytes = input.read(65536);
        if (bytes.isEmpty() && input.error() != QFileDevice::NoError)
            return {DocumentOutputStatus::Failed, text("The roster print file could not be completed.")};
        if (output.write(bytes) != bytes.size()) return {DocumentOutputStatus::Failed, text("The roster print file could not be completed.")};
    }
    if (!output.commit()) return {DocumentOutputStatus::Failed, text("The roster print file could not be completed.")};
    return {DocumentOutputStatus::Sent, text("Roster PDF created.")};
}
}

RosterPrintDialog::RosterPrintDialog(QString databasePath, Domain::ClassId current, bool testing,
    Output::RosterOutputChoices choices, RosterOutputActions actions, QWidget* parent)
    : QDialog(parent), m_path(std::move(databasePath)), m_current(std::move(current)), m_testing(testing),
      m_choices(std::move(choices)), m_actions(std::move(actions))
{
    setObjectName(QStringLiteral("nextRosterPrintDialog"));
    setWindowTitle(text("Roster Templates and Printing")); resize(680, 500);
    auto* root = new QVBoxLayout(this);
    auto* title = new QLabel(text("Choose a template and classes for your roster."), this); title->setWordWrap(true); root->addWidget(title);
    root->addWidget(new QLabel(text("Template"), this));
    m_template = new QComboBox(this); m_template->setObjectName(QStringLiteral("nextRosterTemplate"));
    if (!testing) { m_template->addItem(text("By Day"), 0); m_template->addItem(text("Daily"), 1); }
    m_template->addItem(text("Per Class with Extra Info"), 2); root->addWidget(m_template);
    root->addWidget(new QLabel(text("Classes"), this));
    m_scope = new QComboBox(this); m_scope->setObjectName(QStringLiteral("nextRosterScope"));
    if (!testing) m_scope->addItem(text("All Classes"), 0);
    m_scope->addItem(text("Current Class"), 1);
    if (!testing) m_scope->addItem(text("Selected Classes"), 2);
    root->addWidget(m_scope);
    m_classes = new QListWidget(this); m_classes->setObjectName(QStringLiteral("nextRosterOutputClasses"));
    for (const auto& item : m_choices.classes)
    {
        auto* row = new QListWidgetItem(item.name, m_classes);
        row->setFlags(row->flags() | Qt::ItemIsUserCheckable);
        row->setCheckState(item.classId == m_current ? Qt::Checked : Qt::Unchecked);
    }
    root->addWidget(m_classes);
    m_orientation = new QComboBox(this); m_orientation->setObjectName(QStringLiteral("nextRosterOrientation"));
    m_orientation->addItem(text("Portrait")); m_orientation->addItem(text("Landscape")); root->addWidget(m_orientation);
    m_extra = new QListWidget(this); m_extra->setObjectName(QStringLiteral("nextRosterExtraColumns"));
    for (const auto& column : m_choices.extraColumns)
    {
        auto* item = new QListWidgetItem(column, m_extra);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable); item->setCheckState(Qt::Unchecked);
    }
    root->addWidget(m_extra);
    root->addStretch(1);
    m_status = new QLabel(this); m_status->setObjectName(QStringLiteral("nextRosterOutputStatus")); m_status->setWordWrap(true); root->addWidget(m_status);
    auto* toolbar = new QHBoxLayout(); root->addLayout(toolbar);
    connect(button("Preview", "nextRosterPreview", toolbar, this), &QPushButton::clicked, this, [this] { preview(); });
    toolbar->addStretch();
    connect(button("Save As PDF", "nextRosterSavePdf", toolbar, this), &QPushButton::clicked, this, [this] { save(); });
    connect(button("Print", "nextRosterPrint", toolbar, this), &QPushButton::clicked, this, [this] { print(); });
    connect(button("Close", "nextRosterOutputClose", toolbar, this), &QPushButton::clicked, this, &QDialog::reject);
    connect(m_template, &QComboBox::currentIndexChanged, this, [this] { refreshOptions(); });
    connect(m_scope, &QComboBox::currentIndexChanged, this, [this] { refreshOptions(); });
    connect(m_orientation, &QComboBox::currentIndexChanged, this, [this] { refreshOptions(); });
    connect(m_classes, &QListWidget::itemChanged, this, [this] { refreshOptions(); });
    refreshOptions();
}

Output::RosterOutputRequest RosterPrintDialog::request() const
{
    Output::RosterOutputRequest result{m_current};
    result.testingClass = m_testing;
    result.templateId = static_cast<RosterTemplateRenderer::TemplateId>(m_template->currentData().toInt());
    result.scope = static_cast<Output::RosterOutputScope>(m_scope->currentData().toInt());
    result.orientation = m_orientation->currentIndex() == 0 ? QPageLayout::Portrait : QPageLayout::Landscape;
    for (int index = 0; index < m_classes->count(); ++index)
        if (m_classes->item(index)->checkState() == Qt::Checked) result.selectedClassIds.push_back(m_choices.classes[index].classId);
    if (result.templateId == RosterTemplateRenderer::TemplateId::PerClassWithExtraInfo)
        for (int index = 0; index < m_extra->count(); ++index)
            if (m_extra->item(index)->checkState() == Qt::Checked) result.extraColumns.append(m_extra->item(index)->text());
    return result;
}
void RosterPrintDialog::refreshOptions()
{
    const auto selection = request();
    m_classes->setVisible(selection.scope == Output::RosterOutputScope::SelectedClasses);
    const bool extra = selection.templateId == RosterTemplateRenderer::TemplateId::PerClassWithExtraInfo;
    m_orientation->setVisible(extra); m_extra->setVisible(extra);
    QStringList available;
    for (const auto& item : m_choices.classes)
    {
        const bool included = selection.scope == Output::RosterOutputScope::AllClasses
            || (selection.scope == Output::RosterOutputScope::CurrentClass && item.classId == m_current)
            || (selection.scope == Output::RosterOutputScope::SelectedClasses
                && std::find(selection.selectedClassIds.cbegin(), selection.selectedClassIds.cend(), item.classId) != selection.selectedClassIds.cend());
        if (included)
            for (const auto& column : item.extraColumns)
                if (!available.contains(column, Qt::CaseInsensitive)) available.append(column);
    }
    m_extra->clear();
    for (const auto& column : available)
    {
        auto* item = new QListWidgetItem(column, m_extra);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(selection.extraColumns.contains(column, Qt::CaseInsensitive) ? Qt::Checked : Qt::Unchecked);
    }
    m_extra->setAccessibleName(text("Extra Columns"));
    if (extra) m_status->setText(text("Select up to %1 extra columns.").arg(RosterTemplateRenderer::perClassExtraInfoMaxExtraColumns(selection.orientation)));
    else m_status->clear();
}
void RosterPrintDialog::showResult(const DocumentOutputResult& result)
{
    if (result.status != DocumentOutputStatus::Canceled) m_status->setText(result.message);
}
QString RosterPrintDialog::generate()
{
    if (!m_temporary.isValid() || !m_actions.savePdf) { showResult({DocumentOutputStatus::Failed, text("Roster data is not available.")}); return {}; }
    const auto selected = request();
    if (selected.extraColumns.size() > RosterTemplateRenderer::perClassExtraInfoMaxExtraColumns(selected.orientation))
    { showResult({DocumentOutputStatus::Failed, text("Too many extra columns for the selected orientation.")}); return {}; }
    const auto path = m_temporary.filePath(QStringLiteral("Rosters.pdf"));
    const auto result = m_actions.savePdf(m_path, selected, path, m_actions.fonts);
    showResult(result); return result.status == DocumentOutputStatus::Sent ? path : QString{};
}
void RosterPrintDialog::save()
{
    const auto target = choosePath(m_actions, this); if (target.isEmpty()) return;
    const auto pdf = generate(); if (pdf.isEmpty()) return;
    showResult(copyPdf(pdf, target));
}
void RosterPrintDialog::print()
{
    const auto pdf = generate(); if (pdf.isEmpty()) return;
    const auto selection = request();
    showResult(printFile(m_actions, this, pdf, selection.templateId == RosterTemplateRenderer::TemplateId::PerClassWithExtraInfo
        ? selection.orientation : RosterTemplateRenderer::templateOrientation(selection.templateId)));
}
void RosterPrintDialog::preview()
{
    const auto pdf = generate(); if (pdf.isEmpty()) return;
    QPdfDocument document;
    if (document.load(pdf) != QPdfDocument::Error::None || document.pageCount() <= 0)
    { showResult({DocumentOutputStatus::Failed, text("Unable to load the generated roster PDF.")}); return; }
    QDialog dialog(this); dialog.setObjectName(QStringLiteral("nextRosterPreviewDialog")); dialog.setWindowTitle(text("Roster Preview")); dialog.resize(900, 760);
    auto* root = new QVBoxLayout(&dialog);
    auto* image = new QLabel(&dialog); image->setObjectName(QStringLiteral("nextRosterPreviewPage")); image->setAlignment(Qt::AlignCenter); root->addWidget(image, 1);
    auto* status = new QLabel(&dialog); status->setObjectName(QStringLiteral("nextRosterPreviewStatus")); status->setWordWrap(true); root->addWidget(status);
    auto* navigation = new QHBoxLayout(); root->addLayout(navigation);
    int page = 0;
    auto* previous = button("Previous", "nextRosterPreviousPage", navigation, &dialog);
    auto* count = new QLabel(&dialog); navigation->addWidget(count);
    auto* next = button("Next", "nextRosterNextPage", navigation, &dialog); navigation->addStretch();
    const auto refresh = [&] {
        const QSize size = document.pagePointSize(page).toSize().scaled(QSize(820, 630), Qt::KeepAspectRatio);
        image->setPixmap(QPixmap::fromImage(document.render(page, size)));
        count->setText(text("Page %1 of %2").arg(page + 1).arg(document.pageCount()));
        previous->setEnabled(page > 0); next->setEnabled(page + 1 < document.pageCount());
    };
    connect(previous, &QPushButton::clicked, &dialog, [&] { --page; refresh(); });
    connect(next, &QPushButton::clicked, &dialog, [&] { ++page; refresh(); });
    connect(button("Save As PDF", "nextRosterPreviewSavePdf", navigation, &dialog), &QPushButton::clicked, &dialog, [&] {
        const auto result = copyPdf(pdf, choosePath(m_actions, &dialog));
        if (result.status != DocumentOutputStatus::Canceled) status->setText(result.message);
    });
    const auto selection = request();
    connect(button("Print", "nextRosterPreviewPrint", navigation, &dialog), &QPushButton::clicked, &dialog, [&] {
        const auto result = printFile(m_actions, &dialog, pdf, selection.templateId == RosterTemplateRenderer::TemplateId::PerClassWithExtraInfo
            ? selection.orientation : RosterTemplateRenderer::templateOrientation(selection.templateId));
        if (result.status != DocumentOutputStatus::Canceled) status->setText(result.message);
    });
    connect(button("Close", "nextRosterPreviewClose", navigation, &dialog), &QPushButton::clicked, &dialog, &QDialog::reject);
    refresh(); dialog.exec();
}
}
