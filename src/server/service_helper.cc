// Copyright (c) 2023 dingodb.com, Inc. All Rights Reserved
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "server/service_helper.h"

#include <cassert>
#include <cstdint>
#include <string>
#include <string_view>

#include "butil/status.h"
#include "common/helper.h"
#include "common/logging.h"
#include "document/codec.h"
#include "fmt/core.h"
#include "gflags/gflags.h"
#include "proto/error.pb.h"
#include "server/server.h"
#include "vector/codec.h"

namespace dingodb {

DEFINE_int64(service_log_threshold_time_ns, 1000000000L, "service log threshold time");
DEFINE_uint32(log_print_max_length, 512, "log print max length");

bvar::LatencyRecorder g_raw_latches_recorder("dingo_latches_raw");
bvar::LatencyRecorder g_txn_latches_recorder("dingo_latches_txn");

void ServiceHelper::SetError(pb::error::Error* error, int errcode, const std::string& errmsg) {
  error->set_errcode(static_cast<pb::error::Errno>(errcode));
  error->set_errmsg(errmsg);
}

void ServiceHelper::SetError(pb::error::Error* error, const std::string& errmsg) { error->set_errmsg(errmsg); }

butil::Status ServiceHelper::ValidateRegionEpoch(const pb::common::RegionEpoch& req_epoch, store::RegionPtr region) {
  if (region->Epoch().conf_version() != req_epoch.conf_version() || region->Epoch().version() != req_epoch.version()) {
    return butil::Status(pb::error::Errno::EREGION_VERSION,
                         fmt::format("Region({}) epoch is not match, region_epoch({}_{}) req_epoch({}_{})",
                                     region->Id(), region->Epoch().conf_version(), region->Epoch().version(),
                                     req_epoch.conf_version(), req_epoch.version()));
  }

  return butil::Status::OK();
}

butil::Status ServiceHelper::GetStoreRegionInfo(store::RegionPtr region, pb::error::Error* error) {
  assert(region != nullptr);

  if (error == nullptr) {
    return butil::Status(pb::error::EINTERNAL, "Error is nullptr");
  }

  if (error->errcode() != pb::error::EREGION_VERSION) {
    return butil::Status(pb::error::EINTERNAL, "Not need set store region info");
  }

  auto* store_region_info = error->mutable_store_region_info();
  store_region_info->set_region_id(region->Id());
  *(store_region_info->mutable_current_region_epoch()) = region->Epoch();
  *(store_region_info->mutable_current_range()) = region->Range(false);
  for (const auto& peer : region->Peers()) {
    *(store_region_info->add_peers()) = peer;
  }

  return butil::Status::OK();
}

// Validate region state
butil::Status ServiceHelper::ValidateRegionState(store::RegionPtr region) {
  // Check is exist region.
  if (region == nullptr) {
    return butil::Status(pb::error::EREGION_NOT_FOUND, "Not found region");
  }
  if (region->State() == pb::common::StoreRegionState::NEW) {
    return butil::Status(pb::error::EREGION_UNAVAILABLE, "Region(%lu) is new, waiting later", region->Id());
  }
  if (region->State() == pb::common::StoreRegionState::STANDBY) {
    return butil::Status(pb::error::EREGION_UNAVAILABLE, "Region(%lu) is standby, waiting later", region->Id());
  }
  if (region->State() == pb::common::StoreRegionState::DELETING) {
    return butil::Status(pb::error::EREGION_UNAVAILABLE, "Region(%lu) is deleting", region->Id());
  }
  if (region->State() == pb::common::StoreRegionState::DELETED) {
    return butil::Status(pb::error::EREGION_UNAVAILABLE, "Region(%lu) is deleted", region->Id());
  }
  if (region->State() == pb::common::StoreRegionState::ORPHAN) {
    return butil::Status(pb::error::EREGION_UNAVAILABLE, "Region(%lu) is orphan", region->Id());
  }
  if (region->State() == pb::common::StoreRegionState::TOMBSTONE) {
    return butil::Status(pb::error::EREGION_UNAVAILABLE, "Region(%lu) is tombstone", region->Id());
  }

  return butil::Status();
}

butil::Status ServiceHelper::ValidateRange(const pb::common::Range& range) {
  if (BAIDU_UNLIKELY(range.start_key().empty() || range.end_key().empty())) {
    return butil::Status(pb::error::EILLEGAL_PARAMTETERS, "Range key is empty");
  }

  if (BAIDU_UNLIKELY(range.start_key() >= range.end_key())) {
    DINGO_LOG(ERROR) << fmt::format("pb::error::ERANGE_INVALID, Range is invalid range: {}",
                                    Helper::RangeToString(range));
    return butil::Status(pb::error::ERANGE_INVALID, "Range is invalid");
  }

  return butil::Status();
}

// Validate key in range
butil::Status ServiceHelper::ValidateKeyInRange(const pb::common::Range& range,
                                                const std::vector<std::string_view>& keys) {
  for (const auto& key : keys) {
    if (range.start_key().compare(key) > 0 || range.end_key().compare(key) <= 0) {
      return butil::Status(pb::error::EKEY_OUT_OF_RANGE,
                           fmt::format("Key out of range, region range{} key[{}]", Helper::RangeToString(range),
                                       Helper::StringToHex(key)));
    }
  }

  return butil::Status();
}

// Validate range in range [)
butil::Status ServiceHelper::ValidateRangeInRange(const pb::common::Range& region_range,
                                                  const pb::common::Range& req_range) {
  // Validate start_key
  int min_length = std::min(region_range.start_key().size(), req_range.start_key().size());
  std::string_view req_truncate_start_key(req_range.start_key().data(), min_length);
  std::string_view region_truncate_start_key(region_range.start_key().data(), min_length);
  if (req_truncate_start_key < region_truncate_start_key) {
    return butil::Status(pb::error::EKEY_OUT_OF_RANGE,
                         fmt::format("Key out of range, region range{} req range{}",
                                     Helper::RangeToString(region_range), Helper::RangeToString(req_range)));
  }

  // Validate end_key
  min_length = std::min(region_range.end_key().size(), req_range.end_key().size());
  std::string_view req_truncate_end_key(req_range.end_key().data(), min_length);
  std::string_view region_truncate_end_key(region_range.end_key().data(), min_length);

  std::string next_prefix_key;
  if (req_range.end_key().size() > region_range.end_key().size()) {
    next_prefix_key = Helper::PrefixNext(req_truncate_end_key);
    req_truncate_end_key = std::string_view(next_prefix_key.data(), next_prefix_key.size());
  } else if (req_range.end_key().size() < region_range.end_key().size()) {
    next_prefix_key = Helper::PrefixNext(region_truncate_end_key);
    region_truncate_end_key = std::string_view(next_prefix_key.data(), next_prefix_key.size());
  }

  if (req_truncate_end_key > region_truncate_end_key) {
    return butil::Status(pb::error::EKEY_OUT_OF_RANGE,
                         fmt::format("Key out of range, region range{} req range{}",
                                     Helper::RangeToString(region_range), Helper::RangeToString(req_range)));
  }

  return butil::Status();
}

butil::Status ServiceHelper::ValidateRegion(store::RegionPtr region, const std::vector<std::string_view>& keys) {
  auto status = ValidateRegionState(region);
  if (!status.ok()) {
    return status;
  }

  // for table region, Range is always equal to Range, so here we can use Range to validate
  status = ValidateKeyInRange(region->Range(false), keys);
  if (!status.ok()) {
    return status;
  }

  return butil::Status();
}

butil::Status ServiceHelper::ValidateIndexRegion(store::RegionPtr region, const std::vector<int64_t>& vector_ids) {
  auto status = ValidateRegionState(region);
  if (!status.ok()) {
    return status;
  }

  const auto& range = region->Range(false);
  int64_t min_vector_id = 0, max_vector_id = 0;
  VectorCodec::DecodeRangeToVectorId(false, range, min_vector_id, max_vector_id);
  for (auto vector_id : vector_ids) {
    if (vector_id < min_vector_id || vector_id >= max_vector_id) {
      return butil::Status(pb::error::EKEY_OUT_OF_RANGE,
                           fmt::format("EKEY_OUT_OF_RANGE, region range{} / [{}-{}) req vector id {}",
                                       Helper::RangeToString(range), min_vector_id, max_vector_id, vector_id));
    }
  }

  return butil::Status();
}

butil::Status ServiceHelper::ValidateDocumentRegion(store::RegionPtr region, const std::vector<int64_t>& document_ids) {
  auto status = ValidateRegionState(region);
  if (!status.ok()) {
    return status;
  }

  const auto& range = region->Range(false);
  int64_t min_document_id = 0, max_document_id = 0;
  DocumentCodec::DecodeRangeToDocumentId(false, range, min_document_id, max_document_id);
  for (auto document_id : document_ids) {
    if (document_id < min_document_id || document_id >= max_document_id) {
      return butil::Status(pb::error::EKEY_OUT_OF_RANGE,
                           fmt::format("EKEY_OUT_OF_RANGE, region range{} / [{}-{}) req vector id {}",
                                       Helper::RangeToString(range), min_document_id, max_document_id, document_id));
    }
  }

  return butil::Status();
}

// if one store is set to read-only, all stores are set to read-only
// this flag is set by coordinator and send to all stores using store heartbeat
butil::Status ServiceHelper::ValidateClusterReadOnly() {
  auto is_read_only_or_force_read_only = Server::GetInstance().IsClusterReadOnlyOrForceReadOnly();
  if (!is_read_only_or_force_read_only) {
    return butil::Status();
  }

  if (Server::GetInstance().IsClusterReadOnly()) {
    auto reason = Server::GetInstance().GetClusterReadOnlyReason();
    std::string s = fmt::format("cluster is set to read-only from coordinator, reason: {}", reason);

    DINGO_LOG(WARNING) << s;
    return butil::Status(pb::error::ESYSTEM_CLUSTER_READ_ONLY, s);
  }

  if (Server::GetInstance().IsClusterForceReadOnly()) {
    auto reason = Server::GetInstance().GetClusterForceReadOnlyReason();
    std::string s = fmt::format("cluster is set to read-only from coordinator, reason: {}", reason);

    DINGO_LOG(WARNING) << s;
    return butil::Status(pb::error::ESYSTEM_CLUSTER_READ_ONLY, s);
  }

  return butil::Status();
}

// LatchContextPtr ServiceHelper::LatchesAcquire(store::RegionPtr region, const std::vector<std::string>& keys,
//                                               bool is_txn) {
//   auto start_time_us = butil::gettimeofday_us();

//   auto latch_ctx = LatchContext::New(keys);

//   bool latch_got = false;
//   while (!latch_got) {
//     latch_got = region->LatchesAcquire(latch_ctx->GetLock(), latch_ctx->Cid());
//     if (!latch_got) {
//       latch_ctx->SyncCond().IncreaseWait();
//     }
//   }

//   if (is_txn) {
//     g_txn_latches_recorder << butil::gettimeofday_us() - start_time_us;
//   } else {
//     g_raw_latches_recorder << butil::gettimeofday_us() - start_time_us;
//   }

//   return latch_ctx;
// }

void ServiceHelper::LatchesAcquire(LatchContext& latch_ctx, bool is_txn) {
  auto start_time_us = butil::gettimeofday_us();

  store::RegionPtr region = latch_ctx.GetRegion();
  bool latch_got = false;
  while (!latch_got) {
    latch_got = region->LatchesAcquire(latch_ctx.GetLock(), latch_ctx.Cid());
    if (!latch_got) {
      latch_ctx.SyncCond().IncreaseWait();
    }
  }

  if (is_txn) {
    g_txn_latches_recorder << butil::gettimeofday_us() - start_time_us;
  } else {
    g_raw_latches_recorder << butil::gettimeofday_us() - start_time_us;
  }
}

void ServiceHelper::LatchesRelease(LatchContext& latch_ctx) {
  store::RegionPtr region = latch_ctx.GetRegion();
  region->LatchesRelease(latch_ctx.GetLock(), latch_ctx.Cid());
}

}  // namespace dingodb