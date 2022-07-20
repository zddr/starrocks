// This file is made available under Elastic License 2.0.
// This file is based on code available under the Apache license here:
//   https://github.com/apache/incubator-doris/blob/master/be/src/util/arrow/row_batch.h

// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once

#include <memory>

#include "common/status.h"
#include "exprs/expr.h"

// This file will convert StarRocks RowBatch to/from Arrow's RecordBatch
// RowBatch is used by StarRocks query engine to exchange data between
// each execute node.

namespace arrow {

class RecordBatch;
class Schema;

} // namespace arrow

namespace starrocks {

class RowDescriptor;

// Convert StarRocks RowDescriptor to Arrow Schema.
Status convert_to_arrow_schema(const RowDescriptor& row_desc,
                               const std::unordered_map<int64_t, std::string>& id_to_col_name,
                               std::shared_ptr<arrow::Schema>* result,
                               const std::vector<ExprContext*>& output_expr_ctxs);

Status serialize_record_batch(const arrow::RecordBatch& record_batch, std::string* result);

} // namespace starrocks
