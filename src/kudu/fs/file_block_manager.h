// Copyright (c) 2014, Cloudera, inc.
// Confidential Cloudera Information: Covered by NDA.

#ifndef KUDU_FS_FILE_BLOCK_MANAGER_H
#define KUDU_FS_FILE_BLOCK_MANAGER_H

#include <map>
#include <string>
#include <tr1/memory>
#include <tr1/unordered_set>
#include <vector>

#include "kudu/fs/block_id.h"
#include "kudu/fs/block_manager.h"
#include "kudu/util/locks.h"
#include "kudu/util/oid_generator.h"

namespace kudu {

class Env;
class MemTracker;
class MetricEntity;
class WritableFile;

namespace fs {
class PathInstanceMetadataFile;

namespace internal {
class FileBlockLocation;
class FileReadableBlock;
class FileWritableBlock;

struct BlockManagerMetrics;
} // namespace internal

// A file-backed block storage implementation.
//
// This is a naive block implementation which maps each block to its own
// file on disk. To prevent the block directory from becoming too large,
// blocks are aggregated into a 3-level directory hierarchy.
//
// The block manager can take advantage of multiple filesystem paths. A
// small metadata file containing a UUID is persisted in each path. A block
// written to a given path will be assigned an ID that includes that path's
// UUID. The ID is resolved back into a filesystem path when the block is
// opened for reading.
//
// When creating blocks, the block manager will round robin through the
// available filesystem paths.
//
// TODO: Support path-based block placement hints.

// The file-backed block manager.
class FileBlockManager : public BlockManager {
 public:

  // Creates a new in-memory instance of a FileBlockManager.
  //
  // 'env' should remain alive for the lifetime of the block manager.
  FileBlockManager(Env* env, const BlockManagerOptions& opts);

  virtual ~FileBlockManager();

  virtual Status Create() OVERRIDE;

  virtual Status Open() OVERRIDE;

  virtual Status CreateBlock(const CreateBlockOptions& opts,
                             gscoped_ptr<WritableBlock>* block) OVERRIDE;

  virtual Status CreateBlock(gscoped_ptr<WritableBlock>* block) OVERRIDE;

  virtual Status OpenBlock(const BlockId& block_id,
                           gscoped_ptr<ReadableBlock>* block) OVERRIDE;

  virtual Status DeleteBlock(const BlockId& block_id) OVERRIDE;

  virtual Status CloseBlocks(const std::vector<WritableBlock*>& blocks) OVERRIDE;

 private:
  friend class internal::FileBlockLocation;
  friend class internal::FileReadableBlock;
  friend class internal::FileWritableBlock;

  // Synchronizes the metadata for a block with the given id.
  Status SyncMetadata(const internal::FileBlockLocation& block_id);

  // Look up a root path by its UUID in 'root_paths_by_uuid_'.
  //
  // On success, overwrites 'root_path' with the root path.
  bool FindRootPath(const std::string& root_path_uuid,
                    std::string* root_path) const;

  Env* env() const { return env_; }

  // For manipulating files.
  Env* env_;

  // Filesystem paths where all block directories are found.
  const std::vector<std::string> root_paths_;

  // Maps path UUIDs to path instance files.
  //
  // There's no need to synchronize access to the map as it is only written
  // to during Create() and Open(); all subsequent accesses are reads.
  typedef std::map<std::string, PathInstanceMetadataFile*> PathMap;
  PathMap root_paths_by_uuid_;

  // For generating block IDs.
  ObjectIdGenerator oid_generator_;

  // Protects 'dirty_dirs_' and 'next_root_path_'.
  mutable simple_spinlock lock_;

  // Tracks the block directories which are dirty from block creation. This
  // lets us perform some simple coalescing when synchronizing metadata.
  std::tr1::unordered_set<std::string> dirty_dirs_;

  // Points to the filesystem path to be used when creating the next block.
  PathMap::iterator next_root_path_;

  // Metric container for the block manager.
  // May be null if instantiated without metrics.
  gscoped_ptr<internal::BlockManagerMetrics> metrics_;

  // Tracks memory consumption of any allocations numerous enough to be
  // interesting (e.g. ReadableBlocks).
  std::tr1::shared_ptr<MemTracker> mem_tracker_;

  DISALLOW_COPY_AND_ASSIGN(FileBlockManager);
};

} // namespace fs
} // namespace kudu

#endif
