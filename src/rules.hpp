/*
 *  Copyright (C) 2007  Thiago Macieira <thiago@kde.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef RULES_HPP
#define RULES_HPP

#include <QList>
#include <QMap>
#include <QRegExp>
#include <QString>
#include <QStringList>
#include <QStringBuilder>

class Rules
  {
  public:
    struct Rule
      {
      Rule() : lineNumber(0)
        {
        }
      QString filename;
      int lineNumber;
      };
    struct Repository: Rule
      {
      struct Branch
        {
        QString name;
        };
      QString name;
      QList<Branch> branches;
      QString description;
      QString forwardTo;
      QString prefix;
      Repository()
        {
        }
      const QString info() const
        {
        const QString info = Rule::filename % ":" % QByteArray::number(Rule::lineNumber);
        return info;
        }
      };
    struct Match: Rule
      {
      struct Substitution
        {
        QRegExp pattern;
        QString replacement;
        bool isValid()
          {
          return !pattern.isEmpty();
          }
        QString& apply(QString &string)
          {
          return string.replace(pattern, replacement);
          }
        };
      Match() : minRevision(-1), maxRevision(-1), annotate(false), action(Ignore)
        {
        }
      const QString info() const
        {
        const QString info = Rule::filename % ":" % QByteArray::number(Rule::lineNumber) % " " % rx.pattern();
        return info;
        }
      QRegExp rx;
      QString repository;
      QList<Substitution> repo_substs;
      QString branch;
      QList<Substitution> branch_substs;
      QString prefix;
      int minRevision;
      int maxRevision;
      bool annotate;
      enum Action { Ignore, Export, Recurse } action;
      };
  public:
    Rules(const QString &filename);
    ~Rules();
  public:
    const QList<Repository> repositories() const;
    const QList<Match> matchRules() const;
    Match::Substitution parseSubstitution(const QString &string);
    void load();
  private:
    void load(const QString &filename);
    QString filename;
    QList<Repository> m_repositories;
    QList<Match> m_matchRules;
    QMap<QString,QString> m_variables;
  };

#ifndef QT_NO_DEBUG_STREAM
class QDebug;
QDebug operator<<(QDebug, const Rules::Match &);
#endif

#endif /* RULES_HPP */
