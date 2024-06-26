// Copyright 2021-present StarRocks, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


package com.starrocks.connector.hive.glue.util;


import software.amazon.awssdk.services.glue.model.Partition;

import java.util.List;

public class PartitionKey {

    private final List<String> partitionValues;
    private final int hashCode;

    public PartitionKey(Partition partition) {
        this(partition.values());
    }

    public PartitionKey(List<String> partitionValues) {
        if (partitionValues == null) {
            throw new IllegalArgumentException("Partition values cannot be null");
        }
        this.partitionValues = partitionValues;
        this.hashCode = partitionValues.hashCode();
    }

    @Override
    public boolean equals(Object other) {
        return this == other || (other != null && other instanceof PartitionKey
                && this.partitionValues.equals(((PartitionKey) other).partitionValues));
    }

    @Override
    public int hashCode() {
        return hashCode;
    }

    List<String> getValues() {
        return partitionValues;
    }

}
