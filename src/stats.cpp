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


#include "stats.hpp"
#include "CommandLineParser.h"
#include <QDebug>

Stats *Stats::self = 0;

class Stats::Private
  {
  public:
    Private();
    void printStats() const;
    void ruleMatched(const Rules::Match &rule, const int rev);
    void addRule(const Rules::Match &rule);
  private:
    QMap<QString, int> m_usedRules;
  };

Stats::Stats() : d(new Private())
  {
  use = CommandLineParser::instance()->contains("stats");
  }

Stats::~Stats()
  {
  delete d;
  }

void Stats::init()
  {
  if (self)
    {
    delete self;
    }
  self = new Stats();
  }

Stats* Stats::instance()
  {
  return self;
  }

void Stats::printStats() const
  {
  if (use)
    {
    d->printStats();
    }
  }

void Stats::ruleMatched(const Rules::Match &rule, const int rev)
  {
  if (use)
    {
    d->ruleMatched(rule, rev);
    }
  }

void Stats::addRule(const Rules::Match &rule)
  {
  if (use)
    {
    d->addRule(rule);
    }
  }

Stats::Private::Private()
  {
  }

void Stats::Private::printStats() const
  {
  printf("\nRule stats\n");
  foreach(const QString name, m_usedRules.keys())
    {
    printf("%s was matched %i times\n", qPrintable(name), m_usedRules[name]);
    }
  }

void Stats::Private::ruleMatched(const Rules::Match &rule, const int rev)
  {
  Q_UNUSED(rev);
  const QString name = rule.info();
  if (!m_usedRules.contains(name))
    {
    m_usedRules.insert(name, 1);
    qWarning() << "WARN: New match rule, should have been added when created.";
    }
  else
    {
    m_usedRules[name]++;
    }
  }

void Stats::Private::addRule(const Rules::Match &rule)
  {
  const QString name = rule.info();
  if (m_usedRules.contains(name))
    {
    qWarning() << "WARN: Rule" << name << "was added multiple times.";
    }
  m_usedRules.insert(name, 0);
  }
