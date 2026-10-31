#include "setpoint_generator.h"
#include "timer.h"
#include <stdlib.h>

SetpointGenerator::SetpointGenerator(int16_t initial_reference)
  : reference_(initial_reference), last_time_us_(time_mus()), remainder_(0) {}

int16_t SetpointGenerator::update(int16_t target,
  uint32_t acceleration_rpm_per_second,
  uint32_t deceleration_rpm_per_second)
{
  uint32_t now = time_mus();
  uint32_t elapsed_us = now - last_time_us_;
  last_time_us_ = now;

  if (target == reference_) {
    remainder_ = 0;
    return reference_;
  }
  bool reversing = (reference_ > 0 && target < 0) || (reference_ < 0 && target > 0);
  int16_t goal = reversing ? 0 : target;

  bool accelerating = !reversing && (abs(target) > abs(reference_));
  uint32_t limit = accelerating ? acceleration_rpm_per_second : deceleration_rpm_per_second;

  if (limit == 0) {
    reference_ = target;
    return reference_;
  }

  // limit [rpm/s] * t [µs] / 1e6 * 100 = limit * t / 10000  (í 1/100 rpm)
  uint64_t scaled = (uint64_t)limit * elapsed_us + remainder_;
  int32_t max_step = scaled / 10000;
  remainder_ = scaled % 10000; // brotið geymt til næsta kalls
  int32_t diff = goal - reference_;

  if (abs(diff) <= max_step) {
    reference_ = goal;
  } else if (diff > 0) {
    reference_ += max_step;
  } else {
    reference_ -= max_step;
  }

  return reference_;
}

int16_t SetpointGenerator::reference() const
{
  return reference_;
}

void SetpointGenerator::reset(int16_t reference)
{
  reference_ = reference;
  last_time_us_ = time_mus();
  remainder_ = 0;
}