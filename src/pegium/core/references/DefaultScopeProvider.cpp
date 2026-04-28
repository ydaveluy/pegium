#include <pegium/core/references/DefaultScopeProvider.hpp>

#include <algorithm>
#include <cassert>
#include <memory>
#include <typeindex>
#include <typeinfo>

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
  return std::ranges::all_of(entries, [&visitor](const auto *entry) {
    return visitor(*entry);
  });
}

[[nodiscard]] bool visit_owned_entries(
    const std::deque<AstNodeDescription> &entries,
    DescriptionVisitor visitor) {
  return std::ranges::all_of(
      entries, [&visitor](const auto &entry) { return visitor(entry); });
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
    const pegium::SharedCoreServices &shared, std::type_index referenceType,
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
find_scope_entry(const pegium::SharedCoreServices &shared,
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

[[nodiscard]] bool visit_entries(const pegium::SharedCoreServices &shared,
                                 const workspace::BucketedScopeEntries &entries,
                                 std::type_index referenceType,
                                 std::string_view name,
                                 DescriptionVisitor visitor) {
  for (const auto &bucket : entries.buckets) {
    if (!accepts_bucket(shared, referenceType, bucket)) {
      continue;
    }
    if (name.empty()) {
      if (!visit_owned_entries(bucket.ownedEntries, visitor)) {
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

template <typename Visitor>
bool visit_local_scope_levels(const workspace::LocalSymbols &localSymbols,
                              const AstNode *container, Visitor &&visitor) {
  for (auto *current = container; current != nullptr;
       current = current->getContainer()) {
    const auto *entries = localSymbols.forContainer(current);
    if (entries == nullptr) {
      continue;
    }
    if (!visitor(*entries)) {
      return false;
    }
  }
  return true;
}

} // namespace

DefaultScopeProvider::DefaultScopeProvider(
    const pegium::CoreServices &services)
    : pegium::DefaultCoreService(services), _globalScopeCache(services.shared) {}

const workspace::AstNodeDescription *DefaultScopeProvider::getScopeEntry(
    const ReferenceInfo &context) const {
  if (context.referenceText.empty()) {
    return nullptr;
  }

  const auto referenceType = context.getReferenceType();
  if (const auto *container = context.container; container != nullptr) {
    const auto &localSymbols = getDocument(*container).localSymbols;
    const AstNodeDescription *entry = nullptr;
    (void)visit_local_scope_levels(
        localSymbols, container,
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
  if (const auto *container = context.container; container != nullptr) {
    const auto &localSymbols = getDocument(*container).localSymbols;
    if (!visit_local_scope_levels(
            localSymbols, container,
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
