#ifndef BASEPAGE_H
#define BASEPAGE_H

#include <QWidget>

// =========================================================
// Base Page
// =========================================================

class BasePage : public QWidget
{
    Q_OBJECT

public:
    explicit BasePage(QWidget* parent = nullptr)
        : QWidget(parent)
    {
    }

    virtual ~BasePage() = default;

    virtual void saveData()
    {
        // Default: nothing to save.
    }

    virtual void refresh()
    {
    }
};

#endif // BASEPAGE_H