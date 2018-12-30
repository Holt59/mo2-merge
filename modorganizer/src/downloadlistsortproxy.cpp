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

#include "downloadlistsortproxy.h"
#include "downloadlist.h"
#include "downloadmanager.h"
#include "settings.h"

DownloadListSortProxy::DownloadListSortProxy(const DownloadManager *manager, QObject *parent)
  : QSortFilterProxyModel(parent), m_Manager(manager), m_CurrentFilter()
{
}

void DownloadListSortProxy::updateFilter(const QString &filter)
{
  m_CurrentFilter = filter;
  invalidateFilter();
}


bool DownloadListSortProxy::lessThan(const QModelIndex &left,
                                     const QModelIndex &right) const
{
  int leftIndex  = left.data().toInt();
  int rightIndex = right.data().toInt();
  if ((leftIndex < m_Manager->numTotalDownloads())
      && (rightIndex < m_Manager->numTotalDownloads())) {
    if (left.column() == DownloadList::COL_NAME) {
      return m_Manager->getFileName(left.row()).compare(m_Manager->getFileName(right.row()), Qt::CaseInsensitive) < 0;
    } else if (left.column() == DownloadList::COL_STATUS) {
      DownloadManager::DownloadState leftState = m_Manager->getState(left.row());
      DownloadManager::DownloadState rightState = m_Manager->getState(right.row());
      if (leftState == rightState)
        return m_Manager->getFileTime(left.row()) > m_Manager->getFileTime(right.row());
      else
        return leftState < rightState;
    } else if(left.column() == DownloadList::COL_SIZE){
      return m_Manager->getFileSize(left.row()) < m_Manager->getFileSize(right.row());
    } else {
      return leftIndex < rightIndex;
    }
  } else {
    return leftIndex < rightIndex;
  }
}


bool DownloadListSortProxy::filterAcceptsRow(int sourceRow, const QModelIndex&) const
{
  if (m_CurrentFilter.length() == 0) {
    return true;
  } else if (sourceRow < m_Manager->numTotalDownloads()) {
    QString displayedName = Settings::instance().metaDownloads()
        ? m_Manager->getDisplayName(sourceRow)
        : m_Manager->getFileName(sourceRow);
    return displayedName.contains(m_CurrentFilter, Qt::CaseInsensitive);
  } else {
    return false;
  }
}
