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

#ifndef DIRECTORYENTRY_H
#define DIRECTORYENTRY_H


#include <string>
#include <set>
#include <vector>
#include <map>
#include <cassert>
#define WIN32_MEAN_AND_LEAN
#include <Windows.h>
#include <bsatk.h>
#ifndef Q_MOC_RUN
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#endif
#include "util.h"

namespace MOShared { struct DirectoryEntryFileKey; }

namespace std
{
  template <>
  struct hash<MOShared::DirectoryEntryFileKey>
  {
    using argument_type = MOShared::DirectoryEntryFileKey;
    using result_type = std::size_t;

    inline result_type operator()(const argument_type& key) const;
  };
}


namespace MOShared
{

class DirectoryEntry;
class OriginConnection;
class FileRegister;


class FileEntry
{
public:
  typedef unsigned int Index;
  typedef boost::shared_ptr<FileEntry> Ptr;

  // a vector of {originId, {archiveName, order}}
  //
  // if a file is in an archive, archiveName is the name of the bsa and order
  // is the order of the associated plugin in the plugins list
  //
  // is a file is not in an archive, archiveName is empty and order is usually
  // -1
  typedef std::vector<std::pair<int, std::pair<std::wstring, int>>>
    AlternativesVector;

  FileEntry();
  FileEntry(Index index, const std::wstring &name, DirectoryEntry *parent);

  Index getIndex() const
  {
    return m_Index;
  }

  time_t lastAccessed() const
  {
    return m_LastAccessed;
  }

  void addOrigin(
    int origin, FILETIME fileTime, const std::wstring &archive, int order);

  // remove the specified origin from the list of origins that contain this
  // file. if no origin is left, the file is effectively deleted and true is
  // returned. otherwise, false is returned
  bool removeOrigin(int origin);

  void sortOrigins();

  // gets the list of alternative origins (origins with lower priority than
  // the primary one). if sortOrigins has been called, it is sorted by priority
  // (ascending)
  const AlternativesVector &getAlternatives() const
  {
    return m_Alternatives;
  }

  const std::wstring &getName() const
  {
    return m_Name;
  }

  int getOrigin() const
  {
    return m_Origin;
  }

  int getOrigin(bool &archive) const
  {
    archive = (m_Archive.first.length() != 0);
    return m_Origin;
  }

  const std::pair<std::wstring, int> &getArchive() const
  {
    return m_Archive;
  }

  bool isFromArchive(std::wstring archiveName = L"") const;

  // if originID is -1, uses the main origin; if this file doesn't exist in the
  // given origin, returns an empty string
  //
  std::wstring getFullPath(int originID=-1) const;

  std::wstring getRelativePath() const;

  DirectoryEntry *getParent()
  {
    return m_Parent;
  }

  void setFileTime(FILETIME fileTime) const
  {
    m_FileTime = fileTime;
  }

  FILETIME getFileTime() const
  {
    return m_FileTime;
  }

private:
  Index m_Index;
  std::wstring m_Name;
  int m_Origin;
  std::pair<std::wstring, int> m_Archive;
  AlternativesVector m_Alternatives;
  DirectoryEntry *m_Parent;
  mutable FILETIME m_FileTime;

  time_t m_LastAccessed;

  bool recurseParents(std::wstring &path, const DirectoryEntry *parent) const;
};


// represents a mod or the data directory, providing files to the tree
class FilesOrigin
{
  friend class OriginConnection;

public:
  FilesOrigin();
  FilesOrigin(const FilesOrigin &reference);

  // sets priority for this origin, but it will overwrite the existing mapping
  // for this priority, the previous origin will no longer be referenced
  void setPriority(int priority);

  int getPriority() const
  {
    return m_Priority;
  }

  void setName(const std::wstring &name);
  const std::wstring &getName() const
  {
    return m_Name;
  }

  int getID() const
  {
    return m_ID;
  }

  const std::wstring &getPath() const
  {
    return m_Path;
  }

  std::vector<FileEntry::Ptr> getFiles() const;
  FileEntry::Ptr findFile(FileEntry::Index index) const;

  void enable(bool enabled, time_t notAfter = LONG_MAX);
  bool isDisabled() const
  {
    return m_Disabled;
  }

  void addFile(FileEntry::Index index)
  {
    m_Files.insert(index);
  }

  void removeFile(FileEntry::Index index);

  bool containsArchive(std::wstring archiveName);

private:
  int m_ID;
  bool m_Disabled;
  std::set<FileEntry::Index> m_Files;
  std::wstring m_Name;
  std::wstring m_Path;
  int m_Priority;
  boost::weak_ptr<FileRegister> m_FileRegister;
  boost::weak_ptr<OriginConnection> m_OriginConnection;

  FilesOrigin(
    int ID, const std::wstring &name, const std::wstring &path, int priority,
    boost::shared_ptr<FileRegister> fileRegister,
    boost::shared_ptr<OriginConnection> originConnection);
};


class FileRegister
{
public:
  FileRegister(boost::shared_ptr<OriginConnection> originConnection);

  bool indexValid(FileEntry::Index index) const;

  FileEntry::Ptr createFile(const std::wstring &name, DirectoryEntry *parent);
  FileEntry::Ptr getFile(FileEntry::Index index) const;

  size_t size() const
  {
    return m_Files.size();
  }

  bool removeFile(FileEntry::Index index);
  void removeOrigin(FileEntry::Index index, int originID);
  void removeOriginMulti(std::set<FileEntry::Index> indices, int originID, time_t notAfter);

  void sortOrigins();

private:
  std::map<FileEntry::Index, FileEntry::Ptr> m_Files;
  boost::shared_ptr<OriginConnection> m_OriginConnection;

  FileEntry::Index generateIndex();
  void unregisterFile(FileEntry::Ptr file);
};


struct DirectoryEntryFileKey
{
  DirectoryEntryFileKey(std::wstring v)
    : value(std::move(v)), hash(getHash(value))
  {
  }

  bool operator==(const DirectoryEntryFileKey& o) const
  {
    return (value == o.value);
  }

  static std::size_t getHash(const std::wstring& value)
  {
    return std::hash<std::wstring>()(value);
  }

  const std::wstring value;
  const std::size_t hash;
};


class DirectoryEntry
{
public:
  using FileKey = DirectoryEntryFileKey;

  DirectoryEntry(
    const std::wstring &name, DirectoryEntry *parent, int originID);

  DirectoryEntry(
    const std::wstring &name, DirectoryEntry *parent, int originID,
    boost::shared_ptr<FileRegister> fileRegister,
    boost::shared_ptr<OriginConnection> originConnection);

  ~DirectoryEntry();

  void clear();

  bool isPopulated() const
  {
    return m_Populated;
  }

  bool isTopLevel() const
  {
    return m_TopLevel;
  }

  bool isEmpty() const
  {
    return m_Files.empty() && m_SubDirectories.empty();
  }

  bool hasFiles() const
  {
    return !m_Files.empty();
  }

  const DirectoryEntry *getParent() const
  {
    return m_Parent;
  }

  // add files to this directory (and subdirectories) from the specified origin.
  // That origin may exist or not
  void addFromOrigin(
    const std::wstring &originName,
    const std::wstring &directory, int priority);

  void addFromBSA(
    const std::wstring &originName, std::wstring &directory,
    const std::wstring &fileName, int priority, int order);

  void propagateOrigin(int origin);

  const std::wstring &getName() const
  {
    return m_Name;
  }

  boost::shared_ptr<FileRegister> getFileRegister()
  {
    return m_FileRegister;
  }

  bool originExists(const std::wstring &name) const;
  FilesOrigin &getOriginByID(int ID) const;
  FilesOrigin &getOriginByName(const std::wstring &name) const;
  const FilesOrigin* findOriginByID(int ID) const;

  int anyOrigin() const;

  std::vector<FileEntry::Ptr> getFiles() const;

  void getSubDirectories(
    std::vector<DirectoryEntry*>::const_iterator &begin,
    std::vector<DirectoryEntry*>::const_iterator &end) const
  {
    begin = m_SubDirectories.begin();
    end = m_SubDirectories.end();
  }

  const std::vector<DirectoryEntry*>& getSubDirectories() const
  {
    return m_SubDirectories;
  }

  template <class F>
  void forEachDirectory(F&& f) const
  {
    for (auto&& d : m_SubDirectories) {
      if (!f(*d)) {
        break;
      }
    }
  }

  template <class F>
  void forEachFile(F&& f) const
  {
    for (auto&& p : m_Files) {
      if (auto file=m_FileRegister->getFile(p.second)) {
        if (!f(*file)) {
          break;
        }
      }
    }
  }

  template <class F>
  void forEachFileIndex(F&& f) const
  {
    for (auto&& p : m_Files) {
      if (!f(p.second)) {
        break;
      }
    }
  }

  FileEntry::Ptr getFileByIndex(FileEntry::Index index) const
  {
    return m_FileRegister->getFile(index);
  }

  DirectoryEntry *findSubDirectory(
    const std::wstring &name, bool alreadyLowerCase=false) const;

  DirectoryEntry *findSubDirectoryRecursive(const std::wstring &path);

  /** retrieve a file in this directory by name.
    * @param name name of the file
    * @return fileentry object for the file or nullptr if no file matches
    */
  const FileEntry::Ptr findFile(const std::wstring &name, bool alreadyLowerCase=false) const;
  const FileEntry::Ptr findFile(const FileKey& key) const;

  bool hasFile(const std::wstring& name) const;
  bool containsArchive(std::wstring archiveName);

  // search through this directory and all subdirectories for a file by the
  // specified name (relative path).
  //
  // if directory is not nullptr, the referenced variable will be set to the
  // path containing the file
  //
  const FileEntry::Ptr searchFile(
    const std::wstring &path, const DirectoryEntry **directory=nullptr) const;

  void insertFile(const std::wstring &filePath, FilesOrigin &origin, FILETIME fileTime);

  void removeFile(FileEntry::Index index);

  // remove the specified file from the tree. This can be a path leading to a
  // file in a subdirectory
  bool removeFile(const std::wstring &filePath, int *origin = nullptr);

  /**
   * @brief remove the specified directory
   * @param path directory to remove
   */
  void removeDir(const std::wstring &path);

  bool remove(const std::wstring &fileName, int *origin);

  bool hasContentsFromOrigin(int originID) const;

  FilesOrigin &createOrigin(
    const std::wstring &originName,
    const std::wstring &directory, int priority);

  void removeFiles(const std::set<FileEntry::Index> &indices);

private:
  using FilesMap = std::map<std::wstring, FileEntry::Index>;
  using FilesLookup = std::unordered_map<FileKey, FileEntry::Index>;
  using SubDirectories = std::vector<DirectoryEntry*>;
  using SubDirectoriesLookup = std::unordered_map<std::wstring, DirectoryEntry*>;

  boost::shared_ptr<FileRegister> m_FileRegister;
  boost::shared_ptr<OriginConnection> m_OriginConnection;

  std::wstring m_Name;
  FilesMap m_Files;
  FilesLookup m_FilesLookup;
  SubDirectories m_SubDirectories;
  SubDirectoriesLookup m_SubDirectoriesLookup;

  DirectoryEntry *m_Parent;
  std::set<int> m_Origins;
  bool m_Populated;
  bool m_TopLevel;


  DirectoryEntry(const DirectoryEntry &reference);

  void insert(
    const std::wstring &fileName, FilesOrigin &origin, FILETIME fileTime,
    const std::wstring &archive, int order);

  void addFiles(
    FilesOrigin &origin, wchar_t *buffer, int bufferOffset);

  void addFiles(
    FilesOrigin &origin, BSA::Folder::Ptr archiveFolder, FILETIME &fileTime,
    const std::wstring &archiveName, int order);

  DirectoryEntry *getSubDirectory(
    const std::wstring &name, bool create, int originID = -1);

  DirectoryEntry *getSubDirectoryRecursive(
    const std::wstring &path, bool create, int originID = -1);

  void removeDirRecursive();

  void addDirectoryToList(DirectoryEntry* e);
  void removeDirectoryFromList(SubDirectories::iterator itor);

  void addFileToList(const FileEntry& f);
  void removeFileFromList(FileEntry::Index index);
  void removeFilesFromList(const std::set<FileEntry::Index>& indices);
};

} // namespace MOShared


namespace std
{
  hash<MOShared::DirectoryEntryFileKey>::result_type
  hash<MOShared::DirectoryEntryFileKey>::operator()(
    const argument_type& key) const
  {
    return key.hash;
  }
}

#endif // DIRECTORYENTRY_H
