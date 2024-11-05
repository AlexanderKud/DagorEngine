//
// Dagor Engine 6.5 - Game Libraries
// Copyright (C) Gaijin Games KFT.  All rights reserved.
//
#pragma once

#include <daScript/daScript.h>
#include "daScript/simulate/fs_file_info.h"
#include <daScript/misc/sysos.h>
#include <osApiWrappers/dag_files.h>
#include <osApiWrappers/dag_vromfs.h>
#include <osApiWrappers/dag_basePath.h>
#include <osApiWrappers/dag_direct.h>
#include <ska_hash_map/flat_hash_map2.hpp>

#define DASLIB_MODULE_NAME "daslib"

namespace bind_dascript
{
enum class HotReload
{
  DISABLED,
  ENABLED,
};
enum class LoadDebugCode
{
  NO,
  YES,
};

class FsFileInfo final : public das::FileInfo
{
  const char *source = nullptr;
  uint32_t sourceLength = 0;
  bool owner = false;

public:
  void freeSourceData() override
  {
    if (owner && source)
      memfree_anywhere((void *)source);
    owner = false;
    source = nullptr;
    sourceLength = 0u;
  }
  virtual void getSourceAndLength(const char *&src, uint32_t &len)
  {
    if (owner)
    {
      src = source;
      len = sourceLength;
      return;
    }
    src = nullptr;
    len = 0;
    file_ptr_t f = df_open(name.c_str(), DF_READ | DF_IGNORE_MISSING);
    if (!f)
    {
      logerr("das: script file %s not found", name.c_str());
      return;
    }
    int fileLength = 0;
    src = df_get_vromfs_file_data_for_file_ptr(f, fileLength);
    if (src)
    {
      len = (uint32_t)fileLength;
      df_close(f);
      return;
    }
    fileLength = df_length(f);
    source = (char *)memalloc(fileLength, strmem);
    G_VERIFY(df_read(f, (void *)source, fileLength) == fileLength);

    owner = true;
    src = source;
    sourceLength = len = (uint32_t)fileLength;
    df_close(f);
  }

  virtual ~FsFileInfo() { freeSourceData(); }
};


class DagFileAccess : public das::ModuleFileAccess
{
  das::das_map<das::string, das::string> extraRoots;
  das::FileAccess *localAccess;

public:
  bool verbose = true;

private:
  das::FileAccessPtr makeLocalFileAccess()
  {
    auto res = das::make_smart<DagFileAccess>(bind_dascript::HotReload::DISABLED);
    localAccess = res.get();
    localAccess->addRef();
    return res;
  }

public:
  ska::flat_hash_map<eastl::string, int64_t, ska::power_of_two_std_hash<eastl::string>> filesOpened; // in order to reload changed
                                                                                                     // scripts, we would need to store
                                                                                                     // pointers to FsFileInfo in
                                                                                                     // script
  bool storeOpenedFiles = true;
  bool derivedAccess = false;
  DagFileAccess *owner = nullptr;
  DagFileAccess(const char *pak = nullptr, HotReload allow_hot_reload = HotReload::ENABLED) // -V730 Not all members of a class are
                                                                                            // initialized inside the constructor.
                                                                                            // Consider inspecting: localAccess. (@see
                                                                                            // makeLocalFileAccess)
    :
    das::ModuleFileAccess(pak, makeLocalFileAccess()), storeOpenedFiles(allow_hot_reload == HotReload::ENABLED)
  {}
  DagFileAccess(HotReload allow_hot_reload = HotReload::ENABLED) : storeOpenedFiles(allow_hot_reload == HotReload::ENABLED)
  {
    localAccess = nullptr;
  }
  DagFileAccess(DagFileAccess *modAccess, HotReload allow_hot_reload = HotReload::ENABLED) :
    storeOpenedFiles(allow_hot_reload == HotReload::ENABLED), owner(modAccess)
  {
    localAccess = nullptr;
    if (modAccess)
    {
      context = modAccess->context;
      modGet = modAccess->modGet;
      includeGet = modAccess->includeGet;
      moduleAllowed = modAccess->moduleAllowed;
      verbose = modAccess->verbose;
      derivedAccess = true;
    }
  }
  virtual ~DagFileAccess() override
  {
    if (derivedAccess)
      context = nullptr;
    if (localAccess)
      localAccess->delRef();
  }
  int64_t getFileMtime(const das::string &fileName) const override
  {
    DagorStat stat;
    auto res = df_stat(fileName.c_str(), &stat) >= 0 ? stat.mtime : -1;
    if (res >= 0)
      return res;
#if _TARGET_ANDROID
    VirtualRomFsData *vrom = NULL;
    VromReadHandle data = vromfs_get_file_data(fileName.c_str(), &vrom);
    if (vrom && vrom->version != 0)
      return vrom->version;
#endif
    return 0;
  }
  bool invalidateFileInfo(const das::string &fileName) override
  {
    bool res = false;
    if (owner)
      res = owner->invalidateFileInfo(fileName) || res;
    res = das::ModuleFileAccess::invalidateFileInfo(fileName) || res;
    return res;
  }
  virtual das::FileInfo *getNewFileInfo(const das::string &fname) override
  {
    if (owner)
    {
      auto res = owner->getFileInfo(fname);
      if (storeOpenedFiles && res)
      {
        auto it = owner->filesOpened.find(fname);
        if (DAGOR_LIKELY(it != owner->filesOpened.end()))
        {
          filesOpened.emplace(fname, it->second);
        }
        else
        {
          filesOpened.emplace(fname, getFileMtime(fname));
        }
      }
      return res;
    }
    file_ptr_t f = df_open(fname.c_str(), DF_READ | DF_IGNORE_MISSING);
    if (!f)
    {
      if (verbose)
        logerr("Script file %s not found", fname.c_str());
      return nullptr;
    }
    if (storeOpenedFiles)
    {
      filesOpened.emplace(fname, getFileMtime(fname));
    }
    df_close(f);
    return setFileInfo(fname, das::make_unique<bind_dascript::FsFileInfo>());
  }

  virtual das::ModuleInfo getModuleInfo(const das::string &req, const das::string &from) const override
  {
    if (!failed())
      return das::ModuleFileAccess::getModuleInfo(req, from);
    auto np = req.find_first_of("./");
    if (np != das::string::npos)
    {
      das::string top = req.substr(0, np);
      das::string mod_name = req.substr(np + 1);
      das::ModuleInfo info;
      if (top == DASLIB_MODULE_NAME)
      {
        info.moduleName = mod_name;
        info.fileName = das::getDasRoot() + "/" + DASLIB_MODULE_NAME + "/" + info.moduleName + ".das";
        return info;
      }
      const auto extraRemappedPath = extraRoots.find(top);
      if (extraRemappedPath != extraRoots.end())
      {
        info.moduleName = mod_name;
        info.fileName = das::string(das::string::CtorSprintf(), "%s/%s.das", extraRemappedPath->second.c_str(), mod_name.c_str());
        return info;
      }
    }
    bool reqUsesMountPoint = strncmp(req.c_str(), "%", 1) == 0;
    return das::ModuleFileAccess::getModuleInfo(req, reqUsesMountPoint ? das::string() : from);
  }

  das::string getIncludeFileName(const das::string &fileName, const das::string &incFileName) const override
  {
    const bool incUsesMount = strncmp(incFileName.c_str(), "%", 1) == 0;
    return das::ModuleFileAccess::getIncludeFileName(incUsesMount ? das::string() : fileName, incFileName);
  }
  virtual bool addFsRoot(const das::string &mod, const das::string &path) override
  {
    extraRoots[mod] = path;
    return true;
  }
  virtual bool isSameFileName(const das::string &f1, const das::string &f2) const override
  {
    G_UNUSED(f1);
    G_UNUSED(f2);
    return true;
    // TODO: implement this
    // das::string fixed1;
    // das::string fixed2;
    // G_ASSERT(f1.size() < DAGOR_MAX_PATH && f2.size() < DAGOR_MAX_PATH);
    // if (!dd_resolve_named_mount(fixed1, f1.c_str()))
    //   fixed1 = f1;
    // if (!dd_resolve_named_mount(fixed2, f2.c_str()))
    //   fixed2 = f2;
    // dd_simplify_fname_c(fixed1.data());
    // dd_simplify_fname_c(fixed2.data());
    // fixed1.force_size(strlen(fixed1.data()));
    // fixed2.force_size(strlen(fixed2.data()));
    // return das::ModuleFileAccess::isSameFileName(fixed1, fixed2);
  }
};
} // namespace bind_dascript
