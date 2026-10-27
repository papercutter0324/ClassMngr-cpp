#include "next/ui/placeholder_page.h"

#include <QFont>
#include <QCoreApplication>
#include <QLabel>
#include <QVBoxLayout>

#include <utility>

namespace ClassMngr::Next::Ui
{

PlaceholderPage::PlaceholderPage(
    const PageContext& context,
    QString label,
    QWidget* parent
    )
    : Page(context, parent)
{
    setObjectName(QStringLiteral("classMngrNextPlaceholderPage"));

    auto* const title = new QLabel(std::move(label), this);
    title->setObjectName(QStringLiteral("classMngrNextPlaceholderPageTitle"));
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 5);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);

    auto* const description = new QLabel(
        QCoreApplication::translate(
            "NextPlaceholder", "This section is being prepared."),
        this
        );
    description->setObjectName(
        QStringLiteral("classMngrNextPlaceholderPageDescription")
        );
    description->setWordWrap(true);
    description->setAlignment(Qt::AlignCenter);

    auto* const layout = new QVBoxLayout(this);
    layout->setContentsMargins(40, 36, 40, 36);
    layout->addStretch(1);
    layout->addWidget(title, 0, Qt::AlignHCenter);
    layout->addSpacing(10);
    layout->addWidget(description);
    layout->addStretch(1);
}

PageOperationResult<void> PlaceholderPage::saveChanges()
{
    // Placeholder pages have no persisted data of their own.
    setHasUnsavedChanges(false);
    return {};
}

PageOperationResult<void> PlaceholderPage::discardChanges()
{
    // Placeholder pages have no pending fields to restore.
    setHasUnsavedChanges(false);
    return {};
}

}  // namespace ClassMngr::Next::Ui
