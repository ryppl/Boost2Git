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

#ifndef RULES_LIST_HPP
#define RULES_LIST_HPP

#include <QString>
#include "rules.hpp"

class RulesList
  {
  public:
    RulesList(const QString &filenames);
    ~RulesList();

    const QList<Rules::Repository> allRepositories() const;
    const QList<QList<Rules::Match> > allMatchRules() const;
    const QList<Rules*> rules() const;
    void load();

  private:
    QString m_filenames;
    QList<Rules*> m_rules;
    QList<Rules::Repository> m_allrepositories;
    QList<QList<Rules::Match> > m_allMatchRules;
  };

#endif /* RULES_LIST_HPP */
