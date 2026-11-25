#include "license_dialog.h"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QTextEdit>
#include <QVBoxLayout>

LicenseDialog::LicenseDialog(
    const QString& title,
    const QString& licenseText,
    QWidget* parent,
    DialogGeometryPreferencesCallbacks geometryPreferences
    )
    : DialogShell(
        QStringLiteral("licenseViewer"),
        parent,
        geometryPreferences
        )
{
    setWindowTitle(title);
    setMinimumSize(640, 520);

    auto* textEdit = new QTextEdit(this);
    textEdit->setObjectName(QStringLiteral("licenseText"));
    textEdit->setAccessibleName(tr("License text"));
    textEdit->setReadOnly(true);
    textEdit->setLineWrapMode(QTextEdit::NoWrap);
    textEdit->setPlainText(licenseText);
    textEdit->setFont(
        QFontDatabase::systemFont(QFontDatabase::FixedFont)
        );
    contentLayout()->addWidget(textEdit, 1);

    addButtonBox(QDialogButtonBox::Close);
}
