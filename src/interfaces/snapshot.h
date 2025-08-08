// Copyright (c) 2024 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_INTERFACES_SNAPSHOT_H
#define BITCOIN_INTERFACES_SNAPSHOT_H

#include <memory>
#include <uint256.h>
#include <util/fs.h>

namespace node {
class SnapshotMetadata;
}

namespace interfaces {

//! Interface for managing UTXO snapshots.
class Snapshot
{
public:
    virtual ~Snapshot() = default;

    //! Activate the snapshot, making it the active chainstate.
    virtual bool activate() = 0;

    //! Generate a UTXO snapshot of the current chainstate and write it to disk.
    //! Returns true on success, false on failure. The snapshot content matches
    //! the current tip (equivalent to the "latest" type in the dumptxoutset RPC).
    virtual bool generate(const fs::path& output_path) = 0;
};

} // namespace interfaces

#endif // BITCOIN_INTERFACES_SNAPSHOT_H
