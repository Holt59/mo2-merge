#ifndef ENV_ENVFS_H
#define ENV_ENVFS_H

#include <thread>

namespace env
{

struct File
{
  std::wstring name;
  FILETIME ft;
};

struct Directory
{
  std::wstring name;
  std::list<Directory> dirs;
  std::list<File> files;
};


template <class T>
class ThreadPool
{
public:
  ThreadPool(std::size_t max=1)
    : m_threads(max)
  {
  }

  ~ThreadPool()
  {
    join();
  }

  void setMax(std::size_t n)
  {
    m_threads.resize(n);
  }

  void join()
  {
    for (auto& ti : m_threads) {
      if (ti.thread.joinable()) {
        ti.thread.join();
      }
    }
  }

  T& request()
  {
    if (m_threads.empty()) {
      std::terminate();
    }

    for (;;) {
      for (auto& ti : m_threads) {
        bool expected = false;

        if (ti.busy.compare_exchange_strong(expected, true)) {
          if (ti.thread.joinable()) {
            ti.thread.join();
          }

          ti.thread = std::thread([&]{
            ti.o.run();
            ti.busy = false;
          });

          return ti.o;
        }
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

private:
  struct ThreadInfo
  {
    std::thread thread;
    std::atomic<bool> busy;
    T o;
  };

  std::list<ThreadInfo> m_threads;
};


using DirStartF = void (void*, std::wstring_view);
using DirEndF = void (void*, std::wstring_view);
using FileF = void (void*, std::wstring_view, FILETIME);

void setHandleCloserThreadCount(std::size_t n);

void forEachEntry(
  const std::wstring& path, void* cx,
  DirStartF* dirStartF, DirEndF* dirEndF, FileF* fileF);

Directory getFilesAndDirs(const std::wstring& path);

} // namespace

#endif // ENV_ENVFS_H
