#pragma once

#include "ui/pages/basepage.h"

class QLabel;

class SubPrepPage : public BasePage
{
    Q_OBJECT

public:
    explicit SubPrepPage(
        QWidget* parent = nullptr
        );

private:
    void buildUi();

private:
    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
};
