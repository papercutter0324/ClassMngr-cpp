#include "next/ui/initial_page.h"

#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

namespace ClassMngr::Next::Ui
{

InitialPage::InitialPage(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("classMngrNextInitialPage"));

    auto* title = new QLabel(tr("ClassMngr Next"), this);
    title->setObjectName(QStringLiteral("classMngrNextInitialPageTitle"));
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 8);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);

    auto* description = new QLabel(
        tr("The next-generation workspace is under development."),
        this
        );
    description->setObjectName(
        QStringLiteral("classMngrNextInitialPageDescription")
        );
    description->setWordWrap(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(48, 44, 48, 44);
    layout->addStretch(1);
    layout->addWidget(title, 0, Qt::AlignHCenter);
    layout->addSpacing(12);
    layout->addWidget(description, 0, Qt::AlignHCenter);
    layout->addStretch(1);
}

}  // namespace ClassMngr::Next::Ui
