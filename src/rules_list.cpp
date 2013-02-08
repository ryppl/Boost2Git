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

#include "rules_list.hpp"
#include <QDebug>

RulesList::RulesList(const QString &filenames) : m_filenames(filenames)
  {
  }

RulesList::~RulesList()
  {
  }

void RulesList::load()
  {
  foreach(const QString filename, m_filenames.split(','))
    {
    qDebug() << "Loading rules from:" << filename;
    Rules *rules = new Rules(filename);
    m_rules.append(rules);
    rules->load();
    m_allrepositories.append(rules->repositories());
    QList<Rules::Match> matchRules = rules->matchRules();
    m_allMatchRules.append( QList<Rules::Match>(matchRules));
    }
  }

const QList<Rules::Repository> RulesList::allRepositories() const
  {
  return m_allrepositories;
  }

const QList<QList<Rules::Match> > RulesList::allMatchRules() const
  {
  return m_allMatchRules;
  }

const QList<Rules*> RulesList::rules() const
  {
  return m_rules;
  }
