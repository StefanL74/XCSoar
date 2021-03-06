/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2013 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "Net/DownloadManager.hpp"

#ifdef ANDROID

#include "Android/DownloadManager.hpp"
#include "Android/Main.hpp"

static AndroidDownloadManager *download_manager;

bool
Net::DownloadManager::Initialise()
{
  assert(download_manager == NULL);

  if (!AndroidDownloadManager::Initialise(Java::GetEnv()))
    return false;

  download_manager = AndroidDownloadManager::Create(Java::GetEnv(), *context);
  return download_manager != NULL;
}

void
Net::DownloadManager::BeginDeinitialise()
{
}

void
Net::DownloadManager::Deinitialise()
{
  delete download_manager;
  download_manager = NULL;
}

bool
Net::DownloadManager::IsAvailable()
{
  return download_manager != NULL;
}

void
Net::DownloadManager::AddListener(DownloadListener &listener)
{
  assert(download_manager != NULL);

  download_manager->AddListener(listener);
}

void
Net::DownloadManager::RemoveListener(DownloadListener &listener)
{
  assert(download_manager != NULL);

  download_manager->RemoveListener(listener);
}

void
Net::DownloadManager::Enumerate(DownloadListener &listener)
{
  assert(download_manager != NULL);

  download_manager->Enumerate(Java::GetEnv(), listener);
}

void
Net::DownloadManager::Enqueue(const char *uri, const TCHAR *relative_path)
{
  assert(download_manager != NULL);

  download_manager->Enqueue(Java::GetEnv(), uri, relative_path);
}

void
Net::DownloadManager::Cancel(const TCHAR *relative_path)
{
  assert(download_manager != NULL);

  download_manager->Cancel(Java::GetEnv(), relative_path);
}

#else /* !ANDROID */

#include "ToFile.hpp"
#include "Session.hpp"
#include "Thread/StandbyThread.hpp"
#include "Util/tstring.hpp"
#include "Operation/Operation.hpp"
#include "LocalPath.hpp"
#include "OS/FileUtil.hpp"

#include <list>
#include <algorithm>

#include <string.h>
#include <windef.h> /* for MAX_PATH */

class DownloadManagerThread gcc_final
  : protected StandbyThread, private QuietOperationEnvironment {
  struct Item {
    std::string uri;
    tstring path_relative;

    Item(const Item &other) = delete;

    Item(Item &&other)
      :uri(std::move(other.uri)),
       path_relative(std::move(other.path_relative)) {}

    Item(const char *_uri, const TCHAR *_path_relative)
      :uri(_uri), path_relative(_path_relative) {}

    Item &operator=(const Item &other) = delete;

    gcc_pure
    bool operator==(const TCHAR *relative_path) const {
      return path_relative.compare(relative_path) == 0;
    }
  };

  /**
   * Information about the current download, i.e. queue.front().
   * Protected by StandbyThread::mutex.
   */
  int64_t current_size, current_position;

  std::list<Item> queue;

  std::list<Net::DownloadListener *> listeners;

public:
  DownloadManagerThread()
    :current_size(-1), current_position(-1) {}

  void StopAsync() {
    ScopeLock protect(mutex);
    StandbyThread::StopAsync();
  }

  void WaitStopped() {
    ScopeLock protect(mutex);
    StandbyThread::WaitStopped();
  }

  void AddListener(Net::DownloadListener &listener) {
    ScopeLock protect(mutex);

    assert(std::find(listeners.begin(), listeners.end(),
                     &listener) == listeners.end());

    listeners.push_back(&listener);
  }

  void RemoveListener(Net::DownloadListener &listener) {
    ScopeLock protect(mutex);

    auto i = std::find(listeners.begin(), listeners.end(), &listener);
    assert(i != listeners.end());
    listeners.erase(i);
  }

  void Enumerate(Net::DownloadListener &listener) {
    ScopeLock protect(mutex);

    for (auto i = queue.begin(), end = queue.end(); i != end; ++i) {
      const Item &item = *i;

      int64_t size = -1, position = -1;
      if (i == queue.begin()) {
        size = current_size;
        position = current_position;
      }

      listener.OnDownloadAdded(item.path_relative.c_str(), size, position);
    }
  }

  void Enqueue(const char *uri, const TCHAR *path_relative) {
    ScopeLock protect(mutex);
    queue.push_back(Item(uri, path_relative));

    for (auto i = listeners.begin(), end = listeners.end(); i != end; ++i)
      (*i)->OnDownloadAdded(path_relative, -1, -1);

    if (!IsBusy())
      Trigger();
  }

  void Cancel(const TCHAR *relative_path) {
    ScopeLock protect(mutex);

    auto i = std::find(queue.begin(), queue.end(), relative_path);
    if (i == queue.end())
      return;

    if (i == queue.begin()) {
      /* current download; stop the thread to cancel the current file
         and restart the thread to continue downloading the following
         files */

      StandbyThread::StopAsync();
      StandbyThread::WaitStopped();

      if (!queue.empty())
        Trigger();
    } else {
      /* queued download; simply remove it from the list */
      queue.erase(i);
    }

    for (auto i = listeners.begin(), end = listeners.end(); i != end; ++i)
      (*i)->OnDownloadComplete(relative_path, false);
  }

protected:
  /* methods from class StandbyThread */
  virtual void Tick();

private:
  /* methods from class OperationEnvironment */
  virtual bool IsCancelled() const {
    ScopeLock protect(const_cast<Mutex &>(mutex));
    return StandbyThread::IsStopped();
  }

  virtual void SetProgressRange(unsigned range) gcc_override {
    ScopeLock protect(mutex);
    current_size = range;
  }

  virtual void SetProgressPosition(unsigned position) gcc_override {
    ScopeLock protect(mutex);
    current_position = position;
  }
};

void
DownloadManagerThread::Tick()
{
  Net::Session session;

  while (!queue.empty() && !StandbyThread::IsStopped()) {
    assert(current_size == -1);
    assert(current_position == -1);

    const Item &item = queue.front();
    current_position = 0;
    mutex.Unlock();

    TCHAR path[MAX_PATH];
    LocalPath(path, item.path_relative.c_str());

    TCHAR tmp[MAX_PATH];
    _tcscpy(tmp, path);
    _tcscat(tmp, _T(".tmp"));
    File::Delete(tmp);
    bool success =
      DownloadToFile(session, item.uri.c_str(), tmp, NULL, *this) &&
      File::Replace(tmp, path);

    mutex.Lock();
    current_size = current_position = -1;
    const Item copy(std::move(queue.front()));
    queue.pop_front();
    for (auto i = listeners.begin(), end = listeners.end(); i != end; ++i)
      (*i)->OnDownloadComplete(copy.path_relative.c_str(), success);
  }
}

static DownloadManagerThread *thread;

bool
Net::DownloadManager::Initialise()
{
  assert(thread == NULL);

  thread = new DownloadManagerThread();
  return true;
}

void
Net::DownloadManager::BeginDeinitialise()
{
  assert(thread != NULL);

  thread->StopAsync();
}

void
Net::DownloadManager::Deinitialise()
{
  assert(thread != NULL);

  thread->WaitStopped();
  delete thread;
}

bool
Net::DownloadManager::IsAvailable()
{
  assert(thread != NULL);

  return true;
}

void
Net::DownloadManager::AddListener(DownloadListener &listener)
{
  assert(thread != NULL);

  thread->AddListener(listener);
}

void
Net::DownloadManager::RemoveListener(DownloadListener &listener)
{
  assert(thread != NULL);

  thread->RemoveListener(listener);
}

void
Net::DownloadManager::Enumerate(DownloadListener &listener)
{
  assert(thread != NULL);

  thread->Enumerate(listener);
}

void
Net::DownloadManager::Enqueue(const char *uri, const TCHAR *relative_path)
{
  assert(thread != NULL);

  thread->Enqueue(uri, relative_path);
}

void
Net::DownloadManager::Cancel(const TCHAR *relative_path)
{
  assert(thread != NULL);

  thread->Cancel(relative_path);
}

#endif
