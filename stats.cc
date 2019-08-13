// Copyright 2015 Google Inc. All rights reserved
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// +build ignore

#include "stats.h"

#include <algorithm>
#include <mutex>
#include <vector>

#include "flags.h"
#include "log.h"
#include "stringprintf.h"
#include "timeutil.h"

namespace {

mutex g_mu;
vector<Stats*>* g_stats;

}  // namespace

Stats::Stats(const char* name) : name_(name), elapsed_(0), cnt_(0) {
  unique_lock<mutex> lock(g_mu);
  if (g_stats == NULL)
    g_stats = new vector<Stats*>;
  g_stats->push_back(this);
}

void Stats::DumpTop() const {
  unique_lock<mutex> lock(mu_);
  if (detailed_.size() > 0) {
    vector<pair<string, double>> v(detailed_.begin(), detailed_.end());
    sort(
        v.begin(), v.end(),
        [](const pair<string, double> a, const pair<string, double> b) -> bool {
          return a.second > b.second;
        });
    for (unsigned int i = 0; i < 10 && i < v.size(); i++) {
      LOG_STAT(" %5.3f %s", v[i].first.c_str(), v[i].second);
    }
  }
}

string Stats::String() const {
  unique_lock<mutex> lock(mu_);
  return StringPrintf("%s: %f / %d", name_, elapsed_, cnt_);
}

double Stats::Start() {
  double start = GetTime();
  unique_lock<mutex> lock(mu_);
  cnt_++;
  return start;
}

double Stats::End(double start, const char* msg) {
  double e = GetTime() - start;
  unique_lock<mutex> lock(mu_);
  elapsed_ += e;
  if (msg != 0) {
    detailed_[string(msg)] += e;
  }
  return e;
}

ScopedStatsRecorder::ScopedStatsRecorder(Stats* st, const char* msg)
    : st_(st), msg_(msg), start_time_(0) {
  if (!g_flags.enable_stat_logs)
    return;
  start_time_ = st_->Start();
}

ScopedStatsRecorder::~ScopedStatsRecorder() {
  if (!g_flags.enable_stat_logs)
    return;
  double e = st_->End(start_time_, msg_);
  if (msg_ && e > 3.0) {
    LOG_STAT("slow %s (%f): %s", st_->name_, e, msg_);
  }
}

void ReportAllStats() {
  if (!g_stats)
    return;
  for (Stats* st : *g_stats) {
    LOG_STAT("%s", st->String().c_str());
    st->DumpTop();
  }
  delete g_stats;
}
