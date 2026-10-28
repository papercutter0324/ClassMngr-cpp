#pragma once

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>

namespace ClassMngr::Next::Test
{

// QSQLITE rejects multi-statement QSqlQuery::exec calls. This small fixture
// runner honors SQLite quotes and comments while splitting a checked-in SQL
// script into individual statements.
[[nodiscard]] inline bool executeSqlScript(
    QSqlDatabase database,
    const QString& script,
    QString* error = nullptr)
{
    enum class State
    {
        Normal,
        SingleQuoted,
        DoubleQuoted,
        BacktickQuoted,
        BracketQuoted,
        LineComment,
        BlockComment
    };

    State state = State::Normal;
    QString statement;
    statement.reserve(script.size());
    QSqlQuery query(database);
    auto execute = [&]() {
        const QString text = statement.trimmed();
        statement.clear();
        if (text.isEmpty())
        {
            return true;
        }
        if (query.exec(text))
        {
            return true;
        }
        if (error)
        {
            *error = query.lastError().text();
        }
        return false;
    };

    for (qsizetype index = 0; index < script.size(); ++index)
    {
        const QChar current = script.at(index);
        const QChar next = index + 1 < script.size()
            ? script.at(index + 1) : QChar{};
        switch (state)
        {
        case State::Normal:
            if (current == QLatin1Char('-') && next == QLatin1Char('-'))
            {
                statement.append(QLatin1Char(' '));
                ++index;
                state = State::LineComment;
            }
            else if (current == QLatin1Char('/') && next == QLatin1Char('*'))
            {
                statement.append(QLatin1Char(' '));
                ++index;
                state = State::BlockComment;
            }
            else if (current == QLatin1Char('\''))
            {
                statement.append(current);
                state = State::SingleQuoted;
            }
            else if (current == QLatin1Char('"'))
            {
                statement.append(current);
                state = State::DoubleQuoted;
            }
            else if (current == QLatin1Char('`'))
            {
                statement.append(current);
                state = State::BacktickQuoted;
            }
            else if (current == QLatin1Char('['))
            {
                statement.append(current);
                state = State::BracketQuoted;
            }
            else if (current == QLatin1Char(';'))
            {
                if (!execute())
                {
                    return false;
                }
            }
            else
            {
                statement.append(current);
            }
            break;

        case State::SingleQuoted:
            statement.append(current);
            if (current == QLatin1Char('\''))
            {
                if (next == QLatin1Char('\''))
                {
                    statement.append(next);
                    ++index;
                }
                else
                {
                    state = State::Normal;
                }
            }
            break;

        case State::DoubleQuoted:
            statement.append(current);
            if (current == QLatin1Char('"'))
            {
                if (next == QLatin1Char('"'))
                {
                    statement.append(next);
                    ++index;
                }
                else
                {
                    state = State::Normal;
                }
            }
            break;

        case State::BacktickQuoted:
            statement.append(current);
            if (current == QLatin1Char('`'))
            {
                if (next == QLatin1Char('`'))
                {
                    statement.append(next);
                    ++index;
                }
                else
                {
                    state = State::Normal;
                }
            }
            break;

        case State::BracketQuoted:
            statement.append(current);
            if (current == QLatin1Char(']'))
            {
                state = State::Normal;
            }
            break;

        case State::LineComment:
            if (current == QLatin1Char('\n'))
            {
                statement.append(current);
                state = State::Normal;
            }
            break;

        case State::BlockComment:
            if (current == QLatin1Char('*') && next == QLatin1Char('/'))
            {
                statement.append(QLatin1Char(' '));
                ++index;
                state = State::Normal;
            }
            break;
        }
    }

    return execute();
}

} // namespace ClassMngr::Next::Test
