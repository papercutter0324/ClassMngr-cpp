#ifndef APPLICATIONSERVICES_H
#define APPLICATIONSERVICES_H

#include <QString>
#include <memory>

class DataService;
class ThemeService;

class ApplicationServices
{
public:
    explicit ApplicationServices(
        const QString& databasePath
        );

    ~ApplicationServices();

    DataService* dataService() const;
    ThemeService* themeService() const;

private:
    std::unique_ptr<DataService> m_dataService;
    std::unique_ptr<ThemeService> m_themeService;
};

#endif // APPLICATIONSERVICES_H