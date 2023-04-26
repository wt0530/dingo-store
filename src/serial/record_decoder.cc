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

#include "record_decoder.h"

namespace dingodb {

RecordDecoder::RecordDecoder(int schema_version, std::vector<BaseSchema*>*  schemas,
                             long common_id) {
  this->schema_version_ = schema_version;
  this->schemas_ = schemas;
  this->common_id_ = common_id;
}
std::vector<std::any>* RecordDecoder::Decode(KeyValue*  key_value) {
  Buf* key_buf = new Buf(key_value->GetKey());
  Buf* value_buf = new Buf(key_value->GetValue());
  if (key_buf->ReadLong() != common_id_) {
    //"Wrong Common Id"
  }
  if (key_buf->ReverseReadInt() != codec_version_) {
    //"Wrong Codec Version"
  }
  if (value_buf->ReadInt() != schema_version_) {
    //"Wrong Schema Version"
  }
  std::vector<std::any>* record = new std::vector<std::any>(schemas_->size());
  for (BaseSchema *bs : *schemas_) {
    if (bs != nullptr) {
      BaseSchema::Type type = bs->GetType();
      switch (type) {
        case BaseSchema::kBool: {
          DingoSchema<std::optional<bool>>* bos = static_cast<DingoSchema<std::optional<bool>>*>(bs);
          if (bos->IsKey()) {
            record->at(bos->GetIndex()) = bos->DecodeKey(key_buf);
          } else {
            record->at(bos->GetIndex()) = bos->DecodeValue(value_buf);
          }
          break;
        }
        case BaseSchema::kInteger: {
          DingoSchema<std::optional<int32_t>>* is = static_cast<DingoSchema<std::optional<int32_t>>*>(bs);
          if (is->IsKey()) {
            record->at(is->GetIndex()) = is->DecodeKey(key_buf);
          } else {
            record->at(is->GetIndex()) = is->DecodeValue(value_buf);
          }
          break;
        }
        case BaseSchema::kLong: {
          DingoSchema<std::optional<int64_t>>* ls = static_cast<DingoSchema<std::optional<int64_t>>*>(bs);
          if (ls->IsKey()) {
            record->at(ls->GetIndex()) = ls->DecodeKey(key_buf);
          } else {
            record->at(ls->GetIndex()) = ls->DecodeValue(value_buf);
          }
          break;
        }
        case BaseSchema::kDouble: {
          DingoSchema<std::optional<double>>* ds = static_cast<DingoSchema<std::optional<double>>*>(bs);
          if (ds->IsKey()) {
            record->at(ds->GetIndex()) = ds->DecodeKey(key_buf);
          } else {
            record->at(ds->GetIndex()) = ds->DecodeValue(value_buf);
          }
          break;
        }
        case BaseSchema::kString: {
          DingoSchema<std::optional<std::reference_wrapper<std::string>>>* ss = static_cast<DingoSchema<std::optional<std::reference_wrapper<std::string>>>*>(bs);
          if (ss->IsKey()) {
            record->at(ss->GetIndex()) = ss->DecodeKey(key_buf);
          } else {
            record->at(ss->GetIndex()) = ss->DecodeValue(value_buf);
          }
          break;
        }
        default: {
          break;
        }
      }
    }
  }
  delete key_buf;
  delete value_buf;
  return record;
}
std::vector<std::any>* RecordDecoder::Decode(KeyValue*  key_value,
                                   std::vector<int>*  column_indexes) {
  Buf* key_buf = new Buf(key_value->GetKey());
  Buf* value_buf = new Buf(key_value->GetValue());
  if (key_buf->ReadLong() != common_id_) {
    //"Wrong Common Id"
  }
  if (key_buf->ReverseReadInt() != codec_version_) {
    //"Wrong Codec Version"
  }
  if (value_buf->ReadInt() != schema_version_) {
    //"Wrong Schema Version"
  }
  std::vector<std::any>* record = new std::vector<std::any>(schemas_->size());
  for (BaseSchema *bs : *schemas_) {
    if (bs != nullptr) {
      BaseSchema::Type type = bs->GetType();
      switch (type) {
        case BaseSchema::kBool: {
          DingoSchema<std::optional<bool>>* bos = static_cast<DingoSchema<std::optional<bool>>*>(bs);
          if (VectorFindAndRemove(column_indexes, bos->GetIndex())) {
            if (bos->IsKey()) {
              record->at(bos->GetIndex()) = bos->DecodeKey(key_buf);
            } else {
              record->at(bos->GetIndex()) = bos->DecodeValue(value_buf);
            }
          } else {
            record->at(bos->GetIndex()) = (std::optional<bool>) std::nullopt;
            if (bos->IsKey()) {
              bos->SkipKey(key_buf);
            } else {
              bos->SkipValue(value_buf);
            }
          }
          break;
        }
        case BaseSchema::kInteger: {
          DingoSchema<std::optional<int32_t>>* is = static_cast<DingoSchema<std::optional<int32_t>>*>(bs);
          if (VectorFindAndRemove(column_indexes, is->GetIndex())) {
            if (is->IsKey()) {
              record->at(is->GetIndex()) = is->DecodeKey(key_buf);
            } else {
              record->at(is->GetIndex()) = is->DecodeValue(value_buf);
            }
          } else {
            record->at(is->GetIndex()) = (std::optional<int32_t>) std::nullopt;
            if (is->IsKey()) {
              is->SkipKey(key_buf);
            } else {
              is->SkipValue(value_buf);
            }
          }
          break;
        }
        case BaseSchema::kLong: {
          DingoSchema<std::optional<int64_t>>* ls = static_cast<DingoSchema<std::optional<int64_t>>*>(bs);
          if (VectorFindAndRemove(column_indexes, ls->GetIndex())) {
            if (ls->IsKey()) {
              record->at(ls->GetIndex()) = ls->DecodeKey(key_buf);
            } else {
              record->at(ls->GetIndex()) = ls->DecodeValue(value_buf);
            }
          } else {
            record->at(ls->GetIndex()) = (std::optional<int64_t>) std::nullopt;
            if (ls->IsKey()) {
              ls->SkipKey(key_buf);
            } else {
              ls->SkipValue(value_buf);
            }
          }
          break;
        }
        case BaseSchema::kDouble: {
          DingoSchema<std::optional<double>>* ds = static_cast<DingoSchema<std::optional<double>>*>(bs);
          if (VectorFindAndRemove(column_indexes, ds->GetIndex())) {
            if (ds->IsKey()) {
              record->at(ds->GetIndex()) = ds->DecodeKey(key_buf);
            } else {
              record->at(ds->GetIndex()) = ds->DecodeValue(value_buf);
            }
          } else {
            record->at(ds->GetIndex()) = (std::optional<double>) std::nullopt;
            if (ds->IsKey()) {
              ds->SkipKey(key_buf);
            } else {
              ds->SkipValue(value_buf);
            }
          }
          break;
        }
        case BaseSchema::kString: {
          DingoSchema<std::optional<std::reference_wrapper<std::string>>>* ss = static_cast<DingoSchema<std::optional<std::reference_wrapper<std::string>>>*>(bs);
          if (VectorFindAndRemove(column_indexes, ss->GetIndex())) {
            if (ss->IsKey()) {
              record->at(ss->GetIndex()) = ss->DecodeKey(key_buf);
            } else {
              record->at(ss->GetIndex()) = ss->DecodeValue(value_buf);
            }
          } else {
            record->at(ss->GetIndex()) = (std::optional<std::reference_wrapper<std::string>>) std::nullopt;
            if (ss->IsKey()) {
              ss->SkipKey(key_buf);
            } else {
              ss->SkipValue(value_buf);
            }
          }
          break;
        }
        default: {
          break;
        }
      }
    }
  }
  delete key_buf;
  delete value_buf;
  return record;
}

}  // namespace dingodb