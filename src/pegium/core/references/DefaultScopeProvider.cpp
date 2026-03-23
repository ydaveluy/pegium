#include <pegium/core/references/DefaultScopeProvider.hpp>

#include <cassert>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>
#include <pegium/core/syntax-tree/AstUtils.hpp>
#include <pegium/core/workspace/AstDescriptions.hpp>

namespace pegium::references {
namespace {

using AstNodeDescription = workspace::AstNodeDescription;
using ScopeEntryBucket = workspace::ScopeEntryBucket;
using DescriptionVisitor =
    utils::function_ref<bool(const AstNodeDescription &)>;

[[nodiscard]] bool visit_entry_pointers(
    const std::vector<const AstNodeDescription *> &entries,
    DescriptionVisitor visitor) {
  for (const auto *entry : entries) {
    if (!visitor(*entry)) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool visit_named_entries(
    const workspace::NamedScopeEntries &entries, DescriptionVisitor visitor) {
  if (entries.empty()) {
    return true;
  }
  if (!visitor(*entries.first)) {
    return false;
  }
  return visit_entry_pointers(entries.duplicates, visitor);
}

[[nodiscard]] bool accepts_bucket(
    const services::SharedCoreServices &shared, std::type_index referenceType,
    const ScopeEntryBucket &bucket) {
  if (referenceType == std::type_index(typeid(void)) ||
      bucket.type == std::type_index(typeid(void))) {
    return false;
  }
  if (bucket.type == referenceType) {
    return true;
  }
  return shared.astReflection->isSubtype(bucket.type, referenceType);
}

[[nodiscard]] const AstNodeDescription *
find_scope_entry(const services::SharedCoreServices &shared,
                 const workspace::BucketedScopeEntries &entries,
                 std::type_index referenceType, std::string_view name) noexcept {
  for (const auto &bucket : entries.buckets) {
    if (!accepts_bucket(shared, referenceType, bucket)) {
      continue;
    }
    const auto it = bucket.entriesByName.find(name);
    if (it == bucket.entriesByName.end()) {
      continue;
    }
    assert(!it->second.empty());
    return it->second.first;
  }
  return nullptr;
}

[[nodiscard]] bool visit_entries(const services::SharedCoreServices &shared,
                                 const workspace::BucketedScopeEntries &entries,
                                 std::type_index referenceType,
                                 std::string_view name,
                                 DescriptionVisitor visitor) {
  for (const auto &bucket : entries.buckets) {
    if (!accepts_bucket(shared, referenceType, bucket)) {
      continue;
    }
    if (name.empty()) {
      if (!visit_entry_pointers(bucket.entries, visitor)) {
        return false;
      }
      continue;
    }

    const auto it = bucket.entriesByName.find(name);
    if (it == bucket.entriesByName.end()) {
      continue;
    }
    if (!visit_named_entries(it->second, visitor)) {
      return false;
    }
  }
  return true;
}

template <typename Levels, typename Visitor>
bool visit_local_scope_levels(const Levels &levels, const AstNode *container,
                              Visitor &&visitor) {
  for (auto *current = container; current != nullptr;
       current = current->getContainer()) {
    const auto it = levels.find(current);
    if (it == levels.end()) {
      continue;
    }
    if (!visitor(it->second)) {
      return false;
    }
  }
  return true;
}

} // namespace

DefaultScopeProvider::DefaultScopeProvider(
    const services::CoreServices &services)
    : services::DefaultCoreService(services), _localScopeCache(services.shared),
      _globalScopeCache(services.shared) {}

const workspace::AstNodeDescription *DefaultScopeProvider::getScopeEntry(
    const ReferenceInfo &context) const {
  if (context.referenceText.empty()) {
    return nullptr;
  }

  const auto referenceType = context.getReferenceType();
  const auto *container = context.container;
  if (container != nullptr) {
    const auto localScopes = getLocalScopeLevels(getDocument(*container));
    const AstNodeDescription *entry = nullptr;
    (void)visit_local_scope_levels(
        *localScopes, container,
        [this, referenceType, &context,
         &entry](const workspace::BucketedScopeEntries &entries) {
          entry = find_scope_entry(services.shared, entries, referenceType,
                                   context.referenceText);
          return entry == nullptr;
        });
    if (entry != nullptr) {
      return entry;
    }
  }

  const auto globalEntries = getGlobalEntries(referenceType);
  const auto globalIt = globalEntries->entriesByName.find(context.referenceText);
  if (globalIt == globalEntries->entriesByName.end()) {
    return nullptr;
  }
  assert(!globalIt->second.empty());
  return globalIt->second.first;
}

bool DefaultScopeProvider::visitScopeEntries(
    const ReferenceInfo &context,
    utils::function_ref<bool(const workspace::AstNodeDescription &)> visitor)
    const {
  const auto referenceType = context.getReferenceType();
  const auto *container = context.container;
  if (container != nullptr) {
    const auto localScopes = getLocalScopeLevels(getDocument(*container));
    if (!visit_local_scope_levels(
            *localScopes, container,
            [this, referenceType, &context,
             visitor](const workspace::BucketedScopeEntries &entries) {
              return visit_entries(services.shared, entries, referenceType,
                                   context.referenceText, visitor);
            })) {
      return false;
    }
  }

  const auto globalEntries = getGlobalEntries(referenceType);
  if (context.referenceText.empty()) {
    return visit_entry_pointers(globalEntries->allEntries, visitor);
  }

  const auto globalIt = globalEntries->entriesByName.find(context.referenceText);
  if (globalIt == globalEntries->entriesByName.end()) {
    return true;
  }
  return visit_named_entries(globalIt->second, visitor);
}

std::shared_ptr<const DefaultScopeProvider::LocalScopeLevels>
DefaultScopeProvider::getLocalScopeLevels(
    const workspace::Document &document) const {
  assert(document.id != workspace::InvalidDocumentId);
  return _localScopeCache.get(document.id, LocalScopeCacheKey, [&document]() {
    struct LocalScopeBuilder {
      workspace::BucketedScopeEntries entries;
      std::unordered_map<std::type_index, std::size_t> bucketIndexByType;
    };

    auto levels = std::make_shared<LocalScopeLevels>();
    if (document.localSymbols.empty()) {
      return levels;
    }

    std::unordered_map<const AstNode *, LocalScopeBuilder> builders;
    builders.reserve(document.localSymbols.size());

    for (const auto &[container, description] : document.localSymbols) {
      auto &builder = builders[container];
      const auto *entry = std::addressof(description);
      const auto [bucketIt, inserted] = builder.bucketIndexByType.try_emplace(
          entry->type, builder.entries.buckets.size());
      if (inserted) {
        auto &bucket = builder.entries.buckets.emplace_back();
        bucket.type = entry->type;
      }

      auto &bucket = builder.entries.buckets[bucketIt->second];
      bucket.entries.push_back(entry);
      bucket.entriesByName[entry->name].add(*entry);
    }

    levels->reserve(builders.size());
    for (auto &[container, builder] : builders) {
      levels->emplace(container, std::move(builder.entries));
    }
    return levels;
  });
}

std::shared_ptr<const DefaultScopeProvider::CompiledGlobalEntries>
DefaultScopeProvider::getGlobalEntries(std::type_index referenceType) const {
  return _globalScopeCache.get(referenceType, [this, referenceType] {
    auto compiled = std::make_shared<CompiledGlobalEntries>();
    if (referenceType == std::type_index(typeid(void))) {
      return std::shared_ptr<const CompiledGlobalEntries>(compiled);
    }

    compiled->elements =
        services.shared.workspace.indexManager->allElements(referenceType);
    compiled->allEntries.reserve(compiled->elements.size());
    compiled->entriesByName.reserve(compiled->elements.size());
    for (const auto &entry : compiled->elements) {
      const auto *description = std::addressof(entry);
      compiled->allEntries.push_back(description);
      compiled->entriesByName[description->name].add(*description);
    }
    return std::shared_ptr<const CompiledGlobalEntries>(compiled);
  });
}

} // namespace pegium::references
