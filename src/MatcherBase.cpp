#include "MatcherBase.h"
#include "score_match.h"

#include <iostream>
#include <algorithm>
#include <queue>
#include <thread>

using namespace std;

typedef priority_queue<MatchResult> ResultHeap;

inline int letter_bitmask(const char *str) {
  int result = 0;
  for (int i = 0; str[i]; i++) {
    if (str[i] >= 'a' && str[i] <= 'z') {
      result |= (1 << (str[i] - 'a'));
    }
  }
  return result;
}

inline string str_to_lower(const char *s) {
  string lower(s);
  for (auto& c : lower) {
    if (c >= 'A' && c <= 'Z') {
      c += 'a' - 'A';
    }
  }
  return lower;
}

// Push a new entry on the heap while ensuring size <= max_results.
void push_heap(ResultHeap &heap,
               double score,
               const char *value,
               size_t max_results) {
  if (heap.size() < max_results || score > heap.top().score) {
    heap.emplace(score, value);
    if (heap.size() > max_results) {
      heap.pop();
    }
  }
}

vector<MatchResult> finalize(const string &query,
                             const MatchOptions &options,
                             bool record_match_indexes,
                             ResultHeap &&heap) {
  vector<MatchResult> vec;
  while (heap.size()) {
    const MatchResult &result = heap.top();
    if (record_match_indexes) {
      result.matchIndexes.reset(new vector<int>(query.size()));
      string lower = str_to_lower(result.value);
      score_match(
        result.value,
        lower.c_str(),
        query.c_str(),
        options,
        result.matchIndexes.get()
      );
    }
    vec.push_back(result);
    heap.pop();
  }
  reverse(vec.begin(), vec.end());
  return vec;
}

void thread_worker(
  const string &query,
  const MatchOptions &options,
  size_t max_results,
  const unordered_map<string, MatcherBase::CandidateData> &candidates,
  size_t start,
  size_t end,
  ResultHeap &result
) {
  int bitmask = letter_bitmask(query.c_str());
  for (size_t i = start; i < end; i++) {
    for (auto it = candidates.begin(i); it != candidates.end(i); ++it) {
      if ((bitmask & it->second.bitmask) == bitmask) {
        double score = score_match(
          it->first.c_str(),
          it->second.lowercase.c_str(),
          query.c_str(),
          options
        );
        if (score > 0) {
          push_heap(result, score, it->first.c_str(), max_results);
        }
      }
    }
  }
}

vector<MatchResult> MatcherBase::findMatches(const std::string &query,
                                             const MatcherOptions &options) {
  size_t max_results = options.max_results;
  size_t num_threads = options.num_threads;
  if (max_results == 0) {
    max_results = numeric_limits<size_t>::max();
  }
  MatchOptions matchOptions;
  matchOptions.case_sensitive = options.case_sensitive;

  string new_query;
  // Ignore all whitespace in the query.
  for (auto c : query) {
    if (!isspace(c)) {
      new_query += c;
    }
  }
  if (!options.case_sensitive) {
    new_query = str_to_lower(new_query.c_str());
  }

  if (num_threads == 0 || candidates_.size() < 10000) {
    ResultHeap result;
    thread_worker(new_query, matchOptions, max_results, candidates_, 0,
                  candidates_.bucket_count(), result);
    return finalize(
        new_query, matchOptions, options.record_match_indexes, move(result));
  }

  vector<ResultHeap> thread_results(num_threads);
  vector<thread> threads;
  size_t cur_start = 0;
  for (size_t i = 0; i < num_threads; i++) {
    size_t chunk_size = candidates_.bucket_count() / num_threads;
    // Distribute remainder among the chunks.
    if (i < candidates_.bucket_count() % num_threads) {
      chunk_size++;
    }
    threads.emplace_back(
      thread_worker,
      ref(new_query),
      ref(matchOptions),
      max_results,
      ref(candidates_),
      cur_start,
      cur_start + chunk_size,
      ref(thread_results[i])
    );
    cur_start += chunk_size;
  }

  ResultHeap combined;
  for (size_t i = 0; i < num_threads; i++) {
    threads[i].join();
    while (thread_results[i].size()) {
      auto &top = thread_results[i].top();
      push_heap(combined, top.score, top.value, max_results);
      thread_results[i].pop();
    }
  }
  return finalize(
      new_query, matchOptions, options.record_match_indexes, move(combined));
}

void MatcherBase::addCandidate(const string &candidate) {
  string lowercase = str_to_lower(candidate.c_str());
  int bitmask = letter_bitmask(lowercase.c_str());
  candidates_[candidate] = CandidateData {
    move(lowercase),
    bitmask
  };
}

void MatcherBase::removeCandidate(const string &candidate) {
  candidates_.erase(candidate);
}

void MatcherBase::clear() {
  candidates_.clear();
}

void MatcherBase::reserve(size_t n) {
  candidates_.reserve(n);
}

size_t MatcherBase::size() {
  return candidates_.size();
}
