/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __FAKE_ISOLATION_MODULE_HPP__
#define __FAKE_ISOLATION_MODULE_HPP__

#include <glog/logging.h>

#include <process/timer.hpp>

#include "boost/scoped_ptr.hpp"

#include "common/type_utils.hpp"
#include "slave/isolation_module.hpp"
#include "mesos/mesos.hpp"
#include "common/resources.hpp"

#include "mesos/executor.hpp"

#include "fake/fake_task.hpp"

#include <pthread.h>

namespace mesos {
namespace internal {
namespace fake {

using mesos::internal::slave::IsolationModule;
using mesos::internal::slave::Slave;

class FakeIsolationModule;

class FakeExecutor : public Executor {
public:
  FakeExecutor(FakeIsolationModule* module_, const FakeTaskMap& fakeTasks_)
        : initialized(false), module(module_), fakeTasks(fakeTasks_) {}

  void init(ExecutorDriver* driver, const ExecutorArgs& args) {
    LOG(INFO) << "FakeExecutor: init; this=" << (void*)this;
    CHECK(!initialized);
    initialized = true;
    frameworkId.MergeFrom(args.framework_id());
    executorId.MergeFrom(args.executor_id());
  }

  void launchTask(ExecutorDriver* driver, const TaskDescription& task);

  void killTask(ExecutorDriver* driver, const TaskID& taskId);

  void frameworkMessage(ExecutorDriver* driver, const std::string& data) {
    LOG(FATAL) << "should not get framework message";
  }

  void shutdown(ExecutorDriver* driver) {
    CHECK(initialized);
    LOG(INFO) << "FakeExecutor: shutdown; this=" << (void*)this;
    initialized = false;
  }

  void error(ExecutorDriver* driver, int code, const std::string& message) {
    LOG(ERROR) << "FakeExecutor; code = " << code << "; message = " << message;
  }

  virtual ~FakeExecutor() {}
private:
  bool initialized;
  FrameworkID frameworkId;
  ExecutorID executorId;
  FakeIsolationModule* module;
  const FakeTaskMap& fakeTasks;
};

struct FakeIsolationModuleTicker : public process::Process<FakeIsolationModuleTicker> {
  FakeIsolationModuleTicker(FakeIsolationModule* module_, double interval_)
      : module(module_), interval(interval_) {}

  void initialize() {
    timer = process::delay(interval, self(), &FakeIsolationModuleTicker::tick);
  }

  void tick();

  FakeIsolationModule* module;
  double interval;
  process::timer timer;
};

class FakeIsolationModule : public IsolationModule {
public:
  FakeIsolationModule(const FakeTaskMap& fakeTasks_)
      : fakeTasks(fakeTasks_), shuttingDown(false) {
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&tasksLock, &mattr);
  }

  void initialize(const Configuration& conf, bool local,
                  const process::PID<Slave>& slave);

  void launchExecutor(const FrameworkID& frameworkId,
                      const FrameworkInfo& frameworkInfo,
                      const ExecutorInfo& executorInfo,
                      const std::string& directory,
                      const ResourceHints& resources);

  void killExecutor(const FrameworkID& frameworkId,
                    const ExecutorID& executorId);

  void resourcesChanged(const FrameworkID& frameworkId,
                        const ExecutorID& executorId,
                        const ResourceHints& resources);

  /* for calling elsewhere */
  void registerTask(const FrameworkID& frameworkId,
                    const ExecutorID& executorId,
                    const TaskID& taskId,
                    FakeTask* fakeTask);
  void unregisterTask(const FrameworkID& frameworkId,
                      const ExecutorID& executorId,
                      const TaskID& taskId);

  bool tick();

  virtual ~FakeIsolationModule();

private:
  friend class FakeExecutor;
  friend class FakeIsolationModuleTick;

  struct RunningTaskInfo {
    // possibly null if no task is running
    FakeTask* fakeTask;
    TaskID taskId;

    // includes both tasks and executor
    ResourceHints assignedResources;
  };

  typedef hashmap<std::pair<FrameworkID, ExecutorID>,
                  std::pair<MesosExecutorDriver*, FakeExecutor*> > DriverMap;
  DriverMap drivers;
  // For now, only one task per executor
  typedef hashmap<std::pair<FrameworkID, ExecutorID>, RunningTaskInfo> TaskMap;
  TaskMap tasks;
  pthread_mutex_t tasksLock;

  process::PID<Slave> slave;
  double interval;
  double lastTime;
  const FakeTaskMap& fakeTasks;
  boost::scoped_ptr<FakeIsolationModuleTicker> ticker;
  bool shuttingDown;
};

}  // namespace fake
}  // namespace internal
}  // namespace mesos

#endif
