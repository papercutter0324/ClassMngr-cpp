class MainWindow;

class MenuBuilder
{
public:
    static void build(MainWindow* window);

private:
    static void buildFileMenu(MainWindow* window);
    static void buildEditMenu(MainWindow* window);
    static void buildClassMenu(MainWindow* window);
    static void buildPrintExportMenu(MainWindow* window);
    static void buildHelpMenu(MainWindow* window);
    static void buildAdminMenu(MainWindow* window);
    static void buildDeveloperMenu(MainWindow* window);
};
