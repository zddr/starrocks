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

package com.starrocks.scheduler.history;

import com.starrocks.common.Config;
import com.starrocks.load.pipe.filelist.RepoExecutor;
import com.starrocks.scheduler.Constants;
import com.starrocks.scheduler.persist.TaskRunStatus;
import com.starrocks.statistic.StatsConstants;
import com.starrocks.system.SystemInfoService;
import com.starrocks.thrift.TGetTasksParams;
import com.starrocks.utframe.UtFrameUtils;
import mockit.Expectations;
import mockit.Mock;
import mockit.MockUp;
import mockit.Mocked;
import org.junit.Assert;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.Test;

import java.util.Set;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertTrue;

public class TaskRunHistoryTest {

    @BeforeAll
    public static void beforeAll() {
        UtFrameUtils.createMinStarRocksCluster();
    }

    @Test
    public void testTaskRunStatusSerialization() {
        TaskRunStatus status = new TaskRunStatus();
        String json = status.toJSON();
        assertEquals("{\"taskId\":0,\"createTime\":0,\"expireTime\":0,\"priority\":0,\"mergeRedundant\":false," +
                "\"source\":\"CTAS\",\"errorCode\":0,\"finishTime\":0,\"processStartTime\":0," +
                "\"state\":\"PENDING\",\"progress\":0,\"mvExtraMessage\":{\"forceRefresh\":false," +
                "\"mvPartitionsToRefresh\":[],\"refBasePartitionsToRefreshMap\":{}," +
                "\"basePartitionsToRefreshMap\":{},\"processStartTime\":0,\"executeOption\":{\"priority\":0," +
                "\"isMergeRedundant\":true,\"isManual\":false,\"isSync\":false,\"isReplay\":false}}}", json);

        TaskRunStatus b = TaskRunStatus.fromJson(json);
        assertEquals(status.toJSON(), b.toJSON());
    }

    @Test
    public void testCRUD(@Mocked RepoExecutor repo) {
        new MockUp<TableKeeper>() {
            @Mock
            public boolean isReady() {
                return true;
            }
        };
        new Expectations() {
            {
                repo.executeDML("INSERT INTO _statistics_.task_run_history (task_id, task_run_id, task_name, " +
                        "task_state, create_time, finish_time, expire_time, history_content_json) " +
                        "VALUES(0, 'aaa', 't1', 'SUCCESS', '1970-01-01 08:00:00', " +
                        "'1970-01-01 08:00:00', '1970-01-01 08:00:00', " +
                        "'{\"startTaskRunId\":\"aaa\",\"taskId\":0,\"taskName\":\"t1\",\"createTime\":0," +
                        "\"expireTime\":0,\"priority\":0,\"mergeRedundant\":false,\"source\":\"CTAS\"," +
                        "\"errorCode\":0,\"finishTime\":0,\"processStartTime\":0,\"state\":\"SUCCESS\"," +
                        "\"progress\":0,\"mvExtraMessage\":{\"forceRefresh\":false,\"mvPartitionsToRefresh\":[]," +
                        "\"refBasePartitionsToRefreshMap\":{},\"basePartitionsToRefreshMap\":{}," +
                        "\"processStartTime\":0,\"executeOption\":{\"priority\":0,\"isMergeRedundant\":true," +
                        "\"isManual\":false,\"isSync\":false,\"isReplay\":false}}}')");
            }
        };

        TaskRunHistoryTable history = new TaskRunHistoryTable();
        TaskRunStatus status = new TaskRunStatus();
        status.setStartTaskRunId("aaa");
        status.setTaskName("t1");
        status.setState(Constants.TaskRunState.SUCCESS);
        history.addHistory(status);

        // lookup by params
        TGetTasksParams params = new TGetTasksParams();
        new Expectations() {
            {
                repo.executeDQL("SELECT history_content_json FROM _statistics_.task_run_history WHERE TRUE AND  " +
                        "get_json_string(history_content_json, 'dbName') = 'default_cluster:d1'");
            }
        };
        params.setDb("d1");
        history.lookup(params);

        new Expectations() {
            {
                repo.executeDQL("SELECT history_content_json FROM _statistics_.task_run_history WHERE TRUE AND  " +
                        "task_state = 'SUCCESS'");
            }
        };
        params.setDb(null);
        params.setState("SUCCESS");
        history.lookup(params);

        new Expectations() {
            {
                repo.executeDQL("SELECT history_content_json FROM _statistics_.task_run_history WHERE TRUE AND  " +
                        "task_name = 't1'");
            }
        };
        params.setDb(null);
        params.setState(null);
        params.setTask_name("t1");
        history.lookup(params);

        new Expectations() {
            {
                repo.executeDQL("SELECT history_content_json FROM _statistics_.task_run_history WHERE TRUE AND  " +
                        "task_run_id = 'q1'");
            }
        };
        params.setDb(null);
        params.setState(null);
        params.setTask_name(null);
        params.setQuery_id("q1");
        history.lookup(params);

        // lookup by task names
        String dbName = "";
        Set<String> taskNames = Set.of("t1", "t2");
        new Expectations() {
            {
                repo.executeDQL("SELECT history_content_json FROM _statistics_.task_run_history WHERE TRUE AND  " +
                        "task_name IN ('t1','t2')");
            }
        };
        history.lookupByTaskNames(dbName, taskNames);

    }

    @Test
    public void testKeeper(@Mocked RepoExecutor repo) {
        TableKeeper keeper = TaskRunHistoryTable.createKeeper();
        assertEquals(StatsConstants.STATISTICS_DB_NAME, keeper.getDatabaseName());
        assertEquals(TaskRunHistoryTable.TABLE_NAME, keeper.getTableName());
        assertEquals(TaskRunHistoryTable.CREATE_TABLE, keeper.getCreateTableSql());
        assertEquals(TaskRunHistoryTable.TABLE_REPLICAS, keeper.getTableReplicas());

        // database not exists
        new Expectations() {
            {
                keeper.checkDatabaseExists();
                result = false;
            }
        };
        keeper.run();
        assertFalse(keeper.isDatabaseExisted());

        // create table
        keeper.setDatabaseExisted(true);
        new Expectations() {
            {
                repo.executeDDL("CREATE TABLE IF NOT EXISTS _statistics_.task_run_history (task_id bigint NOT NULL, " +
                        "task_run_id string NOT NULL, create_time datetime NOT NULL, " +
                        "task_name string NOT NULL, task_state STRING NOT NULL, finish_time datetime NOT NULL, " +
                        "expire_time datetime NOT NULL, history_content_json JSON NOT NULL)PRIMARY KEY " +
                        "(task_id, task_run_id, create_time) PARTITION BY date_trunc('DAY', create_time) " +
                        "DISTRIBUTED BY HASH(task_id) BUCKETS 8 PROPERTIES( 'replication_num' = '1', " +
                        "'partition_live_number' = '7')");
            }
        };
        keeper.run();
        assertTrue(keeper.isTableExisted());
        assertFalse(keeper.isTableCorrected());

        new MockUp<SystemInfoService>() {
            @Mock
            public int getTotalBackendNumber() {
                return 3;
            }
        };
        // correct table replicas
        keeper.run();
        assertTrue(keeper.isTableCorrected());
    }

    @Test
    public void testHistoryVacuum(@Mocked RepoExecutor repo) {
        new MockUp<TableKeeper>() {
            @Mock
            public boolean isReady() {
                return true;
            }
        };

        TaskRunHistory history = new TaskRunHistory();

        // prepare
        long currentTime = 1720165070904L;
        TaskRunStatus run1 = new TaskRunStatus();
        run1.setExpireTime(currentTime - 1);
        run1.setQueryId("q1");
        run1.setTaskName("t1");
        run1.setState(Constants.TaskRunState.RUNNING);
        history.addHistory(run1);
        TaskRunStatus run2 = new TaskRunStatus();
        run2.setExpireTime(currentTime + 10000);
        run2.setQueryId("q2");
        run2.setTaskName("t2");
        run2.setState(Constants.TaskRunState.SUCCESS);
        history.addHistory(run2);
        assertEquals(2, history.getInMemoryHistory().size());

        // run the normal vacuum
        new Expectations() {
            {
                repo.executeDML(
                        "INSERT INTO _statistics_.task_run_history (task_id, task_run_id, task_name, task_state, " +
                                "create_time, finish_time, expire_time, history_content_json) VALUES(0, 'null', 't2'," +
                                " 'SUCCESS', '1970-01-01 08:00:00', '1970-01-01 08:00:00', '2024-07-05 15:38:00', " +
                                "'{\"queryId\":\"q2\",\"taskId\":0,\"taskName\":\"t2\",\"createTime\":0," +
                                "\"expireTime\":1720165080904,\"priority\":0,\"mergeRedundant\":false," +
                                "\"source\":\"CTAS\",\"errorCode\":0,\"finishTime\":0,\"processStartTime\":0," +
                                "\"state\":\"SUCCESS\",\"progress\":0,\"mvExtraMessage\":{\"forceRefresh\":false," +
                                "\"mvPartitionsToRefresh\":[],\"refBasePartitionsToRefreshMap\":{}," +
                                "\"basePartitionsToRefreshMap\":{},\"processStartTime\":0," +
                                "\"executeOption\":{\"priority\":0,\"isMergeRedundant\":true,\"isManual\":false," +
                                "\"isSync\":false,\"isReplay\":false}}}')");

            }
        };
        history.vacuum();
        assertEquals(1, history.getInMemoryHistory().size());

        // vacuum failed
        new Expectations() {
            {
                repo.executeDML(anyString);
                result = new RuntimeException("insert failed");
            }
        };
        TaskRunStatus run3 = new TaskRunStatus();
        run3.setExpireTime(System.currentTimeMillis() + 10000);
        run3.setQueryId("q3");
        run3.setTaskName("t3");
        run3.setState(Constants.TaskRunState.SUCCESS);
        history.addHistory(run3);
        history.vacuum();
        assertEquals(2, history.getInMemoryHistory().size());
    }

    @Test
    public void testDisableArchiveHistory() {
        Config.enable_task_history_archive = false;
        TaskRunHistory history = new TaskRunHistory();

        // prepare
        long currentTime = 1720165070904L;
        TaskRunStatus run1 = new TaskRunStatus();
        run1.setExpireTime(currentTime - 1);
        run1.setQueryId("q1");
        run1.setTaskName("t1");
        run1.setState(Constants.TaskRunState.RUNNING);
        history.addHistory(run1);
        TaskRunStatus run2 = new TaskRunStatus();
        run2.setExpireTime(currentTime + 10000);
        run2.setQueryId("q2");
        run2.setTaskName("t2");
        run2.setState(Constants.TaskRunState.SUCCESS);
        history.addHistory(run2);
        assertEquals(2, history.getInMemoryHistory().size());

        // run, trigger the expiration
        history.vacuum();
        Assert.assertEquals(0, history.getInMemoryHistory().size());

        Config.enable_task_history_archive = true;
    }
}