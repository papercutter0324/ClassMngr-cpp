#include "basepage.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSizePolicy>
#include <QWidget>



// =========================================================
// Constructor
// =========================================================

BasePage::BasePage(
    QWidget* parent
    )
    : QWidget(parent)
{
    setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding
        );



    // =====================================================
    // Content Layout
    // =====================================================

    m_contentLayout =
        new QVBoxLayout;



    // =====================================================
    // Main Layout
    // =====================================================

    m_mainLayout =
        new QVBoxLayout(this);

    m_mainLayout->addLayout(
        m_contentLayout
        );

    m_mainLayout->setStretch(
        0,
        1
        );

    m_mainLayout->addStretch();



    // =====================================================
    // Bottom Bar
    // =====================================================

    m_bottomBar =
        new QWidget(this);

    m_bottomLayout =
        new QHBoxLayout(m_bottomBar);

    m_bottomLayout->setContentsMargins(
        16,
        8,
        16,
        8
        );

    m_bottomLayout->setSpacing(8);

    m_mainLayout->addWidget(
        m_bottomBar
        );
}



// =========================================================
// Persistence
// =========================================================

void BasePage::saveData()
{
}



// =========================================================
// Refresh
// =========================================================

void BasePage::refresh()
{
    if (!isVisible())
    {
        return;
    }

    update();
}



// =========================================================
// Accessors
// =========================================================

QVBoxLayout* BasePage::contentLayout() const
{
    return m_contentLayout;
}

QHBoxLayout* BasePage::bottomLayout() const
{
    return m_bottomLayout;
}