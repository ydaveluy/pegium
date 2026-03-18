#include <pegium/references/DefaultScopeProvider.hpp>

#include <memory>
#include <ranges>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

#include <pegium/services/CoreServices.hpp>
#include <pegium/services/SharedCoreServices.hpp>
#include <pegium/syntax-tree/AstUtils.hpp>

namespace pegium::references {

namespace {

std::type_index invalid_type() noexcept {
  return std::type_index(typeid(void));
}

} // namespace

class DefaultScopeProvider::ReferenceTypeFilter final : public BucketTypeFilter {
public:
  ReferenceTypeFilter(const DefaultScopeProvider &provider,
                      ScopeQueryContext context) noexcept
      : _provider(provider), _context(std::move(context)) {}

  [[nodiscard]] bool
  accepts(const workspace::ScopeEntryBucket &bucket) const override {
    return _provider.acceptsBucket(_context, bucket);
  }

private:
  const DefaultScopeProvider &_provider;
  ScopeQueryContext _context;
};

DefaultScopeProvider::DefaultScopeProvider(
    const services::CoreServices &services)
    : services::DefaultCoreService(services), _localScopeCache(services.shared) {}

std::shared_ptr<const Scope>
DefaultScopeProvider::getScope(const ScopeQueryContext &context) const {
  auto filter = std::make_shared<ReferenceTypeFilter>(*this, context);
  auto globalScope = getGlobalScope(filter);

  const auto *container = context.container;
  const auto *document = container ? tryGetDocument(*container) : nullptr;
  if (document == nullptr || container == nullptr) {
    return globalScope;
  }

  const auto localScopes = getLocalScopeLevels(*document);
  if (localScopes == nullptr || localScopes->empty()) {
    return globalScope;
  }

  std::vector<std::shared_ptr<const workspace::BucketedScopeEntries>> localLevels;
  localLevels.reserve(8);
  for (auto *current = container; current != nullptr;
       current = current->getContainer()) {
    if (const auto it = localScopes->find(current); it != localScopes->end()) {
      localLevels.push_back(it->second);
    }
  }

  if (localLevels.empty()) {
    return globalScope;
  }
  return std::make_shared<CompositeBucketedTypeScope>(std::move(localLevels),
                                                      std::move(filter),
                                                      std::move(globalScope));
}

const workspace::AstNodeDescription *DefaultScopeProvider::getScopeEntry(
    const ScopeQueryContext &context) const noexcept {
  if (context.referenceText.empty()) {
    return nullptr;
  }

  const auto *container = context.container;
  const auto *document = container ? tryGetDocument(*container) : nullptr;
  if (document != nullptr && container != nullptr) {
    if (const auto localScopes = getLocalScopeLevels(*document);
        localScopes != nullptr && !localScopes->empty()) {
      for (auto *current = container; current != nullptr;
           current = current->getContainer()) {
        const auto it = localScopes->find(current);
        if (it == localScopes->end() || it->second == nullptr) {
          continue;
        }
        if (const auto *entry =
                findScopeEntry(*it->second, context, context.referenceText);
            entry != nullptr) {
          return entry;
        }
      }
    }
  }

  const auto *indexManager = coreServices.shared.workspace.indexManager.get();
  if (indexManager == nullptr) {
    return nullptr;
  }
  if (const auto globalEntries = indexManager->allBucketedScopeEntries();
      globalEntries == nullptr) {
    return nullptr;
  } else {
    return findScopeEntry(*globalEntries, context, context.referenceText);
  }
}

utils::stream<const workspace::AstNodeDescription *>
DefaultScopeProvider::getScopeEntries(const ScopeQueryContext &context) const {
  std::vector<const workspace::AstNodeDescription *> matches;
  const auto *container = context.container;
  const auto *document = container ? tryGetDocument(*container) : nullptr;
  if (document != nullptr && container != nullptr) {
    if (const auto localScopes = getLocalScopeLevels(*document);
        localScopes != nullptr && !localScopes->empty()) {
      for (auto *current = container; current != nullptr;
           current = current->getContainer()) {
        const auto it = localScopes->find(current);
        if (it == localScopes->end() || it->second == nullptr) {
          continue;
        }
        appendScopeEntries(matches, *it->second, context, context.referenceText);
      }
    }
  }

  const auto *indexManager = coreServices.shared.workspace.indexManager.get();
  if (indexManager != nullptr) {
    if (const auto globalEntries = indexManager->allBucketedScopeEntries();
        globalEntries != nullptr) {
      appendScopeEntries(matches, *globalEntries, context, context.referenceText);
    }
  }
  return utils::make_stream<const workspace::AstNodeDescription *>(
      std::move(matches));
}

std::shared_ptr<const DefaultScopeProvider::LocalScopeLevels>
DefaultScopeProvider::getLocalScopeLevels(
    const workspace::Document &document) const {
  auto buildLevels = [&document]() {
    struct LocalScopeBuilder {
      std::shared_ptr<workspace::BucketedScopeEntries> entries;
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
      if (builder.entries == nullptr) {
        builder.entries = std::make_shared<workspace::BucketedScopeEntries>();
      }

      const auto *entry = std::addressof(description);
      const auto [bucketIt, inserted] = builder.bucketIndexByType.try_emplace(
          entry->type, builder.entries->buckets.size());
      if (inserted) {
        auto &bucket = builder.entries->buckets.emplace_back();
        bucket.type = entry->type;
        bucket.representative = entry;
      }

      auto &bucket = builder.entries->buckets[bucketIt->second];
      bucket.entries.push_back(entry);
      bucket.entriesByName.emplace(entry->name, entry);
    }

    levels->reserve(builders.size());
    for (auto &[container, builder] : builders) {
      levels->emplace(container, std::move(builder.entries));
    }
    return levels;
  };

  if (document.id == workspace::InvalidDocumentId) {
    return buildLevels();
  }
  return _localScopeCache.get(document.id, LocalScopeCacheKey,
                              [&buildLevels]() { return buildLevels(); });
}

const AstNode *DefaultScopeProvider::loadAstNode(
    const workspace::AstNodeDescription &description) const {
  if (description.node != nullptr) {
    return description.node;
  }
  if (description.documentId == workspace::InvalidDocumentId) {
    return nullptr;
  }
  const auto *documents = coreServices.shared.workspace.documents.get();
  if (documents == nullptr) {
    return nullptr;
  }
  const auto document = documents->getDocument(description.documentId);
  if (!document || !document->hasAst() || document->parseResult.value == nullptr) {
    return nullptr;
  }

  if (const auto *node = document->findAstNode(description.symbolId);
      node != nullptr) {
    return node;
  }
  return nullptr;
}

bool DefaultScopeProvider::acceptsBucket(
    const ScopeQueryContext &context,
    const workspace::ScopeEntryBucket &bucket) const {
  const auto expectedType = context.referenceType;
  if (expectedType == invalid_type() || bucket.type == invalid_type()) {
    return false;
  }
  if (bucket.type == expectedType) {
    return true;
  }

  const auto *reflection = coreServices.shared.astReflection.get();
  if (reflection != nullptr) {
    if (const auto known =
            reflection->lookupSubtype(bucket.type, expectedType);
        known.has_value()) {
      return *known;
    }
  }

  const auto *representative = bucket.representative;
  const auto *target = representative == nullptr
                           ? nullptr
                           : (representative->node != nullptr
                                  ? representative->node
                                  : loadAstNode(*representative));
  const auto matches = target != nullptr && context.accepts(target);

  if (reflection != nullptr) {
    if (matches) {
      reflection->registerSubtype(bucket.type, expectedType);
    } else {
      reflection->registerNonSubtype(bucket.type, expectedType);
    }
  }

  return matches;
}

const workspace::AstNodeDescription *DefaultScopeProvider::findScopeEntry(
    const workspace::BucketedScopeEntries &entries,
    const ScopeQueryContext &context, std::string_view name) const noexcept {
  for (const auto &bucket : entries.buckets) {
    const auto [first, last] = bucket.entriesByName.equal_range(name);
    if (first == last || !acceptsBucket(context, bucket)) {
      continue;
    }
    return first->second;
  }
  return nullptr;
}

void DefaultScopeProvider::appendScopeEntries(
    std::vector<const workspace::AstNodeDescription *> &matches,
    const workspace::BucketedScopeEntries &entries,
    const ScopeQueryContext &context, std::string_view name) const {
  for (const auto &bucket : entries.buckets) {
    if (!acceptsBucket(context, bucket)) {
      continue;
    }
    if (name.empty()) {
      for (const auto *entry : bucket.entries) {
        if (entry != nullptr) {
          matches.push_back(entry);
        }
      }
      continue;
    }
    const auto [first, last] = bucket.entriesByName.equal_range(name);
    if (first == last) {
      continue;
    }
    for (auto rangeIt = first; rangeIt != last; ++rangeIt) {
      if (rangeIt->second != nullptr) {
        matches.push_back(rangeIt->second);
      }
    }
  }
}

std::shared_ptr<const Scope> DefaultScopeProvider::createScope(
    std::shared_ptr<const workspace::BucketedScopeEntries> entries,
    std::shared_ptr<const BucketTypeFilter> filter,
    std::shared_ptr<const Scope> outerScope) const {
  return std::make_shared<SharedBucketedTypeScope>(std::move(entries),
                                                   std::move(filter),
                                                   std::move(outerScope));
}

std::shared_ptr<const Scope> DefaultScopeProvider::getGlobalScope(
    std::shared_ptr<const BucketTypeFilter> filter) const {
  const auto *indexManager = coreServices.shared.workspace.indexManager.get();
  if (indexManager == nullptr) {
    return createScope(std::make_shared<workspace::BucketedScopeEntries>(),
                       std::move(filter));
  }
  return createScope(indexManager->allBucketedScopeEntries(), std::move(filter));
}

} // namespace pegium::references
