/*
 * Copyright 2017 Google Inc.
 * Copyright 2020 The Open GEE Contributors 
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __geFilePoolImpl_h
#define __geFilePoolImpl_h

#include <khMTTypes.h>
#include <fcntl.h>
#include <string>
#include <memory>

class geFilePool;

class AbstractFileIdentifier {
public:
  virtual bool isValid() { return false; }
  virtual void invalidate() {};
  virtual int getAsFD() { return -1; }

  virtual int FsyncAndClose() = 0;
  virtual int Close() = 0;
  virtual bool PreadAll(void* buffer, size_t size, off64_t offset) = 0;
  virtual bool PwriteAll(const void* buffer, size_t size, off64_t offset) = 0;
};

class POSIXFileIdentifier: public AbstractFileIdentifier {
  int fd;
public:
  POSIXFileIdentifier(int i = -1) : fd{ i } {}
  bool isValid() override { return fd != -1; }
  void invalidate() override { fd = -1; }
  int getAsFD() override { return fd; }

  int FsyncAndClose() override;
  int Close() override;
  bool PreadAll(void* buffer, size_t size, off64_t offset) override;
  bool PwriteAll(const void* buffer, size_t size, off64_t offset) override;
};

class FileReservationImpl : public khMTRefCounter {
  bool isWriter;
  std::unique_ptr<AbstractFileIdentifier> fid;
 public:
  std::unique_ptr<AbstractFileIdentifier> Fid(void) const { return fid; }
  FileReservationImpl(void) : isWriter(false) { }
  ~FileReservationImpl(void) {
    assert(!(fid.isValid()));
    isWriter = false;
  }

  // Thin wrappers around khOpen & khClose
  // set (or clear) fd
  // no exceptions
  // return bool success and set errno
  bool UnlockAndClose_(geFilePool &pool); // will set fd to -1
  bool UnlockAndOpen_(geFilePool &pool, const std::string &fname,
                      int flags, mode_t createMask);
};
typedef khRefGuard<FileReservationImpl> FileReservation;


class SignalingFileReservation : public FileReservation {
  geFilePool &pool;
 public:
   SignalingFileReservation(geFilePool &pool_, const FileReservation &o);
  ~SignalingFileReservation(void);
};


class geFilePool;
class FileReferenceImpl : public khMTRefCounter {
  friend class geFilePool;

  geFilePool &pool;
  const std::string fname;
  FileReservation reservation;
  bool   operationPending;
  int    closeError;
  int    openFlags;
  mode_t createMask;
  std::uint64_t cachedFilesize;

  class ChangingGuard {
    FileReferenceImpl* fileref;
   public:
    ChangingGuard(FileReferenceImpl *file);
    ~ChangingGuard(void);
  };
  friend class ChangingGuard;

  void ReleaseReservation_locked(void);
  void Open_locked(void);
  void SetOperationPending_locked(void);
  void ClearOperationPending_locked(void);
  void Dump_locked(void);
  FileReferenceImpl(geFilePool &pool_, const std::string &fname_,
                    int openFlags_, int createMask_);
  inline bool CareAboutCloseErrors(void) const { return IsWriter(); }
 public:
  ~FileReferenceImpl(void);

  SignalingFileReservation GetReservation();
  void Pread(void *buffer, size_t size, off64_t offset);
  void Pwrite(const void *buffer, size_t size, off64_t offset);
  void Close(void);

  static inline bool IsWriter(int flags) { return ((flags & O_WRONLY) ||
                                                   (flags & O_RDWR)); }
  inline bool IsWriter(void) const { return IsWriter(openFlags); }
  inline std::uint64_t Filesize(void) const { return cachedFilesize;}
};
typedef khRefGuard<FileReferenceImpl> FileReference;


class LockingFileReference : public FileReference {
  geFilePool &pool;
 public:
  LockingFileReference(geFilePool &pool_, const FileReference &o);
  ~LockingFileReference(void);
};

#endif /* __geFilePoolImpl_h */
