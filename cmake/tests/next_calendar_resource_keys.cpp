#include <QResource>

#include <array>
#include <iostream>

int main()
{
    constexpr std::array resourceKeys{
        ":/qt/qml/ClassMngr/Calendar/EventCalendar.qml",
        ":/qt/qml/ClassMngr/Calendar/MonthGridDelegate.qml",
    };

    for (const auto* key : resourceKeys) {
        if (!QResource(QString::fromLatin1(key)).isValid()) {
            std::cerr << "Missing embedded Calendar QML resource: " << key << '\n';
            return 1;
        }
    }

    return 0;
}
