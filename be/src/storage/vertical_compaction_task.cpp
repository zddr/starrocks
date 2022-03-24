// This file is licensed under the Elastic License 2.0. Copyright 2021-present, StarRocks Limited.

#include "storage/vertical_compaction_task.h"

#include <vector>

#include "column/schema.h"
#include "runtime/current_thread.h"
#include "storage/compaction_utils.h"
#include "storage/olap_common.h"
#include "storage/rowset/beta_rowset.h"
#include "storage/rowset/column_reader.h"
#include "storage/rowset/rowset.h"
#include "storage/rowset/rowset_writer.h"
#include "storage/storage_engine.h"
#include "storage/vectorized/chunk_helper.h"
#include "storage/vectorized/row_source_mask.h"
#include "storage/vectorized/tablet_reader.h"
#include "storage/vectorized/tablet_reader_params.h"
#include "util/time.h"
#include "util/trace.h"

namespace starrocks {

Status VerticalCompactionTask::run_impl() {
    Statistics statistics;
    RETURN_IF_ERROR(_vertical_compaction_data(&statistics));
    TRACE_COUNTER_INCREMENT("merged_rows", statistics.merged_rows);
    TRACE_COUNTER_INCREMENT("filtered_rows", statistics.filtered_rows);
    TRACE_COUNTER_INCREMENT("output_rows", statistics.output_rows);

    RETURN_IF_ERROR(_validate_compaction(statistics));
    TRACE("[Compaction] vertical compaction validated");

    _commit_compaction();
    TRACE("[Compaction] vertical compaction committed");

    return Status::OK();
}

Status VerticalCompactionTask::_vertical_compaction_data(Statistics* statistics) {
    TRACE("[Compaction] start vertical comapction data");
    int64_t max_rows_per_segment = CompactionUtils::get_segment_max_rows(
            config::max_segment_file_size, _task_info.input_rows_num, _task_info.input_rowsets_size);

    std::unique_ptr<RowsetWriter> output_rs_writer;
    RETURN_IF_ERROR(CompactionUtils::construct_output_rowset_writer(
            _tablet.get(), max_rows_per_segment, _task_info.algorithm, _task_info.output_version, &output_rs_writer));

    std::vector<std::vector<uint32_t>> column_groups;
    CompactionUtils::split_column_into_groups(_tablet->num_columns(), _tablet->num_key_columns(),
                                              config::vertical_compaction_max_columns_per_group, &column_groups);
    _task_info.column_group_size = column_groups.size();

    auto mask_buffer =
            std::make_unique<vectorized::RowSourceMaskBuffer>(_tablet->tablet_id(), _tablet->data_dir()->path());
    auto source_masks = std::make_unique<std::vector<vectorized::RowSourceMask>>();
    TRACE("[Compaction] compaction prepare finished, max_rows_per_segment:$0, column groups "
          "size:$1",
          max_rows_per_segment, column_groups.size());

    for (size_t i = 0; i < column_groups.size(); ++i) {
        if (should_stop()) {
            LOG(INFO) << "vertical compaction task_id:" << _task_info.task_id << " is stopped.";
            return Status::Cancelled("vertical compaction task is stopped.");
        }
        bool is_key = (i == 0);
        if (!is_key) {
            // read mask buffer from the beginning
            mask_buffer->flip_to_read();
        }
        RETURN_IF_ERROR(_compact_column_group(is_key, i, column_groups[i], output_rs_writer.get(), mask_buffer.get(),
                                              source_masks.get(), statistics));
    }
    TRACE("[Compaction] data compacted");

    RETURN_IF_ERROR(output_rs_writer->final_flush());

    TRACE("[Compaction] writer flushed");

    StatusOr<RowsetSharedPtr> build_res = output_rs_writer->build();
    if (!build_res.ok()) {
        LOG(WARNING) << "rowset writer build failed. compaction task_id:" << _task_info.task_id
                     << ", tablet:" << _task_info.tablet_id << " output_version=" << _task_info.output_version;
        return build_res.status();
    } else {
        _output_rowset = build_res.value();
    }
    _task_info.output_num_rows = _output_rowset->num_rows();
    _task_info.output_segments_num = _output_rowset->num_segments();
    _task_info.output_rowset_size = _output_rowset->data_disk_size();
    if (statistics) {
        _task_info.filtered_rows = statistics->filtered_rows;
        _task_info.merged_rows = statistics->merged_rows;
    }
    TRACE_COUNTER_INCREMENT("output_rowset_data_size", _output_rowset->data_disk_size());
    TRACE_COUNTER_INCREMENT("output_segments_num", _output_rowset->num_segments());
    TRACE("[Compaction] output rowset built");

    return Status::OK();
}

Status VerticalCompactionTask::_compact_column_group(bool is_key, int column_group_index,
                                                     const std::vector<uint32_t>& column_group,
                                                     RowsetWriter* output_rs_writer,
                                                     vectorized::RowSourceMaskBuffer* mask_buffer,
                                                     std::vector<vectorized::RowSourceMask>* source_masks,
                                                     Statistics* statistics) {
    vectorized::Schema schema =
            vectorized::ChunkHelper::convert_schema_to_format_v2(_tablet->tablet_schema(), column_group);
    vectorized::TabletReader reader(std::static_pointer_cast<Tablet>(_tablet->shared_from_this()),
                                    output_rs_writer->version(), schema, is_key, mask_buffer);
    vectorized::TabletReaderParams reader_params;
    DCHECK(compaction_type() == BASE_COMPACTION || compaction_type() == CUMULATIVE_COMPACTION);
    reader_params.reader_type =
            compaction_type() == BASE_COMPACTION ? READER_BASE_COMPACTION : READER_CUMULATIVE_COMPACTION;
    reader_params.profile = _runtime_profile.create_child("merge_rowsets");

    StatusOr<int32_t> ret = _calculate_chunk_size_for_column_group(column_group);
    if (!ret.ok()) {
        return ret.status();
    }
    int32_t chunk_size = ret.value();
    VLOG(1) << "compaction task_id:" << _task_info.task_id << ", tablet=" << _tablet->tablet_id()
            << ", column group=" << column_group_index << ", reader chunk size=" << chunk_size;
    reader_params.chunk_size = chunk_size;
    RETURN_IF_ERROR(reader.prepare());
    RETURN_IF_ERROR(reader.open(reader_params));

    StatusOr<size_t> rows_st = _compact_data(is_key, chunk_size, column_group, schema, &reader, output_rs_writer,
                                             mask_buffer, source_masks);
    if (!rows_st.ok()) {
        return rows_st.status();
    }

    if (is_key && statistics) {
        statistics->output_rows = rows_st.value();
        statistics->merged_rows = reader.merged_rows();
        statistics->filtered_rows = reader.stats().rows_del_filtered;
    }

    RETURN_IF_ERROR(output_rs_writer->flush_columns());

    if (is_key) {
        RETURN_IF_ERROR(mask_buffer->flush());
    }
    return Status::OK();
}

StatusOr<int32_t> VerticalCompactionTask::_calculate_chunk_size_for_column_group(
        const std::vector<uint32_t>& column_group) {
    int64_t total_num_rows = 0;
    int64_t total_mem_footprint = 0;
    for (auto& rowset : _input_rowsets) {
        if (rowset->rowset_meta()->rowset_type() != BETA_ROWSET) {
            LOG(WARNING) << "unsupported rowset type:" << rowset->rowset_meta()->rowset_type();
            return Status::InternalError("unsupported rowset type");
        }

        total_num_rows += rowset->num_rows();
        auto* beta_rowset = down_cast<BetaRowset*>(rowset.get());
        for (auto& segment : beta_rowset->segments()) {
            for (uint32_t column_index : column_group) {
                const auto* column_reader = segment->column(column_index);
                if (column_reader == nullptr) {
                    continue;
                }
                total_mem_footprint += column_reader->total_mem_footprint();
            }
        }
    }
    int32_t chunk_size =
            CompactionUtils::get_read_chunk_size(config::compaction_memory_limit_per_worker, config::vector_chunk_size,
                                                 total_num_rows, total_mem_footprint, _task_info.input_segments_num);
    return chunk_size;
}

StatusOr<size_t> VerticalCompactionTask::_compact_data(bool is_key, int32_t chunk_size,
                                                       const std::vector<uint32_t>& column_group,
                                                       const vectorized::Schema& schema,
                                                       vectorized::TabletReader* reader, RowsetWriter* output_rs_writer,
                                                       vectorized::RowSourceMaskBuffer* mask_buffer,
                                                       std::vector<vectorized::RowSourceMask>* source_masks) {
    DCHECK(reader);
    size_t output_rows = 0;
    auto chunk = vectorized::ChunkHelper::new_chunk(schema, chunk_size);
    auto char_field_indexes = vectorized::ChunkHelper::get_char_field_indexes(schema);

    Status status = Status::OK();
    size_t column_group_del_filtered_rows = 0;
    size_t column_group_merged_rows = 0;
    while (LIKELY(!should_stop())) {
#ifndef BE_TEST
        status = tls_thread_status.mem_tracker()->check_mem_limit("Compaction");
        if (!status.ok()) {
            LOG(WARNING) << "fail to execute compaction: " << status.message() << std::endl;
            return status;
        }
#endif

        chunk->reset();
        status = reader->get_next(chunk.get(), source_masks);
        if (!status.ok()) {
            if (status.is_end_of_file()) {
                break;
            } else {
                LOG(WARNING) << "reader get next error. tablet=" << _tablet->tablet_id()
                             << ", err=" << status.to_string();
                return Status::InternalError(fmt::format("reader get_next error: {}", status.to_string()));
            }
        }

        vectorized::ChunkHelper::padding_char_columns(char_field_indexes, schema, _tablet->tablet_schema(),
                                                      chunk.get());

        RETURN_IF_ERROR(output_rs_writer->add_columns(*chunk, column_group, is_key));

        _task_info.total_output_num_rows += chunk->num_rows();
        _task_info.total_del_filtered_rows += reader->stats().rows_del_filtered - column_group_del_filtered_rows;
        _task_info.total_merged_rows += reader->merged_rows() - column_group_merged_rows;
        column_group_del_filtered_rows = reader->stats().rows_del_filtered;
        column_group_merged_rows = reader->merged_rows();

        if (is_key) {
            output_rows += chunk->num_rows();
            if (!source_masks->empty()) {
                RETURN_IF_ERROR(mask_buffer->write(*source_masks));
            }
        }
        if (!source_masks->empty()) {
            source_masks->clear();
        }
    }
    if (should_stop()) {
        LOG(INFO) << "vertical compaction task_id:" << _task_info.task_id << ", tablet:" << _task_info.tablet_id
                  << " is stopped.";
        return Status::Cancelled("vertical compaction task is stopped.");
    }
    return output_rows;
}

} // namespace starrocks
