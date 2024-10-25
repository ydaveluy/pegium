#pragma once

#include <any>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pegium {

class GrammarElement;

struct AstNode {
  virtual ~AstNode() noexcept = default;
};

struct RootCstNode;

/**
 * A node in the Concrete Syntax Tree (CST).
 */
struct CstNode {

  /** The container of the node */
  // const CstNode *container;
  /** The actual text */
  std::string_view text;
  /** The root CST node */
  RootCstNode *root;
  /** The grammar element from which this node was parsed */
  const GrammarElement *grammarSource;
  /** The AST node created from this CST node */
  // std::any astNode;

  class Iterator {
  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = CstNode;
    using pointer = CstNode *;
    using reference = CstNode &;

    explicit Iterator(pointer root = nullptr);
    reference operator*() const;
    pointer operator->() const;
    Iterator &operator++();
    bool operator==(const Iterator &other) const;
    // bool operator!=(const Iterator &other) const;
    void prune();

  private:
    struct NodeFrame {
      pointer node;
      size_t childIndex;
      bool prune;
      NodeFrame(pointer n, size_t index, bool p)
          : node(n), childIndex(index), prune(p) {}
      bool operator==(const NodeFrame &other) const {
        return node == other.node /*&& childIndex == other.childIndex &&
               prune == other.prune*/
            ;
      }
    };

    // std::vector<NodeFrame> stack;
    // std::stack<std::pair<const CstNode*, size_t>> stack;
    std::vector<std::pair<CstNode *, size_t>> stack;
    // static_assert(sizeof(std::stack<std::pair<CstNode*, size_t>>) ==
    // sizeof(std::vector<std::pair<CstNode*, size_t>>));
    bool pruneCurrent = false;

    void advance();
  };

  Iterator begin() { return Iterator(this); }
  Iterator end() { return Iterator(); }

  std::vector<CstNode> content;
  // A leaf CST node corresponds to a token in the input token stream.
  bool isLeaf = false;
  // Whether the token is hidden, i.e. not explicitly part of the containing
  // grammar rule
  bool hidden = false;
};

struct RootCstNode : public CstNode {
  std::string fullText;
};

} // namespace pegium