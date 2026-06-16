#ifndef APPLICATIONSERVICES_H
#define APPLICATIONSERVICES_H

#include "core/result.h"

#include <QString>
#include <memory>

class DataService;
class ThemeService;

class ApplicationServices
{
public:
    ApplicationServices();

    ~ApplicationServices();

    [[nodiscard]] Status openDatabase(
        const QString& databasePath
        );

    void closeDatabase();

    [[nodiscard]] bool hasOpenDatabase() const;

    [[nodiscard]] QString currentDatabasePath() const;

    [[nodiscard]] DataService* dataService() const;
    [[nodiscard]] ThemeService* themeService() const;

private:
    std::unique_ptr<DataService> m_dataService;
    std::unique_ptr<ThemeService> m_themeService;
};

#endif // APPLICATIONSERVICES_H
