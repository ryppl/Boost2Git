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

#ifndef LOGGING_PROCESS_HPP
#define LOGGING_PROCESS_HPP

#include <QFile>
#include <QProcess>
#include "options.hpp"

class LoggingQProcess: public QProcess
  {
  public:
    LoggingQProcess(const QString filename)
      {
      if (options.debug_rules)
        {
        logging = true;
        QString name = filename;
        name.replace('/', '_');
        name.prepend("gitlog-");
        log.setFileName(name);
        log.open(QIODevice::WriteOnly);
        }
      else
        {
        logging = false;
        }
      }
    ~LoggingQProcess()
      {
      if (logging)
        {
        log.close();
        }
      }
    qint64 write(const char *data)
      {
      Q_ASSERT(state() == QProcess::Running);
      if (logging)
        {
        log.write(data);
        }
      return QProcess::write(data);
      }
    qint64 write(const char *data, qint64 length)
      {
      Q_ASSERT(state() == QProcess::Running);
      if (logging)
        {
        log.write(data, length);
        }
      return QProcess::write(data, length);
      }
    qint64 write(const QByteArray &data)
      {
      Q_ASSERT(state() == QProcess::Running);
      if (logging)
        {
        log.write(data);
        }
      return QProcess::write(data);
      }
    qint64 writeNoLog(const char *data)
      {
      Q_ASSERT(state() == QProcess::Running);
      return QProcess::write(data);
      }
    qint64 writeNoLog(const char *data, qint64 length)
      {
      Q_ASSERT(state() == QProcess::Running);
      return QProcess::write(data, length);
      }
    qint64 writeNoLog(const QByteArray &data)
      {
      Q_ASSERT(state() == QProcess::Running);
      return QProcess::write(data);
      }
    bool putChar(char c)
      {
      Q_ASSERT(state() == QProcess::Running);
      if (logging)
        {
        log.putChar(c);
        }
      return QProcess::putChar(c);
      }
  private:
    QFile log;
    bool logging;
  };

#endif /* LOGGING_PROCESS_HPP */
