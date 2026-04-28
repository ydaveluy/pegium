#include <pegium/core/workspace/LocalSymbols.hpp>

#include <utility>

namespace pegium::workspace {

const AstNodeDescription &
LocalSymbols::emplace(const AstNode *container,
                      AstNodeDescription description) {
  auto &bucketed = _byContainer[container];
  const auto type = description.type;

  // Linear scan: containers typically hold ≤2-3 distinct symbol types, so an
  // unrolled equality check is faster than a side hash map.
  ScopeEntryBucket *bucket = nullptr;
  for (auto &candidate : bucketed.buckets) {
    if (candidate.type == type) {
      bucket = std::addressof(candidate);
      break;
    }
  }
  if (bucket == nullptr) {
    auto &fresh = bucketed.buckets.emplace_back();
    fresh.type = type;
    bucket = std::addressof(fresh);
  }

  // `std::deque::push_back` is element-stable, so the address captured below
  // stays valid for the entire lifetime of this `LocalSymbols`. The name
  // index is filled lazily by `forContainer` to avoid double-paying the
  // hash-map insertion cost during collection.
  auto &stored = bucket->ownedEntries.emplace_back(std::move(description));
  // Always invalidate, in case a late `emplace` follows a `forContainer`
  // call that already built the index.
  bucketed.indexed = false;
  ++_totalSize;
  return stored;
}

void LocalSymbols::buildNameIndex(const BucketedScopeEntries &bucketed) {
  for (const auto &bucket : bucketed.buckets) {
    // `clear` defends against a re-index after a late `emplace`; on the first
    // build the map is already empty and this is a no-op.
    bucket.entriesByName.clear();
    bucket.entriesByName.reserve(bucket.ownedEntries.size());
    for (const auto &entry : bucket.ownedEntries) {
      bucket.entriesByName[entry.name].add(entry);
    }
  }
  bucketed.indexed = true;
}

} // namespace pegium::workspace
