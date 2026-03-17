#pragma once

#include <cstdint>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include <pegium/references/Scope.hpp>
#include <pegium/references/ScopeProvider.hpp>
#include <pegium/services/DefaultCoreService.hpp>
#include <pegium/syntax-tree/Reference.hpp>
#include <pegium/utils/Caching.hpp>
#include <pegium/workspace/Document.hpp>
#include <pegium/workspace/IndexManager.hpp>

namespace pegium::services {
struct CoreServices;
}

namespace pegium::references {

class DefaultScopeProvider : public ScopeProvider,
                             protected services::DefaultCoreService {
public:
  explicit DefaultScopeProvider(const services::CoreServices &services);

  std::shared_ptr<const Scope>
  getScope(const ReferenceInfo &context) const override {
    return getScope(makeScopeQueryContext(context));
  }
  std::shared_ptr<const Scope>
  getScope(const ScopeQueryContext &context) const override;
  [[nodiscard]] const workspace::AstNodeDescription *
  getScopeEntry(const ReferenceInfo &context) const noexcept override {
    return getScopeEntry(makeScopeQueryContext(context));
  }
  [[nodiscard]] const workspace::AstNodeDescription *
  getScopeEntry(const ScopeQueryContext &context) const noexcept override;
  [[nodiscard]] utils::stream<const workspace::AstNodeDescription *>
  getScopeEntries(const ReferenceInfo &context) const override {
    return getScopeEntries(makeScopeQueryContext(context));
  }
  [[nodiscard]] utils::stream<const workspace::AstNodeDescription *>
  getScopeEntries(const ScopeQueryContext &context) const override;

private:
  class ReferenceTypeFilter;
  using LocalScopeLevels =
      std::unordered_map<const AstNode *,
                         std::shared_ptr<const workspace::BucketedScopeEntries>>;
  static constexpr std::uint8_t LocalScopeCacheKey = 0;

  [[nodiscard]] std::shared_ptr<const LocalScopeLevels>
  getLocalScopeLevels(const workspace::Document &document) const;
  [[nodiscard]] const AstNode *
  loadAstNode(const workspace::AstNodeDescription &description) const;
  [[nodiscard]] bool acceptsBucket(const ScopeQueryContext &context,
                                   const workspace::ScopeEntryBucket &bucket) const;
  [[nodiscard]] const workspace::AstNodeDescription *findScopeEntry(
      const workspace::BucketedScopeEntries &entries, const ScopeQueryContext &context,
      std::string_view name) const noexcept;
  void appendScopeEntries(
      std::vector<const workspace::AstNodeDescription *> &matches,
      const workspace::BucketedScopeEntries &entries, const ScopeQueryContext &context,
      std::string_view name) const;
  [[nodiscard]] std::shared_ptr<const Scope> createScope(
      std::shared_ptr<const workspace::BucketedScopeEntries> entries,
      std::shared_ptr<const BucketTypeFilter> filter,
      std::shared_ptr<const Scope> outerScope = nullptr) const;
  [[nodiscard]] std::shared_ptr<const Scope>
  getGlobalScope(std::shared_ptr<const BucketTypeFilter> filter) const;

  mutable utils::DocumentCache<std::uint8_t,
                               std::shared_ptr<const LocalScopeLevels>>
      _localScopeCache;
};

} // namespace pegium::references
