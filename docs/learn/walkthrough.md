# Build a Language End-to-End

Build a complete, working Pegium language by following the smallest full example in the repository: **`statemachine`**. Every file shown here lives under [`examples/statemachine/`](https://github.com/ydaveluy/pegium/tree/main/examples/statemachine), so you can read, run, copy, and adapt it.

A statemachine document looks like this:

```text
statemachine TrafficLight

events
    switchCapacity
    next

initialState PowerOff

state PowerOff
    switchCapacity => RedLight
end
```

You will build an AST, a grammar, validation, a CLI that consumes the parsed model, and an optional language server. Pegium is hand-written C++ with **no code-generation or scaffolding step** — each artifact below is a small source file you write yourself.

## 1. Shape the AST — `ast.hpp`

The AST is your semantic model: plain C++ structs deriving from `pegium::AstNode`, or from `pegium::NamedAstNode` for declarations that have a name.

```cpp
// examples/statemachine/src/statemachine/core/ast.hpp
#include <pegium/core/syntax-tree/AstNode.hpp>

namespace statemachine::ast {

struct Event : pegium::NamedAstNode {};
struct Command : pegium::NamedAstNode {};

struct State;

struct Transition : pegium::AstNode {
  reference<Event> event;
  reference<State> state;
};

struct State : pegium::NamedAstNode {
  vector<reference<Command>> actions;
  vector<pointer<Transition>> transitions;
};

struct Statemachine : pegium::NamedAstNode {
  vector<pointer<Event>> events;
  vector<pointer<Command>> commands;
  reference<State> init;
  vector<pointer<State>> states;
};

} // namespace statemachine::ast
```

The member-type aliases come from `AstNode`:

- `NamedAstNode` supplies the `name` field that the default naming, scoping, and linking services read. Always use it for named declarations — deriving plain `AstNode` with a hand-written `name` compiles but is invisible to the default `NameProvider`.
- `pointer<T>` is an arena-owned child node; `vector<>` holds repeated members.
- `reference<T>` is a cross-link to another node, resolved later by the linker.

## 2. Write the grammar — `core/Parser.hpp`

Subclass `pegium::parser::PegiumParser`, override `getEntryRule()` and `getSkipper()`, and declare your terminals and rules with the PEG DSL. The grammar shapes the AST directly through the `assign`/`append` actions.

```cpp
// examples/statemachine/src/statemachine/core/Parser.hpp
#include <statemachine/core/ast.hpp>
#include <pegium/core/parser/PegiumParser.hpp>

namespace statemachine::parser {
using namespace pegium::parser;

class StateMachineParser : public PegiumParser {
public:
  using PegiumParser::PegiumParser;
  using PegiumParser::parse;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return StatemachineRule;
  }
  const Skipper &getSkipper() const noexcept override { return skipper; }

  // Whitespace is ignored; comments are hidden (kept in the CST for formatting).
  static constexpr auto WS = some(s);
  Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};
  Skipper skipper = skip(ignored(WS), hidden(ML_COMMENT, SL_COMMENT));

  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};

  Rule<ast::Event> EventRule{"Event", assign<&ast::Event::name>(ID)};
  Rule<ast::Command> CommandRule{"Command", assign<&ast::Command::name>(ID)};

  Rule<ast::Transition> TransitionRule{
      "Transition", assign<&ast::Transition::event>(ID) + "=>"_kw +
                        assign<&ast::Transition::state>(ID)};

  Rule<ast::State> StateRule{
      "State",
      "state"_kw.i() + assign<&ast::State::name>(ID) +
          option("actions"_kw.i() + "{"_kw +
                 some(append<&ast::State::actions>(ID)) + "}"_kw) +
          many(append<&ast::State::transitions>(TransitionRule)) +
          "end"_kw.i()};

  Rule<ast::Statemachine> StatemachineRule{
      "Statemachine",
      "statemachine"_kw.i() + assign<&ast::Statemachine::name>(ID) +
          option("events"_kw.i() +
                 append<&ast::Statemachine::events>(EventRule) +
                 many(!("commands"_kw.i() | "initialState"_kw.i()) +
                      append<&ast::Statemachine::events>(EventRule))) +
          option("commands"_kw.i() +
                 append<&ast::Statemachine::commands>(CommandRule) +
                 many(!"initialState"_kw.i() +
                      append<&ast::Statemachine::commands>(CommandRule))) +
          "initialState"_kw.i() + assign<&ast::Statemachine::init>(ID) +
          many(append<&ast::Statemachine::states>(StateRule))};
};

} // namespace statemachine::parser
```

The DSL building blocks:

- `assign<&T::member>(...)` writes one value into a field; `append<&T::vec>(...)` adds a repeated member or child.
- `"kw"_kw` matches a keyword (`.i()` makes it case-insensitive); `"a-zA-Z_"_cr` is a character range; `s`, `w`, `eol`, `eof` are predefined terminals.
- `+` sequences, `|` chooses, `option(...)` / `many(...)` / `some(...)` repeat, and `!x` is a negative lookahead. In `StatemachineRule`, the `!("commands"_kw.i() | "initialState"_kw.i())` guard stops the `events` list as soon as the next section keyword appears.
- The skipper's `ignored(...)` text disappears from the CST entirely; `hidden(...)` (comments) stays available to source-aware features such as formatting and hover.

## 3. Add validation — `core/validation/`

A validator is a plain class with `check`-style methods. Each receives a node and a `ValidationAcceptor` to report problems on.

```cpp
// StatemachineValidator.hpp
class StatemachineValidator final {
public:
  void checkStateNameStartsWithCapital(
      const ast::State &state,
      const pegium::validation::ValidationAcceptor &accept) const;
  void checkUniqueStatesAndEvents(
      const ast::Statemachine &model,
      const pegium::validation::ValidationAcceptor &accept) const;
};
```

```cpp
// StatemachineValidator.cpp
void StatemachineValidator::checkStateNameStartsWithCapital(
    const ast::State &state,
    const pegium::validation::ValidationAcceptor &accept) const {
  if (!state.name.empty() &&
      std::toupper(static_cast<unsigned char>(state.name.front())) !=
          state.name.front()) {
    accept.warning(state, "State name should start with a capital letter.")
        .property<&ast::State::name>();
  }
}
```

Register the checks on the language's `ValidationRegistry`:

```cpp
template <typename TServices>
void registerValidationChecks(TServices &services) {
  auto &registry = *services.validation.validationRegistry;
  auto &validator = *services.validator;
  registry.registerChecks(
      {pegium::validation::ValidationRegistry::makeValidationCheck<
           &StatemachineValidator::checkStateNameStartsWithCapital>(validator),
       pegium::validation::ValidationRegistry::makeValidationCheck<
           &StatemachineValidator::checkUniqueStatesAndEvents>(validator)});
}
```

## 4. Assemble the services — `core/Services.hpp` + `core/Module.cpp`

Pegium wires a language through explicit service containers. Define a container that grafts your language-specific members onto a Pegium base:

```cpp
// core/Services.hpp
struct StatemachineAddedServices {
  std::unique_ptr<validation::StatemachineValidator> validator;
};

struct StatemachineCoreServices final : pegium::CoreServices,
                                        StatemachineAddedServices {
  using pegium::CoreServices::CoreServices;
};
```

Then assemble it in a module function: install the Pegium defaults, then set your parser, file extension, validator, and checks.

```cpp
// core/Module.cpp
template <typename Services>
void applyStatemachineCoreModule(Services &services) {
  services.parser = std::make_unique<const parser::StateMachineParser>(services);
  services.languageMetaData.fileExtensions = {".statemachine"};
  services.validator = std::make_unique<validation::StatemachineValidator>();
  validation::registerValidationChecks(services);
}

std::unique_ptr<StatemachineCoreServices>
createStatemachineServices(const pegium::SharedCoreServices &shared,
                           std::string languageId) {
  auto services = std::make_unique<StatemachineCoreServices>(shared);
  services->languageMetaData.languageId = std::move(languageId);
  pegium::installDefaultCoreServices(*services);
  installStatemachineCoreModule(*services);
  return services;
}
```

The wiring is ordinary C++ you can read, so it stays obvious what your language depends on.

## 5. Run it headlessly — `cli/main.cpp`

The CLI parses a file, checks for errors, then walks the typed AST:

```cpp
auto shared = pegium::cli::make_shared_services();
auto services = statemachine::createStatemachineServices(shared);
auto &languageServices = *services;
shared.serviceRegistry->registerServices(std::move(services));

auto document =
    pegium::cli::build_document_from_path(fileName, languageServices);
if (pegium::cli::has_error_diagnostics(*document)) {
  pegium::cli::print_error_diagnostics(*document, std::cerr);
  return 2;
}

auto *model =
    pegium::ast_ptr_cast<ast::Statemachine>(document->parseResult.value);
for (const auto &state : model->states) {
  // ... do something with the typed AST
}
```

For the full pattern and how cross-references resolve, see [Run a Language Headlessly](consume-the-ast.md).

## 6. (Optional) Add a language server — `lsp/` + `lsp/main.cpp`

For editor features, build the LSP container with `makeDefaultServices<...>` and an LSP module (see [LSP Services](../build-a-language/lsp-services.md)), then start the server with a single call:

```cpp
// lsp/main.cpp
int main(int argc, char **argv) {
  return pegium::runLanguageServerMain(
      argc, argv, "statemachine-lsp",
      statemachine::lsp::registerStatemachineServices);
}
```

## 7. Build and run

```bash
cmake -S . -B build
cmake --build build -j

./build/examples/statemachine/pegium-example-statemachine-cli \
  generate examples/statemachine/example/trafficlight.statemachine
```

## Related pages

- [Test Your Language](test-your-language.md) — assert on the AST and diagnostics
- [Run a Language Headlessly](consume-the-ast.md) — the full consume-the-AST pattern
- [References and Scoping](../build-a-language/references-and-scoping.md) — cross-references across files
- [Validation](../build-a-language/validation.md) — fast vs slow checks
