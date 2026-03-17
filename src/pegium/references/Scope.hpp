#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <pegium/utils/Stream.hpp>
#include <pegium/utils/TransparentStringHash.hpp>
#include <pegium/workspace/AstDescriptions.hpp>

namespace pegium::references {

struct ScopeOptions {
  bool caseInsensitive = false;
  bool concatOuterScope = true;
};

class Scope {
public:
  virtual ~Scope() noexcept = default;

  [[nodiscard]] virtual const workspace::AstNodeDescription *
  getElement(std::string_view name) const noexcept = 0;

  [[nodiscard]] virtual utils::stream<const workspace::AstNodeDescription *>
  getElements(std::string_view name) const = 0;

  [[nodiscard]] virtual utils::stream<const workspace::AstNodeDescription *>
  getAllElements() const = 0;
};

class BucketTypeFilter {
public:
  virtual ~BucketTypeFilter() noexcept = default;

  [[nodiscard]] virtual bool
  accepts(const workspace::ScopeEntryBucket &bucket) const = 0;
};

class SharedBucketedTypeScope final : public Scope {
public:
  SharedBucketedTypeScope() = default;
  explicit SharedBucketedTypeScope(
      std::shared_ptr<const workspace::BucketedScopeEntries> elements,
      std::shared_ptr<const BucketTypeFilter> filter,
      std::shared_ptr<const Scope> outerScope = nullptr,
      ScopeOptions options = {});

  [[nodiscard]] const workspace::AstNodeDescription *
  getElement(std::string_view name) const noexcept override;

  [[nodiscard]] utils::stream<const workspace::AstNodeDescription *>
  getElements(std::string_view name) const override;

  [[nodiscard]] utils::stream<const workspace::AstNodeDescription *>
  getAllElements() const override;

private:
  [[nodiscard]] bool acceptsBucket(
      const workspace::ScopeEntryBucket &bucket) const noexcept;

  std::shared_ptr<const workspace::BucketedScopeEntries> _elements;
  std::shared_ptr<const BucketTypeFilter> _filter;
  std::shared_ptr<const Scope> _outerScope;
  ScopeOptions _options;
};

class CompositeBucketedTypeScope final : public Scope {
public:
  CompositeBucketedTypeScope() = default;
  explicit CompositeBucketedTypeScope(
      std::vector<std::shared_ptr<const workspace::BucketedScopeEntries>> levels,
      std::shared_ptr<const BucketTypeFilter> filter,
      std::shared_ptr<const Scope> outerScope = nullptr,
      ScopeOptions options = {});

  [[nodiscard]] const workspace::AstNodeDescription *
  getElement(std::string_view name) const noexcept override;

  [[nodiscard]] utils::stream<const workspace::AstNodeDescription *>
  getElements(std::string_view name) const override;

  [[nodiscard]] utils::stream<const workspace::AstNodeDescription *>
  getAllElements() const override;

private:
  [[nodiscard]] bool acceptsBucket(
      const workspace::ScopeEntryBucket &bucket) const noexcept;

  std::vector<std::shared_ptr<const workspace::BucketedScopeEntries>> _levels;
  std::shared_ptr<const BucketTypeFilter> _filter;
  std::shared_ptr<const Scope> _outerScope;
  ScopeOptions _options;
};

class MapScope final : public Scope {
public:
  MapScope() = default;
  explicit MapScope(std::vector<workspace::AstNodeDescription> elements,
                    std::shared_ptr<const Scope> outerScope = nullptr,
                    ScopeOptions options = {});

  [[nodiscard]] const workspace::AstNodeDescription *
  getElement(std::string_view name) const noexcept override;

  [[nodiscard]] utils::stream<const workspace::AstNodeDescription *>
  getElements(std::string_view name) const override;

  [[nodiscard]] utils::stream<const workspace::AstNodeDescription *>
  getAllElements() const override;

private:
  std::vector<workspace::AstNodeDescription> _elements;
  std::unordered_map<std::string_view, const workspace::AstNodeDescription *,
                     utils::TransparentStringHash, std::equal_to<>>
      _elementsByName;
  std::unordered_map<std::string, const workspace::AstNodeDescription *,
                     utils::TransparentStringHash, std::equal_to<>>
      _caseInsensitiveElementsByName;
  std::shared_ptr<const Scope> _outerScope;
  ScopeOptions _options;
};

class MultiMapScope final : public Scope {
public:
  MultiMapScope() = default;
  explicit MultiMapScope(std::vector<workspace::AstNodeDescription> elements,
                         std::shared_ptr<const Scope> outerScope = nullptr,
                         ScopeOptions options = {});

  [[nodiscard]] const workspace::AstNodeDescription *
  getElement(std::string_view name) const noexcept override;

  [[nodiscard]] utils::stream<const workspace::AstNodeDescription *>
  getElements(std::string_view name) const override;

  [[nodiscard]] utils::stream<const workspace::AstNodeDescription *>
  getAllElements() const override;

private:
  std::vector<workspace::AstNodeDescription> _elements;
  std::unordered_multimap<std::string_view, const workspace::AstNodeDescription *,
                          utils::TransparentStringHash, std::equal_to<>>
      _elementsByName;
  std::unordered_multimap<std::string, const workspace::AstNodeDescription *,
                          utils::TransparentStringHash, std::equal_to<>>
      _caseInsensitiveElementsByName;
  std::shared_ptr<const Scope> _outerScope;
  ScopeOptions _options;
};

class EmptyScope final : public Scope {
public:
  [[nodiscard]] const workspace::AstNodeDescription *
  getElement(std::string_view) const noexcept override;

  [[nodiscard]] utils::stream<const workspace::AstNodeDescription *>
  getElements(std::string_view) const override;

  [[nodiscard]] utils::stream<const workspace::AstNodeDescription *>
  getAllElements() const override;
};

extern const EmptyScope EMPTY_SCOPE;

} // namespace pegium::references
