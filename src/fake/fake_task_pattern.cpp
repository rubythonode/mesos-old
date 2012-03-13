#include "fake/fake_task_pattern.hpp"

#include <glog/logging.h>

namespace mesos {
namespace internal {
namespace fake {

double Pattern::at(int index) const
{
  return counts[index % counts.size()];
}

double Pattern::countDuring(seconds start, seconds end) const
{
  double startOffset = start.value / duration.value;
  double endOffset = end.value / duration.value;

  double value = 0.0;

  int startIndex = (int) startOffset;
  int endIndex = (int) endOffset;

  if (startIndex == endIndex) {
    value += at(startIndex) * (endOffset - startOffset);
  } else {
    value += at(startIndex) * (startIndex + 1 - startOffset);
    value += at(endIndex) * (endOffset - endIndex);
    for (int i = startIndex + 1; i < endIndex; ++i) {
      value += at(i);
    }
  }
  return value;
}

PatternTask::PatternTask(
    const Resources& _constUsage, const ResourceHints& _request,
    const GenericPattern* _pattern, double _cpuPerUnit, seconds _baseTime)
  : constUsage(_constUsage),
    request(_request),
    pattern(_pattern),
    cpuPerUnit(_cpuPerUnit),
    violations(0.0),
    score(0.0),
    baseTime(_baseTime)
{
}

Resources PatternTask::getUsage(seconds from, seconds to) const
{
  Resource cpus;
  cpus.set_name("cpus");
  cpus.set_type(Value::SCALAR);
  double duration = to.value - from.value;
  cpus.mutable_scalar()->set_value(cpuPerUnit / duration *
      pattern->countDuring(from - baseTime, to - baseTime));
  return constUsage + cpus;
}

TaskState PatternTask::takeUsage(seconds from, seconds to,
                                 const Resources& resources)
{
  if (!(constUsage <= resources)) {
    violations += pattern->countDuring(from - baseTime, to - baseTime);
    return TASK_LOST;
  } else {
    double duration = to.value - from.value;
    double count = pattern->countDuring(from - baseTime, to - baseTime);
    double cpus =
      (resources - constUsage).get("cpus", Value::Scalar()).value();
    double missingCpu = count * cpuPerUnit / duration - cpus;
    LOG(INFO) << "missingCpu = " << missingCpu;
    score += count;
    if (missingCpu > 0) {
      double curViolations = missingCpu / cpuPerUnit * duration;
      score -= curViolations;
      violations += curViolations;
    }
    return TASK_RUNNING;
  }
}

void PatternTask::printToStream(std::ostream& out) const
{
  out << "PatternTask[constUsage: " << constUsage
      << "; request: " << request
      << "; cpuPerUnit: " << cpuPerUnit
      << "; violations: " << violations
      << "]";
}

}  // namespace fake
}  // namespace internal
}  // namespace mesos