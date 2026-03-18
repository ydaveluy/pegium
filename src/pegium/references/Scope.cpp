#include <pegium/references/Scope.hpp>

#include <array>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

namespace pegium::references {
namespace {

using AstNodeDescription = workspace::AstNodeDescription;

inline char toLowerAscii(char c) noexcept {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

inline std::string normalizeName(std::string_view name) {
  std::string lowered;
  lowered.reserve(name.size());
  for (char c : name) {
    lowered.push_back(toLowerAscii(c));
  }
  return lowered;
}

class InsensitiveNameMatcher {
public:
  explicit InsensitiveNameMatcher(std::string_view needle) {
    _lowerNeedle.reserve(needle.size());
    for (char c : needle) {
      _lowerNeedle.push_back(toLowerAscii(c));
    }
  }

  [[nodiscard]] bool matches(std::string_view candidate) const noexcept {
    if (candidate.size() != _lowerNeedle.size()) {
      return false;
    }
    for (std::size_t i = 0; i < candidate.size(); ++i) {
      if (toLowerAscii(candidate[i]) != _lowerNeedle[i]) {
        return false;
      }
    }
    return true;
  }

private:
  std::string _lowerNeedle;
};

[[nodiscard]] utils::stream<const AstNodeDescription *>
makeEmptyDescriptionPointerStream() {
  return utils::make_stream<const AstNodeDescription *>(
      std::views::empty<const AstNodeDescription *>);
}

[[nodiscard]] utils::stream<const AstNodeDescription *>
makeDescriptionPointers(const std::vector<AstNodeDescription> &entries) {
  std::vector<const AstNodeDescription *> pointers;
  pointers.reserve(entries.size());
  for (const auto &entry : entries) {
    pointers.push_back(std::addressof(entry));
  }
  return utils::make_stream<const AstNodeDescription *>(std::move(pointers));
}

template <typename MultiMapType>
[[nodiscard]] utils::stream<const AstNodeDescription *>
makeMultiMapDescriptionPointers(const MultiMapType &map, std::string_view key) {
  const auto range = map.equal_range(key);
  std::vector<const AstNodeDescription *> entries;
  for (auto it = range.first; it != range.second; ++it) {
    entries.push_back(it->second);
  }
  return utils::make_stream<const AstNodeDescription *>(std::move(entries));
}

void appendSensitiveMatches(std::vector<const AstNodeDescription *> &matches,
                            const workspace::ScopeEntryBucket &bucket,
                            std::string_view name) {
  const auto [first, last] = bucket.entriesByName.equal_range(name);
  for (auto it = first; it != last; ++it) {
    if (it->second != nullptr) {
      matches.push_back(it->second);
    }
  }
}

[[nodiscard]] const AstNodeDescription *
findCaseInsensitiveMatch(const workspace::ScopeEntryBucket &bucket,
                         const InsensitiveNameMatcher &matcher) {
  for (const auto *entry : bucket.entries) {
    if (entry != nullptr && matcher.matches(entry->name)) {
      return entry;
    }
  }
  return nullptr;
}

} // namespace

//==============================================================================
// SharedBucketedTypeScope
//==============================================================================

SharedBucketedTypeScope::SharedBucketedTypeScope(
    std::shared_ptr<const workspace::BucketedScopeEntries> elements,
    std::shared_ptr<const BucketTypeFilter> filter,
    std::shared_ptr<const Scope> outerScope, ScopeOptions options)
    : _elements(std::move(elements)), _filter(std::move(filter)),
      _outerScope(std::move(outerScope)), _options(options) {}

bool SharedBucketedTypeScope::acceptsBucket(
    const workspace::ScopeEntryBucket &bucket) const noexcept {
  return _filter != nullptr && _filter->accepts(bucket);
}

const workspace::AstNodeDescription *
SharedBucketedTypeScope::getElement(std::string_view name) const noexcept {
  if (_elements != nullptr) {
    if (_options.caseInsensitive) {
      const InsensitiveNameMatcher matcher(name);
      for (const auto &bucket : _elements->buckets) {
        const auto *entry = findCaseInsensitiveMatch(bucket, matcher);
        if (entry == nullptr || !acceptsBucket(bucket)) {
          continue;
        }
        return entry;
      }
    } else {
      for (const auto &bucket : _elements->buckets) {
        const auto it = bucket.entriesByName.find(name);
        if (it == bucket.entriesByName.end() || !acceptsBucket(bucket)) {
          continue;
        }
        return it->second;
      }
    }
  }
  return _outerScope ? _outerScope->getElement(name) : nullptr;
}

utils::stream<const workspace::AstNodeDescription *>
SharedBucketedTypeScope::getElements(std::string_view name) const {
  std::vector<const AstNodeDescription *> localEntries;
  if (_elements != nullptr) {
    if (_options.caseInsensitive) {
      const InsensitiveNameMatcher matcher(name);
      for (const auto &bucket : _elements->buckets) {
        const auto *entry = findCaseInsensitiveMatch(bucket, matcher);
        if (entry == nullptr || !acceptsBucket(bucket)) {
          continue;
        }
        localEntries.push_back(entry);
        for (const auto *candidate : bucket.entries) {
          if (candidate != nullptr && candidate != entry &&
              matcher.matches(candidate->name)) {
            localEntries.push_back(candidate);
          }
        }
      }
    } else {
      for (const auto &bucket : _elements->buckets) {
        const auto it = bucket.entriesByName.find(name);
        if (it == bucket.entriesByName.end() || !acceptsBucket(bucket)) {
          continue;
        }
        appendSensitiveMatches(localEntries, bucket, name);
      }
    }
  }
  auto local = utils::make_stream<const AstNodeDescription *>(std::move(localEntries));
  if (_outerScope != nullptr && _options.concatOuterScope) {
    return utils::join<const AstNodeDescription *>(std::move(local),
                                                   _outerScope->getElements(name));
  }
  return local;
}

utils::stream<const workspace::AstNodeDescription *>
SharedBucketedTypeScope::getAllElements() const {
  std::vector<const AstNodeDescription *> localEntries;
  if (_elements != nullptr) {
    for (const auto &bucket : _elements->buckets) {
      if (!acceptsBucket(bucket)) {
        continue;
      }
      localEntries.insert(localEntries.end(), bucket.entries.begin(),
                          bucket.entries.end());
    }
  }
  auto local = utils::make_stream<const AstNodeDescription *>(std::move(localEntries));
  if (_outerScope == nullptr) {
    return local;
  }
  return utils::join<const AstNodeDescription *>(std::move(local),
                                                   _outerScope->getAllElements());
}

//==============================================================================
// CompositeBucketedTypeScope
//==============================================================================

CompositeBucketedTypeScope::CompositeBucketedTypeScope(
    std::vector<std::shared_ptr<const workspace::BucketedScopeEntries>> levels,
    std::shared_ptr<const BucketTypeFilter> filter,
    std::shared_ptr<const Scope> outerScope, ScopeOptions options)
    : _levels(std::move(levels)), _filter(std::move(filter)),
      _outerScope(std::move(outerScope)), _options(options) {}

bool CompositeBucketedTypeScope::acceptsBucket(
    const workspace::ScopeEntryBucket &bucket) const noexcept {
  return _filter != nullptr && _filter->accepts(bucket);
}

const workspace::AstNodeDescription *
CompositeBucketedTypeScope::getElement(std::string_view name) const noexcept {
  if (_options.caseInsensitive) {
    const InsensitiveNameMatcher matcher(name);
    for (const auto &level : _levels) {
      if (level == nullptr) {
        continue;
      }
      for (const auto &bucket : level->buckets) {
        const auto *entry = findCaseInsensitiveMatch(bucket, matcher);
        if (entry == nullptr || !acceptsBucket(bucket)) {
          continue;
        }
        return entry;
      }
    }
  } else {
    for (const auto &level : _levels) {
      if (level == nullptr) {
        continue;
      }
      for (const auto &bucket : level->buckets) {
        const auto it = bucket.entriesByName.find(name);
        if (it == bucket.entriesByName.end() || !acceptsBucket(bucket)) {
          continue;
        }
        return it->second;
      }
    }
  }
  return _outerScope ? _outerScope->getElement(name) : nullptr;
}

utils::stream<const workspace::AstNodeDescription *>
CompositeBucketedTypeScope::getElements(std::string_view name) const {
  std::vector<const AstNodeDescription *> localEntries;
  if (_options.caseInsensitive) {
    const InsensitiveNameMatcher matcher(name);
    for (const auto &level : _levels) {
      if (level == nullptr) {
        continue;
      }
      for (const auto &bucket : level->buckets) {
        const auto *entry = findCaseInsensitiveMatch(bucket, matcher);
        if (entry == nullptr || !acceptsBucket(bucket)) {
          continue;
        }
        localEntries.push_back(entry);
        for (const auto *candidate : bucket.entries) {
          if (candidate != nullptr && candidate != entry &&
              matcher.matches(candidate->name)) {
            localEntries.push_back(candidate);
          }
        }
      }
    }
  } else {
    for (const auto &level : _levels) {
      if (level == nullptr) {
        continue;
      }
      for (const auto &bucket : level->buckets) {
        const auto it = bucket.entriesByName.find(name);
        if (it == bucket.entriesByName.end() || !acceptsBucket(bucket)) {
          continue;
        }
        appendSensitiveMatches(localEntries, bucket, name);
      }
    }
  }
  auto local = utils::make_stream<const AstNodeDescription *>(std::move(localEntries));
  if (_outerScope != nullptr && _options.concatOuterScope) {
    return utils::join<const AstNodeDescription *>(std::move(local),
                                                   _outerScope->getElements(name));
  }
  return local;
}

utils::stream<const workspace::AstNodeDescription *>
CompositeBucketedTypeScope::getAllElements() const {
  std::vector<const AstNodeDescription *> localEntries;
  for (const auto &level : _levels) {
    if (level == nullptr) {
      continue;
    }
    for (const auto &bucket : level->buckets) {
      if (!acceptsBucket(bucket)) {
        continue;
      }
      localEntries.insert(localEntries.end(), bucket.entries.begin(),
                          bucket.entries.end());
    }
  }
  auto local = utils::make_stream<const AstNodeDescription *>(std::move(localEntries));
  if (_outerScope == nullptr) {
    return local;
  }
  return utils::join<const AstNodeDescription *>(std::move(local),
                                                   _outerScope->getAllElements());
}

//==============================================================================
// MapScope
//==============================================================================

MapScope::MapScope(std::vector<workspace::AstNodeDescription> elements,
                   std::shared_ptr<const Scope> outerScope,
                   ScopeOptions options)
    : _outerScope(std::move(outerScope)), _options(options) {
  _elements.reserve(elements.size());
  if (_options.caseInsensitive) {
    _caseInsensitiveElementsByName.reserve(elements.size());
  } else {
    _elementsByName.reserve(elements.size());
  }
  for (auto &element : elements) {
    _elements.emplace_back(std::move(element));
    const auto *entry = std::addressof(_elements.back());
    if (_options.caseInsensitive) {
      _caseInsensitiveElementsByName.insert_or_assign(
          normalizeName(entry->name), entry);
    } else {
      _elementsByName.insert_or_assign(entry->name, entry);
    }
  }
}

const workspace::AstNodeDescription *
MapScope::getElement(std::string_view name) const noexcept {
  if (_options.caseInsensitive) {
    const auto key = normalizeName(name);
    if (const auto it = _caseInsensitiveElementsByName.find(key);
        it != _caseInsensitiveElementsByName.end()) {
      return it->second;
    }
  } else if (const auto it = _elementsByName.find(name);
             it != _elementsByName.end()) {
    return it->second;
  }
  return _outerScope ? _outerScope->getElement(name) : nullptr;
}

utils::stream<const workspace::AstNodeDescription *>
MapScope::getElements(std::string_view name) const {
  const auto *entry = getElement(name);
  if (entry == nullptr) {
    return _outerScope != nullptr && _options.concatOuterScope
               ? _outerScope->getElements(name)
               : makeEmptyDescriptionPointerStream();
  }
  std::vector<const AstNodeDescription *> localEntries{entry};
  auto local = utils::make_stream<const AstNodeDescription *>(std::move(localEntries));
  if (_outerScope != nullptr && _options.concatOuterScope) {
    return utils::join<const AstNodeDescription *>(std::move(local),
                                                   _outerScope->getElements(name));
  }
  return local;
}

utils::stream<const workspace::AstNodeDescription *> MapScope::getAllElements() const {
  auto local = makeDescriptionPointers(_elements);
  if (_outerScope == nullptr) {
    return local;
  }
  return utils::join<const AstNodeDescription *>(std::move(local),
                                                   _outerScope->getAllElements());
}

//==============================================================================
// MultiMapScope
//==============================================================================

MultiMapScope::MultiMapScope(
    std::vector<workspace::AstNodeDescription> elements,
    std::shared_ptr<const Scope> outerScope, ScopeOptions options)
    : _outerScope(std::move(outerScope)), _options(options) {
  _elements.reserve(elements.size());
  if (_options.caseInsensitive) {
    _caseInsensitiveElementsByName.reserve(elements.size());
  } else {
    _elementsByName.reserve(elements.size());
  }
  for (auto &element : elements) {
    _elements.emplace_back(std::move(element));
    const auto *entry = std::addressof(_elements.back());
    if (_options.caseInsensitive) {
      _caseInsensitiveElementsByName.emplace(normalizeName(entry->name),
                                             entry);
    } else {
      _elementsByName.emplace(entry->name, entry);
    }
  }
}

const workspace::AstNodeDescription *
MultiMapScope::getElement(std::string_view name) const noexcept {
  if (_options.caseInsensitive) {
    const auto key = normalizeName(name);
    if (const auto it = _caseInsensitiveElementsByName.find(key);
        it != _caseInsensitiveElementsByName.end()) {
      return it->second;
    }
  } else if (const auto it = _elementsByName.find(name);
             it != _elementsByName.end()) {
    return it->second;
  }
  return _outerScope ? _outerScope->getElement(name) : nullptr;
}

utils::stream<const workspace::AstNodeDescription *>
MultiMapScope::getElements(std::string_view name) const {
  utils::stream<const AstNodeDescription *> local = _options.caseInsensitive
                                                ? makeMultiMapDescriptionPointers(
                                                      _caseInsensitiveElementsByName,
                                                      normalizeName(name))
                                                : makeMultiMapDescriptionPointers(
                                                      _elementsByName, name);
  if (_outerScope != nullptr && _options.concatOuterScope) {
    return utils::join<const AstNodeDescription *>(std::move(local),
                                                   _outerScope->getElements(name));
  }
  return local;
}

utils::stream<const workspace::AstNodeDescription *>
MultiMapScope::getAllElements() const {
  auto local = makeDescriptionPointers(_elements);
  if (_outerScope == nullptr) {
    return local;
  }
  return utils::join<const AstNodeDescription *>(std::move(local),
                                                   _outerScope->getAllElements());
}

//==============================================================================
// EmptyScope
//==============================================================================

const workspace::AstNodeDescription *
EmptyScope::getElement(std::string_view) const noexcept {
  return nullptr;
}

utils::stream<const workspace::AstNodeDescription *>
EmptyScope::getElements(std::string_view) const {
  return makeEmptyDescriptionPointerStream();
}

utils::stream<const workspace::AstNodeDescription *>
EmptyScope::getAllElements() const {
  return makeEmptyDescriptionPointerStream();
}

const EmptyScope EMPTY_SCOPE{};

} // namespace pegium::references
