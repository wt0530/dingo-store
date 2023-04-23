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
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "braft/raft.h"
#include "braft/util.h"
#include "brpc/channel.h"
#include "brpc/controller.h"
#include "bthread/bthread.h"
#include "gflags/gflags.h"
#include "google/protobuf/util/json_util.h"
#include "proto/coordinator.pb.h"
#include "proto/meta.pb.h"
#include "proto/node.pb.h"

std::string MessageToJsonString(const google::protobuf::Message& message);

// common functions
std::string GetLeaderLocation(uint32_t service_type);

// coordinator service functions
void SendGetNodeInfo(brpc::Controller& cntl, dingodb::pb::node::NodeService_Stub& stub);
void SendGetLogLevel(brpc::Controller& cntl, dingodb::pb::node::NodeService_Stub& stub);
void SendChangeLogLevel(brpc::Controller& cntl, dingodb::pb::node::NodeService_Stub& stub);
void SendHello(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendGetStoreMap(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendGetExecutorMap(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendGetCoordinatorMap(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendGetRegionMap(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendCreateStore(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendDeleteStore(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendStoreHearbeat(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub,
                       uint64_t store_id);
void SendGetStoreMetrics(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendCreateExecutor(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendDeleteExecutor(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendCreateExecutorUser(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendUpdateExecutorUser(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendDeleteExecutorUser(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendGetExecutorUserMap(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendExecutorHeartbeat(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);

// region
void SendQueryRegion(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendCreateRegionForSplit(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendDropRegion(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendDropRegionPermanently(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendSplitRegion(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendMergeRegion(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendAddPeerRegion(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendRemovePeerRegion(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendGetOrphanRegion(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);

// store operation
void SendGetStoreOperation(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendCleanStoreOperation(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendAddStoreOperation(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendRemoveStoreOperation(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);

// meta service functions
void SendGetSchemas(brpc::Controller& cntl, dingodb::pb::meta::MetaService_Stub& stub);
void SendGetSchema(brpc::Controller& cntl, dingodb::pb::meta::MetaService_Stub& stub);
void SendGetSchemaByName(brpc::Controller& cntl, dingodb::pb::meta::MetaService_Stub& stub);
void SendGetTablesCount(brpc::Controller& cntl, dingodb::pb::meta::MetaService_Stub& stub);
void SendGetTables(brpc::Controller& cntl, dingodb::pb::meta::MetaService_Stub& stub);
void SendGetTable(brpc::Controller& cntl, dingodb::pb::meta::MetaService_Stub& stub);
void SendGetTableByName(brpc::Controller& cntl, dingodb::pb::meta::MetaService_Stub& stub);
void SendGetTableRange(brpc::Controller& cntl, dingodb::pb::meta::MetaService_Stub& stub);
void SendCreateTableId(brpc::Controller& cntl, dingodb::pb::meta::MetaService_Stub& stub);
void SendCreateTable(brpc::Controller& cntl, dingodb::pb::meta::MetaService_Stub& stub, bool with_table_id);
void SendDropTable(brpc::Controller& cntl, dingodb::pb::meta::MetaService_Stub& stub);
void SendDropSchema(brpc::Controller& cntl, dingodb::pb::meta::MetaService_Stub& stub);
void SendCreateSchema(brpc::Controller& cntl, dingodb::pb::meta::MetaService_Stub& stub);
void SendGetTableMetrics(brpc::Controller& cntl, dingodb::pb::meta::MetaService_Stub& stub);

// raft_control functions
void SendRaftAddPeer(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);
void SendRaftRemovePeer(brpc::Controller& cntl, dingodb::pb::coordinator::CoordinatorService_Stub& stub);

// auto increment functions
void SendGetAutoIncrement(brpc::Controller& cntl, dingodb::pb::meta::MetaService_Stub& stub);
void SendCreateAutoIncrement(brpc::Controller& cntl, dingodb::pb::meta::MetaService_Stub& stub);
void SendUpdateAutoIncrement(brpc::Controller& cntl, dingodb::pb::meta::MetaService_Stub& stub);
void SendGenerateAutoIncrement(brpc::Controller& cntl, dingodb::pb::meta::MetaService_Stub& stub);
void SendDeleteAutoIncrement(brpc::Controller& cntl, dingodb::pb::meta::MetaService_Stub& stub);
