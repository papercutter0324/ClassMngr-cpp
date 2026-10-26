#pragma once

#include <QMainWindow>

#include <functional>

class QCloseEvent;

namespace ClassMngr::Next::Ui
{

class ApplicationShell final : public QMainWindow
{
public:
    using CloseHandler = std::function<void()>;

    explicit ApplicationShell(QWidget* parent = nullptr);

    void setCloseHandler(CloseHandler handler);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    CloseHandler m_closeHandler;
};

}  // namespace ClassMngr::Next::Ui
