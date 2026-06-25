#include "campus_dashboard_page.h"

#include <QFontMetrics>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QWidget>

#include <utility>

QLabel* CampusDashboardPage::createTranslatableLabel(
    const char* sourceText,
    QWidget* parent
    )
{
    auto* label =
        new QLabel(
            tr(sourceText),
            parent
            );

    TranslatableLabel entry;
    entry.label = label;
    entry.sourceText = sourceText;
    m_translatableLabels.append(entry);

    return label;
}

void CampusDashboardPage::addFormRow(
    QFormLayout* form,
    const char* labelText,
    QWidget* field
    )
{
    if (!form)
    {
        return;
    }

    form->addRow(
        createTranslatableLabel(
            labelText,
            form->parentWidget()
            ),
        field
        );
}

void CampusDashboardPage::insertFormRow(
    QFormLayout* form,
    int row,
    const char* labelText,
    QWidget* field
    )
{
    if (!form)
    {
        return;
    }

    form->insertRow(
        row,
        createTranslatableLabel(
            labelText,
            form->parentWidget()
            ),
        field
        );
}

void CampusDashboardPage::retranslateRegisteredLabels()
{
    for (const TranslatableLabel& entry : std::as_const(m_translatableLabels))
    {
        if (
            entry.label
            && entry.sourceText
            )
        {
            entry.label->setText(
                tr(entry.sourceText)
            );
        }
    }

    alignAllAddressDetailsWithCompleteFields();
}

QLineEdit* CampusDashboardPage::addLineField(
    QFormLayout* form,
    const char* labelText
    )
{
    auto* edit =
        new QLineEdit(this);

    edit->setMinimumWidth(280);

    addFormRow(
        form,
        labelText,
        edit
        );

    m_lineEdits.append(edit);

    connect(
        edit,
        &QLineEdit::textEdited,
        this,
        [this]()
        {
            handleFieldEdited();
        }
        );

    return edit;
}

QPlainTextEdit* CampusDashboardPage::addTextField(
    QFormLayout* form,
    const char* labelText,
    int minimumLines,
    int maximumLines
    )
{
    auto* edit =
        new QPlainTextEdit(this);

    edit->setTabChangesFocus(true);
    edit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    edit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    configureExpandingTextField(
        edit,
        minimumLines,
        maximumLines
        );

    addFormRow(
        form,
        labelText,
        edit
        );

    m_textEdits.append(edit);

    connect(
        edit,
        &QPlainTextEdit::textChanged,
        this,
        [this, edit]()
        {
            updateTextFieldHeight(edit);
            handleFieldEdited();
        }
        );

    return edit;
}

void CampusDashboardPage::configureExpandingTextField(
    QPlainTextEdit* edit,
    int minimumLines,
    int maximumLines
    )
{
    if (!edit)
    {
        return;
    }

    edit->setProperty(
        "minimumVisibleLines",
        minimumLines
        );

    edit->setProperty(
        "maximumVisibleLines",
        maximumLines
        );

    updateTextFieldHeight(edit);
}

void CampusDashboardPage::updateTextFieldHeight(
    QPlainTextEdit* edit
    )
{
    if (!edit)
    {
        return;
    }

    const int minimumLines =
        edit
            ->property("minimumVisibleLines")
            .toInt();

    const int maximumLines =
        edit
            ->property("maximumVisibleLines")
            .toInt();

    const int blockCount =
        edit->document()
            ? edit->document()->blockCount()
            : minimumLines;

    const int visibleLines =
        qBound(
            minimumLines,
            blockCount,
            maximumLines
            );

    const QFontMetrics metrics(edit->font());

    const int framePadding =
        edit->frameWidth() * 2 + 12;

    edit->setFixedHeight(
        (metrics.lineSpacing() * visibleLines * 6) / 5
            + framePadding
        );
}
