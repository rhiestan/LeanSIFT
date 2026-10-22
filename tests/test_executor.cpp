// LeanSift - fast SIFT feature extraction
// SPDX-License-Identifier: BSD-2-Clause

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

#include "doctest.h"
#include "lsift/executor.hpp"

namespace {

void ExpectCoversRange(lsift::Executor& ex, std::size_t n, std::size_t grain) {
  std::vector<std::atomic<int>> hits(n);
  ex.parallel_for(n, grain, [&](std::size_t b, std::size_t e) {
    CHECK(b < e);
    CHECK(e <= n);
    for (std::size_t i = b; i < e; ++i) hits[i].fetch_add(1);
  });
  for (std::size_t i = 0; i < n; ++i) {
    CAPTURE(i);
    REQUIRE(hits[i].load() == 1);
  }
}

TEST_CASE("ThreadPool.CoversEveryIndexOnce") {
  lsift::ThreadPool pool(8);
  for (std::size_t n : {0u, 1u, 2u, 7u, 100u, 1000u, 12345u}) {
    for (std::size_t grain : {1u, 3u, 64u, 5000u}) ExpectCoversRange(pool, n, grain);
  }
}

TEST_CASE("ThreadPool.ManyShortJobs") {
  lsift::ThreadPool pool(4);
  std::atomic<long long> sum{0};
  for (int job = 0; job < 2000; ++job) {
    pool.parallel_for(64, 1, [&](std::size_t b, std::size_t e) {
      for (std::size_t i = b; i < e; ++i) sum.fetch_add(static_cast<long long>(i));
    });
  }
  CHECK(sum.load() == 2000LL * (63 * 64 / 2));
}

TEST_CASE("ThreadPool.UsesSeveralThreads") {
  lsift::ThreadPool pool(4);
  if (pool.concurrency() < 2) {
    MESSAGE("skipped: fewer than two threads");
    return;
  }
  std::atomic<int> concurrent{0}, peak{0};
  pool.parallel_for(64, 1, [&](std::size_t, std::size_t) {
    const int c = ++concurrent;
    int p = peak.load();
    while (c > p && !peak.compare_exchange_weak(p, c)) {
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    --concurrent;
  });
  CHECK(peak.load() >= 2);
}

TEST_CASE("ThreadPool.PropagatesExceptions") {
  lsift::ThreadPool pool(4);
  CHECK_THROWS_AS(pool.parallel_for(1000, 1,
                                    [](std::size_t b, std::size_t) {
                                      if (b == 500) throw std::runtime_error("boom");
                                    }),
                  std::runtime_error);
  // The pool keeps working afterwards.
  ExpectCoversRange(pool, 1000, 7);
}

TEST_CASE("ThreadPool.NestedCallsRunSerially") {
  lsift::ThreadPool pool(4);
  std::atomic<int> total{0};
  pool.parallel_for(16, 1, [&](std::size_t, std::size_t) {
    pool.parallel_for(10, 1, [&](std::size_t b, std::size_t e) { total += static_cast<int>(e - b); });
  });
  CHECK(total.load() == 160);
}

TEST_CASE("ThreadPool.ConcurrentCallersDoNotDeadlock") {
  lsift::ThreadPool pool(4);
  std::vector<std::thread> callers;
  std::atomic<long long> total{0};
  for (int t = 0; t < 4; ++t) {
    callers.emplace_back([&] {
      for (int j = 0; j < 200; ++j) {
        pool.parallel_for(100, 3, [&](std::size_t b, std::size_t e) { total += static_cast<long long>(e - b); });
      }
    });
  }
  for (auto& c : callers) c.join();
  CHECK(total.load() == 4LL * 200 * 100);
}

TEST_CASE("SerialExecutor.SingleRange") {
  lsift::SerialExecutor ex;
  int calls = 0;
  ex.parallel_for(10, 1, [&](std::size_t b, std::size_t e) {
    ++calls;
    CHECK(b == 0u);
    CHECK(e == 10u);
  });
  CHECK(calls == 1);
}

}  // namespace
