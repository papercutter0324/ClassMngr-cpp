#pragma once

#include <QMainWindow>

#include <functional>

class QCloseEvent;

namespace ClassMngr::Next::Resources
{
class ResourceLoader;
}

namespace ClassMngr::Next::Ui
{

class PageHost;

class ApplicationShell final : public QMainWindow
{
public:
    using CloseHandler = std::function<void()>;

    // The resource loader is borrowed and must outlive this shell.
    explicit ApplicationShell(
        Resources::ResourceLoader& resourceLoader,
        QWidget* parent = nullptr
        );

    void setCloseHandler(CloseHandler handler);
    [[nodiscard]] PageHost& pageHost() noexcept;
    [[nodiscard]] const PageHost& pageHost() const noexcept;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    PageHost* m_pageHost = nullptr;
    CloseHandler m_closeHandler;
};

}  // namespace ClassMngr::Next::Ui
