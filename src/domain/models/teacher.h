#ifndef TEACHER_H
#define TEACHER_H

#include <QString>
#include <QStringList>

struct Teacher
{
    int id = -1;

    QString teacherKr;
    QString teacherEn;
    QString preferredRomanization;
    QString preferredName;

    QString roomNumber;
    QString birthday;
    QString phoneNumber;

    QString wifiName;
    QString wifiPassword;
    QString internetType = QStringLiteral("WiFi");

    QString zoomId;
    QString zoomPassword;
    QString projectionType = QStringLiteral("HDMI");

    QString notes;

    [[nodiscard]] QStringList preferredNameChoices() const
    {
        QStringList choices;

        for (const QString& choice : {
                 teacherEn.trimmed(),
                 preferredRomanization.trimmed()
             })
        {
            if (!choice.isEmpty() && !choices.contains(choice))
            {
                choices.append(choice);
            }
        }

        return choices;
    }

    [[nodiscard]] QString preferredDisplayName() const
    {
        const QString selected = preferredName.trimmed();

        if (!selected.isEmpty())
        {
            return selected;
        }

        const QStringList choices = preferredNameChoices();

        if (!choices.isEmpty())
        {
            return choices.first();
        }

        return teacherKr.trimmed();
    }
};

#endif // TEACHER_H
