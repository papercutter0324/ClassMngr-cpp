#include "teacher_validator.h"

#include "core/utils/student_name_utils.h"
#include "domain/validation/shared_validation.h"
#include "domain/validation/validation_rules.h"

#include <QDate>
#include <QRegularExpression>
#include <QStringList>

namespace
{
constexpr qsizetype RoomNumberMaximumLength = 64;
constexpr qsizetype BirthdayMaximumLength = 5;
constexpr qsizetype PhoneNumberMaximumLength = 32;
constexpr qsizetype CredentialMaximumLength = 128;
constexpr qsizetype PasswordMaximumLength = 256;
constexpr qsizetype NotesMaximumLength = 10000;

const QStringList InternetTypes{
    QStringLiteral("WiFi"),
    QStringLiteral("LAN"),
    QStringLiteral("Both"),
    QStringLiteral("N/A")
};

const QStringList ProjectionTypes{
    QStringLiteral("HDMI"),
    QStringLiteral("Zoom"),
    QStringLiteral("Any"),
    QStringLiteral("N/A")
};

QString canonicalChoice(const QString& value, const QStringList& choices)
{
    const QString trimmed = value.trimmed();
    for (const QString& choice : choices)
    {
        if (choice.compare(trimmed, Qt::CaseInsensitive) == 0)
        {
            return choice;
        }
    }

    // Preserve unknown values for the validator instead of quietly replacing
    // them with a default that means something else.
    return trimmed;
}

QString normalizedPreferredName(const QString& value, const Teacher& teacher)
{
    return canonicalChoice(value, teacher.preferredNameChoices());
}

ValidationLocation field(const QString& name)
{
    return {.field = name};
}

bool birthdayIsValid(const QString& value)
{
    const QString birthday = value.trimmed();
    if (birthday.isEmpty())
    {
        return true;
    }

    static const QRegularExpression format(
        QStringLiteral("^\\d{2}-\\d{2}$")
        );
    if (!format.match(birthday).hasMatch())
    {
        return false;
    }

    return QDate::fromString(
        QStringLiteral("2000-%1").arg(birthday),
        QStringLiteral("yyyy-MM-dd")
        ).isValid();
}

ValidationResult textLength(
    const QString& value,
    qsizetype maximumLength,
    const QString& name
    )
{
    return ValidationRules::textLength(value, 0, maximumLength, field(name));
}
}

Teacher TeacherValidator::normalized(const Teacher& teacher)
{
    Teacher normalized = teacher;

    normalized.teacherKr =
        StudentNameUtils::normalizeKoreanName(teacher.teacherKr);
    normalized.teacherEn =
        StudentNameUtils::normalizeEnglishName(teacher.teacherEn);
    normalized.preferredRomanization =
        StudentNameUtils::normalizeEnglishName(teacher.preferredRomanization);
    normalized.preferredName = normalizedPreferredName(
        teacher.preferredName,
        normalized
        );

    normalized.roomNumber = teacher.roomNumber.trimmed();
    normalized.birthday = teacher.birthday.trimmed();
    normalized.phoneNumber = normalizedPhoneNumber(teacher.phoneNumber);
    normalized.wifiName = teacher.wifiName.trimmed();
    normalized.wifiPassword = teacher.wifiPassword.trimmed();
    normalized.internetType = canonicalChoice(teacher.internetType, InternetTypes);
    normalized.zoomId = teacher.zoomId.trimmed();
    normalized.zoomPassword = teacher.zoomPassword.trimmed();
    normalized.projectionType = canonicalChoice(
        teacher.projectionType,
        ProjectionTypes
        );
    normalized.notes = teacher.notes.trimmed();

    return normalized;
}

QString TeacherValidator::normalizedPhoneNumber(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty())
    {
        return {};
    }

    QString digits;
    bool sawLeadingPlus = false;
    for (const QChar character : trimmed)
    {
        if (character.isDigit())
        {
            digits.append(character);
        }
        else if (character == QChar(u'+') && digits.isEmpty() && !sawLeadingPlus)
        {
            sawLeadingPlus = true;
        }
        else if (character != QChar(u' ')
                 && character != QChar(u'-')
                 && character != QChar(u'(')
                 && character != QChar(u')'))
        {
            // Do not transform an invalid value into a different, seemingly
            // valid number. Validation will report it unchanged.
            return trimmed;
        }
    }

    if (digits.isEmpty())
    {
        return trimmed;
    }

    if (!sawLeadingPlus && digits.size() == 11 && digits.startsWith("010"))
    {
        return QStringLiteral("%1-%2-%3")
            .arg(digits.first(3), digits.mid(3, 4), digits.last(4));
    }

    if (!sawLeadingPlus && digits.size() == 10 && digits.startsWith("02"))
    {
        return QStringLiteral("%1-%2-%3")
            .arg(digits.first(2), digits.mid(2, 4), digits.last(4));
    }

    return sawLeadingPlus ? QStringLiteral("+%1").arg(digits) : digits;
}

ValidationResult TeacherValidator::validate(const Teacher& teacher)
{
    ValidationResult result;

    if (teacher.teacherKr.trimmed().isEmpty()
        && teacher.teacherEn.trimmed().isEmpty()
        && teacher.preferredRomanization.trimmed().isEmpty())
    {
        result.add(ValidationRules::issue(
            QStringLiteral("teacher.name.required"),
            field(QStringLiteral("teacherEn"))
            ));
    }

    result.merge(SharedValidation::koreanName(
        teacher.teacherKr,
        field(QStringLiteral("teacherKr"))
        ));
    result.merge(SharedValidation::englishName(
        teacher.teacherEn,
        field(QStringLiteral("teacherEn"))
        ));
    result.merge(SharedValidation::englishName(
        teacher.preferredRomanization,
        field(QStringLiteral("preferredRomanization"))
        ));

    const QString preferredName = teacher.preferredName.trimmed();
    if (!preferredName.isEmpty()
        && !teacher.preferredNameChoices().contains(preferredName))
    {
        result.add(ValidationRules::issue(
            QStringLiteral("teacher.preferred_name.invalid_choice"),
            field(QStringLiteral("preferredName")),
            ValidationSeverity::Error,
            {{QStringLiteral("value"), preferredName},
             {QStringLiteral("allowedValues"), teacher.preferredNameChoices()}}
            ));
    }

    result.merge(textLength(
        teacher.roomNumber,
        RoomNumberMaximumLength,
        QStringLiteral("roomNumber")
        ));
    result.merge(textLength(
        teacher.birthday,
        BirthdayMaximumLength,
        QStringLiteral("birthday")
        ));
    result.merge(textLength(
        teacher.phoneNumber,
        PhoneNumberMaximumLength,
        QStringLiteral("phoneNumber")
        ));
    result.merge(textLength(
        teacher.wifiName,
        CredentialMaximumLength,
        QStringLiteral("wifiName")
        ));
    result.merge(textLength(
        teacher.wifiPassword,
        PasswordMaximumLength,
        QStringLiteral("wifiPassword")
        ));
    result.merge(textLength(
        teacher.zoomId,
        CredentialMaximumLength,
        QStringLiteral("zoomId")
        ));
    result.merge(textLength(
        teacher.zoomPassword,
        PasswordMaximumLength,
        QStringLiteral("zoomPassword")
        ));
    result.merge(textLength(
        teacher.notes,
        NotesMaximumLength,
        QStringLiteral("notes")
        ));

    if (!birthdayIsValid(teacher.birthday))
    {
        result.add(ValidationRules::issue(
            QStringLiteral("teacher.birthday.invalid"),
            field(QStringLiteral("birthday"))
            ));
    }

    static const QRegularExpression phoneCharacters(
        QStringLiteral("^\\+?[0-9() -]+$")
        );
    const QString phone = teacher.phoneNumber.trimmed();
    if (!phone.isEmpty())
    {
        QString digits = phone;
        digits.remove(QRegularExpression(QStringLiteral("[^0-9]")));
        if (!phoneCharacters.match(phone).hasMatch()
            || digits.size() < 7
            || digits.size() > 15)
        {
            result.add(ValidationRules::issue(
                QStringLiteral("teacher.phone.invalid"),
                field(QStringLiteral("phoneNumber"))
                ));
        }
    }

    result.merge(ValidationRules::stringEnumValue(
        teacher.internetType,
        InternetTypes,
        field(QStringLiteral("internetType"))
        ));
    result.merge(ValidationRules::stringEnumValue(
        teacher.projectionType,
        ProjectionTypes,
        field(QStringLiteral("projectionType"))
        ));

    return result;
}
