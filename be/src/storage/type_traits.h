// This file is made available under Elastic License 2.0.
// This file is based on code available under the Apache license here:
//   https://github.com/apache/incubator-doris/blob/master/be/src/olap/types.h

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

#include <cstdint>

#include "runtime/decimalv2_value.h"
#include "storage/collection.h"
#include "storage/olap_common.h"
#include "util/slice.h"

namespace starrocks {

// CppTypeTraits:
// Infer on-disk type(CppType) from FieldType
template <FieldType field_type>
struct CppTypeTraits {};

template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_BOOL> {
    using CppType = bool;
    using UnsignedCppType = bool;
};

template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_NONE> {
    using CppType = bool;
    using UnsignedCppType = bool;
};

template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_TINYINT> {
    using CppType = int8_t;
    using UnsignedCppType = uint8_t;
};
template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_UNSIGNED_TINYINT> {
    using CppType = uint8_t;
    using UnsignedCppType = uint8_t;
};
template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_SMALLINT> {
    using CppType = int16_t;
    using UnsignedCppType = uint16_t;
};
template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_UNSIGNED_SMALLINT> {
    using CppType = uint16_t;
    using UnsignedCppType = uint16_t;
};
template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_INT> {
    using CppType = int32_t;
    using UnsignedCppType = uint32_t;
};
template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_UNSIGNED_INT> {
    using CppType = uint32_t;
    using UnsignedCppType = uint32_t;
};
template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_BIGINT> {
    using CppType = int64_t;
    using UnsignedCppType = uint64_t;
};
template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_UNSIGNED_BIGINT> {
    using CppType = uint64_t;
    using UnsignedCppType = uint64_t;
};
template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_LARGEINT> {
    using CppType = int128_t;
    using UnsignedCppType = uint128_t;
};
template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_FLOAT> {
    using CppType = float;
};
template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_DOUBLE> {
    using CppType = double;
};
template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_DECIMAL> {
    using CppType = decimal12_t;
    using UnsignedCppType = decimal12_t;
};
template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_DECIMAL_V2> {
    using CppType = DecimalV2Value;
    using UnsignedCppType = DecimalV2Value;
};

template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_DECIMAL32> {
    using CppType = int32_t;
    using UnsignedCppType = uint32_t;
};

template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_DECIMAL64> {
    using CppType = int64_t;
    using UnsignedCppType = uint64_t;
};

template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_DECIMAL128> {
    using CppType = int128_t;
    using UnsignedCppType = uint128_t;
};

template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_DATE> {
    using CppType = uint24_t;
    using UnsignedCppType = uint24_t;
};
template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_DATE_V2> {
    using CppType = int32_t;
    using UnsignedCppType = uint32_t;
};
template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_DATETIME> {
    using CppType = int64_t;
    using UnsignedCppType = uint64_t;
};
template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_TIMESTAMP> {
    using CppType = int64_t;
    using UnsignedCppType = uint64_t;
};
template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_IPV4> {
    using CppType = int64_t;
    using UnsignedCppType = uint64_t;
};
template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_CHAR> {
    using CppType = Slice;
};
template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_VARCHAR> {
    using CppType = Slice;
};
template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_HLL> {
    using CppType = Slice;
};
template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_OBJECT> {
    using CppType = Slice;
};

template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_PERCENTILE> {
    using CppType = Slice;
};

template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_JSON> {
    using CppType = Slice;
};

template <>
struct CppTypeTraits<OLAP_FIELD_TYPE_ARRAY> {
    using CppType = Collection;
};

// Instantiate this template to get static access to the type traits.
template <FieldType field_type>
struct TypeTraits {
    using CppType = typename CppTypeTraits<field_type>::CppType;

    static const FieldType type = field_type;
    static const int32_t size = sizeof(CppType);
};

} // namespace starrocks
