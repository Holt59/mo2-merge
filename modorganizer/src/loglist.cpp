/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "loglist.h"
#include <scopeguard.h>
#include <report.h>
#include <log.h>
#include <QMutexLocker>
#include <QFile>
#include <QIcon>
#include <QDateTime>
#include <Windows.h>

using namespace MOBase;

static LogModel* g_instance = nullptr;
const std::size_t MaxLines = 1000;

LogModel::LogModel()
{
  connect(this, &LogModel::entryAdded, [&](auto&& e){ onEntryAdded(e); });
}

void LogModel::create()
{
  g_instance = new LogModel;
}

LogModel& LogModel::instance()
{
  return *g_instance;
}

void LogModel::add(MOBase::log::Entry e)
{
  emit entryAdded(std::move(e));
}

void LogModel::onEntryAdded(MOBase::log::Entry e)
{
  bool full = false;
  if (m_messages.size() > MaxLines) {
    m_messages.pop_front();
    full = true;
  }

  const int row = static_cast<int>(m_messages.size());

  if (!full) {
    beginInsertRows(QModelIndex(), row, row + 1);
  }

  m_messages.emplace_back(std::move(e));

  if (!full) {
    endInsertRows();
  } else {
    emit dataChanged(
      createIndex(row, 0),
      createIndex(row + 1, columnCount({})));
  }
}

QModelIndex LogModel::index(int row, int column, const QModelIndex&) const
{
  return createIndex(row, column, row);
}

QModelIndex LogModel::parent(const QModelIndex&) const
{
  return QModelIndex();
}

int LogModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid())
    return 0;
  else
    return static_cast<int>(m_messages.size());
}

int LogModel::columnCount(const QModelIndex&) const
{
  return 3;
}

QVariant LogModel::data(const QModelIndex& index, int role) const
{
  using namespace std::chrono;

  const auto row = static_cast<std::size_t>(index.row());
  if (row >= m_messages.size()) {
    return {};
  }

  const auto& e = m_messages[row];

  if (role == Qt::DisplayRole) {
    if (index.column() == 1) {
      const auto ms = duration_cast<milliseconds>(e.time.time_since_epoch());
      const auto s = duration_cast<seconds>(ms);

      const std::time_t t = s.count();
      const std::size_t frac = ms.count() % 1000;

      auto time = QDateTime::fromTime_t(t).time();
      time = time.addMSecs(frac);

      return time.toString("hh:mm:ss.zzz");
    } else if (index.column() == 2) {
      return QString::fromStdString(e.message);
    }
  }

  if (role == Qt::DecorationRole) {
    if (index.column() == 0) {
      switch (e.level) {
        case log::Warning:
          return QIcon(":/MO/gui/warning");

        case log::Error:
          return QIcon(":/MO/gui/problem");

        case log::Debug:  // fall-through
        case log::Info:
        default:
          return {};
      }
    }
  }

  return QVariant();
}

QVariant LogModel::headerData(int, Qt::Orientation, int) const
{
  return {};
}

void vlog(const char *format, ...)
{
  va_list argList;
  va_start(argList, format);

  static const int BUFFERSIZE = 1000;

  char buffer[BUFFERSIZE + 1];
  buffer[BUFFERSIZE] = '\0';

  vsnprintf(buffer, BUFFERSIZE, format, argList);

  qCritical("%s", buffer);

  va_end(argList);
}
