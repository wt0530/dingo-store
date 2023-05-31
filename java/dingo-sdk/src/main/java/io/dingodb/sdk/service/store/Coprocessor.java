/*
 * Copyright 2021 DataCanvas
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package io.dingodb.sdk.service.store;

import io.dingodb.sdk.common.serial.schema.DingoSchema;
import io.dingodb.sdk.common.utils.ByteArrayUtils;
import lombok.AllArgsConstructor;
import lombok.Builder;
import lombok.Getter;
import lombok.NoArgsConstructor;

import java.util.Collections;
import java.util.List;

@Getter
@Builder
public class Coprocessor {

    private static final SchemaWrapper DEFAULT_WRAPPER = SchemaWrapper.builder().build();

    private int schemaVersion;

    @Builder.Default
    private SchemaWrapper originalSchema = DEFAULT_WRAPPER;

    @Builder.Default
    private SchemaWrapper resultSchema = DEFAULT_WRAPPER;

    @Builder.Default
    private List<Integer> selection = Collections.emptyList();

    @Builder.Default
    private byte[] expression = ByteArrayUtils.EMPTY_BYTES;

    @Builder.Default
    private List<Integer> groupBy = Collections.emptyList();

    @Builder.Default
    private List<AggregationOperator> aggregations = Collections.emptyList();

    @Getter
    @Builder
    @NoArgsConstructor
    @AllArgsConstructor
    public static class SchemaWrapper {
        @Builder.Default
        private List<DingoSchema> schemas = Collections.emptyList();
        private long commonId;
    }
}