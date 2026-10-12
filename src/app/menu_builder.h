#pragma once

#include <memory>

namespace ClassMngr::Next::Application
{
class ExcelImportTimeoutPreferencesPort;
}

class MainWindow;

class MenuBuilder
{
public:
    static void build(
        MainWindow* window,
        std::shared_ptr<
            ClassMngr::Next::Application::ExcelImportTimeoutPreferencesPort
            > excelImportTimeoutPreferencesPort = {}
        );

private:
    static void buildFileMenu(MainWindow* window);
    static void buildEditMenu(
        MainWindow* window,
        std::shared_ptr<
            ClassMngr::Next::Application::ExcelImportTimeoutPreferencesPort
            > excelImportTimeoutPreferencesPort
        );
    static void buildClassMenu(MainWindow* window);
    static void buildPrintExportMenu(MainWindow* window);
    static void buildHelpMenu(MainWindow* window);
    static void buildAdminMenu(MainWindow* window);
};
