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

#ifndef EXECUTABLESLIST_H
#define EXECUTABLESLIST_H

#include "executableinfo.h"

#include <vector>

#include <QFileInfo>
#include <QMetaType>

namespace MOBase { class IPluginGame; }

/*!
 * @brief Information about an executable
 **/
class Executable
{
public:
  enum Flag
  {
    CustomExecutable = 0x01,
    ShowInToolbar = 0x02,
    UseApplicationIcon = 0x04,

    AllFlags = 0xff //I know, I know
  };

  Q_DECLARE_FLAGS(Flags, Flag);

  Executable(
    QString title, QFileInfo binaryInfo, QString arguments,
    QString steamAppID, QString workingDirectory, Flags flags);

  const QString& title() const;
  void setTitle(const QString& s);

  const QFileInfo& binaryInfo() const;
  void setBinaryInfo(const QFileInfo& fi);

  const QString& arguments() const;
  void setArguments(const QString& s);

  const QString& steamAppID() const;
  void setSteamAppID(const QString& s);

  const QString& workingDirectory() const;
  void setWorkingDirectory(const QString& s);

  Flags flags() const;
  void setFlags(Flags f);

  bool isCustom() const;
  bool isShownOnToolbar() const;
  void setShownOnToolbar(bool state);
  bool usesOwnIcon() const;

private:
  QString m_title;
  QFileInfo m_binaryInfo;
  QString m_arguments;
  QString m_steamAppID;
  QString m_workingDirectory;
  Flags m_flags;
};


/*!
 * @brief List of executables configured to by started from MO
 **/
class ExecutablesList {
public:
  using vector_type = std::vector<Executable>;
  using iterator = vector_type::iterator;
  using const_iterator = vector_type::const_iterator;

  /**
   * standard container interface
   */
  iterator begin();
  const_iterator begin() const;
  iterator end();
  const_iterator end() const;
  std::size_t size() const;
  bool empty() const;

  /**
   * @brief initializes the list from the settings and the given plugin
   */
  void load(const MOBase::IPluginGame* game, QSettings& settings);

  /**
   * @brief writes the current list to the settings
   */
  void store(QSettings& settings);

  /**
   * @brief get an executable by name
   *
   * @param title the title of the executable to look up
   * @return the executable
   * @exception runtime_error will throw an exception if the executable is not found
   **/
  const Executable &get(const QString &title) const;

  /**
   * @brief find an executable by its name
   *
   * @param title the title of the executable to look up
   * @return the executable
   * @exception runtime_error will throw an exception if the name is not correct
   **/
  Executable &get(const QString &title);

  /**
   * @brief find an executable by a fileinfo structure
   * @param info the info object to search for
   * @return the executable
   * @exception runtime_error will throw an exception if the name is not correct
   */
  Executable &getByBinary(const QFileInfo &info);

  /**
   * @brief returns an iterator for the given executable by title, or end()
   */
  iterator find(const QString &title);
  const_iterator find(const QString &title) const;

  /**
   * @brief determine if an executable exists
   * @param title the title of the executable to look up
   * @return true if the executable exists, false otherwise
   **/
  bool titleExists(const QString &title) const;

  /**
   * @brief add a new executable to the list
   * @param executable
   */
  void addExecutable(const Executable &executable);

  /**
   * @brief add a new executable to the list
   *
   * @param title name displayed in the UI
   * @param executableName the actual filename to execute
   * @param arguments arguments to pass to the executable
   **/
  void addExecutable(const QString &title,
                     const QString &executableName,
                     const QString &arguments,
                     const QString &workingDirectory,
                     const QString &steamAppID,
                     Executable::Flags flags)
  {
    updateExecutable(title, executableName, arguments, workingDirectory,
                     steamAppID, Executable::AllFlags, flags);
  }

  /**
   * @brief Update an executable to the list
   *
   * @param title name displayed in the UI
   * @param executableName the actual filename to execute
   * @param arguments arguments to pass to the executable
   * @param closeMO if true, MO will be closed when the binary is started
   **/
  void updateExecutable(const QString &title,
                        const QString &executableName,
                        const QString &arguments,
                        const QString &workingDirectory,
                        const QString &steamAppID,
                        Executable::Flags mask,
                        Executable::Flags flags);

  /**
   * @brief remove the executable with the specified file name. This needs to be an absolute file path
   *
   * @param title title of the executable to remove
   * @note if the executable name is invalid, nothing happens. There is no way to determine if this was successful
   **/
  void remove(const QString &title);

private:
  std::vector<Executable> m_Executables;

  void addExecutableInternal(const QString &title, const QString &executableName, const QString &arguments,
                             const QString &workingDirectory,
                             const QString &steamAppID);

  /**
  * @brief add the executables preconfigured for this game
  **/
  void addFromPlugin(MOBase::IPluginGame const *game);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Executable::Flags)

#endif // EXECUTABLESLIST_H
