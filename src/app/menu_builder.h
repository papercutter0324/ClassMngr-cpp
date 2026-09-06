#include <memory>

class QDialog;
class MainWindow;

class MenuBuilder
{
public:
    static void build(MainWindow* window);

    // The menu action normally opens Preferences through a blocking modal
    // loop. Visual evidence needs the same production dialog without nesting
    // that loop inside the capture harness.
    [[nodiscard]] static std::unique_ptr<QDialog> createPreferencesDialog(
        MainWindow* window
        );

private:
    static void buildFileMenu(MainWindow* window);
    static void buildEditMenu(MainWindow* window);
    static void buildClassMenu(MainWindow* window);
    static void buildPrintExportMenu(MainWindow* window);
    static void buildHelpMenu(MainWindow* window);
    static void buildAdminMenu(MainWindow* window);
    static void buildDeveloperMenu(MainWindow* window);
};
