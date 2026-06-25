#include <pegium/core/workspace/LocalSymbols.hpp>

#include <utility>

namespace pegium::workspace {

const AstNodeDescription &
LocalSymbols::emplace(const AstNode *container,
                      AstNodeDescription description) {
  auto &buckets = _byContainer[container];
  const auto type = description.type;

  // Linear scan: containers typically hold ≤2-3 distinct symbol types, so an
  // unrolled equality check is faster than a side hash map.
  ScopeEntryBucket *bucket = nullptr;
  for (auto &candidate : buckets) {
    if (candidate.type == type) {
      bucket = std::addressof(candidate);
      break;
    }
  }
  if (bucket == nullptr) {
    auto &fresh = buckets.emplace_back();
    fresh.type = type;
    bucket = std::addressof(fresh);
  }

  // `std::deque::push_back` is element-stable, so the address pushed into the
  // name index stays valid for the entire lifetime of this `LocalSymbols`.
  const auto &stored = bucket->ownedEntries.emplace_back(std::move(description));
  bucket->entriesByName.try_emplace(stored.name).first->second.add(stored);
  ++_totalSize;
  return stored;
}

} // namespace pegium::workspace
