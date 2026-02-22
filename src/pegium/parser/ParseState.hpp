#pragma once

#include <cstddef>
#include <cstdint>
#include <pegium/grammar/AbstractElement.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/StepTrace.hpp>

namespace pegium::parser {

struct ParseState {
  using Checkpoint = CstBuilder::Checkpoint;
  const char *const begin;
  const char *const end;

  ParseState(CstBuilder &builder, const ParseContext &context) noexcept
      : begin(builder.input_begin()), end(builder.input_end()), _cursor(begin),
        _maxCursor(begin), builder(builder), context(context) {}

  inline void skipHiddenNodes() noexcept {
    _cursor = context.skipHiddenNodes(_cursor, end, builder);
    if (_cursor > _maxCursor) {
      _maxCursor = _cursor;
    }
  }

  [[nodiscard]] inline Checkpoint mark() const noexcept {
    detail::stepTraceInc(detail::StepCounter::ParseStateMark);
    return builder.mark(_cursor);
  }

  inline void
  rewind(const Checkpoint &checkpoint) noexcept {
    detail::stepTraceInc(detail::StepCounter::ParseStateRewind);
    _cursor = builder.rewind(checkpoint);
  }

  [[nodiscard]] inline Checkpoint enter() noexcept {
    detail::stepTraceInc(detail::StepCounter::ParseStateEnter);
    const auto checkpoint = builder.mark(_cursor);
    builder.enter(_cursor);
    return checkpoint;
  }

  inline void
  exit(const grammar::AbstractElement *element) noexcept {
    detail::stepTraceInc(detail::StepCounter::ParseStateExit);
    builder.exit(_cursor, element);
  }

  inline void leaf(const char *endPtr,
                   const grammar::AbstractElement *element,
                   bool hidden = false) {
    detail::stepTraceInc(detail::StepCounter::ParseStateLeaf);
    builder.leaf(_cursor, endPtr, element, hidden);
    _cursor = endPtr;
    if (_cursor > _maxCursor) {
      _maxCursor = _cursor;
    }
  }

  [[nodiscard]] inline uint64_t
  node_count() const noexcept {
    return builder.node_count();
  }

  inline void
  override_grammar_element(NodeId id,
                           const grammar::AbstractElement *element) noexcept {
    builder.override_grammar_element(id, element);
  }

  constexpr const char *cursor() const noexcept { return _cursor; }
  constexpr const char *maxCursor() const noexcept { return _maxCursor; }
  [[nodiscard]] constexpr std::size_t cursorOffset() const noexcept {
    return static_cast<std::size_t>(_cursor - begin);
  }
  [[nodiscard]] constexpr std::size_t maxCursorOffset() const noexcept {
    return static_cast<std::size_t>(_maxCursor - begin);
  }

private:
  const char *_cursor;
  const char *_maxCursor;
  CstBuilder &builder;
  const ParseContext &context;
};

} // namespace pegium::parser
