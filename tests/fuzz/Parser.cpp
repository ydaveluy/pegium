#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>
#include <pegium/core/utils/UriUtils.hpp>
#include <pegium/core/workspace/Document.hpp>
#include <pegium/core/workspace/TextDocumentProvider.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

using namespace pegium::parser;
namespace grammar = pegium::grammar;

constexpr std::size_t kMaxSmokeInputBytes = 128u * 1024u;
constexpr std::string_view kFuzzLanguageId = "fuzz";
constexpr std::string_view kFuzzUri = "file:///tmp/pegium-fuzz.expr";

struct Expr : pegium::AstNode {};

struct NumberExpr : Expr {
  int value = 0;
};

struct BinaryExpr : Expr {
  pointer<Expr> left;
  std::string op;
  pointer<Expr> right;
};

struct DiagnosticSnapshot {
  ParseDiagnosticKind kind = ParseDiagnosticKind::Deleted;
  pegium::TextOffset offset = 0;
  pegium::TextOffset beginOffset = 0;
  pegium::TextOffset endOffset = 0;
  std::string message;

  bool operator==(const DiagnosticSnapshot &) const = default;
};

struct ExpectSnapshot {
  pegium::TextOffset probeOffset = 0;
  pegium::TextOffset expectOffset = 0;
  bool reachedAnchor = false;
  std::vector<std::string> frontier;

  bool operator==(const ExpectSnapshot &) const = default;
};

struct DocumentSnapshot {
  bool fullMatch = false;
  bool hasAst = false;
  bool hasCst = false;
  std::size_t referenceCount = 0;
  pegium::TextOffset parsedLength = 0;
  pegium::TextOffset lastVisibleCursorOffset = 0;
  pegium::TextOffset failureVisibleCursorOffset = 0;
  pegium::TextOffset maxCursorOffset = 0;
  bool recovered = false;
  std::uint32_t recoveryCount = 0;
  std::vector<DiagnosticSnapshot> diagnostics;
  std::vector<ExpectSnapshot> expectations;

  bool operator==(const DocumentSnapshot &) const = default;
};

class FuzzTextDocuments final : public pegium::workspace::TextDocumentProvider {
public:
  [[nodiscard]] std::shared_ptr<pegium::workspace::TextDocument>
  get(std::string_view uri) const override {
    const auto normalizedUri = pegium::utils::normalize_uri(uri);
    if (normalizedUri.empty()) {
      return nullptr;
    }
    const auto it = _documents.find(normalizedUri);
    return it == _documents.end() ? nullptr : it->second;
  }

  [[nodiscard]] bool
  set(std::shared_ptr<pegium::workspace::TextDocument> document) {
    document = normalize(std::move(document));
    if (document == nullptr) {
      return false;
    }
    const bool inserted = !_documents.contains(document->uri());
    _documents.insert_or_assign(document->uri(), std::move(document));
    return inserted;
  }

  void remove(std::string_view uri) {
    const auto normalizedUri = pegium::utils::normalize_uri(uri);
    if (!normalizedUri.empty()) {
      _documents.erase(normalizedUri);
    }
  }

private:
  [[nodiscard]] static std::shared_ptr<pegium::workspace::TextDocument>
  normalize(std::shared_ptr<pegium::workspace::TextDocument> document) {
    if (document == nullptr) {
      return nullptr;
    }
    const auto normalizedUri = pegium::utils::normalize_uri(document->uri());
    if (normalizedUri.empty()) {
      return nullptr;
    }
    if (normalizedUri == document->uri()) {
      return document;
    }
    return std::make_shared<pegium::workspace::TextDocument>(
        pegium::workspace::TextDocument::create(
            normalizedUri, document->languageId(), document->version(),
            std::string(document->getText())));
  }

  std::unordered_map<std::string,
                     std::shared_ptr<pegium::workspace::TextDocument>>
      _documents;
};

struct FuzzParser final : PegiumParser {
  using PegiumParser::parse;
  using PegiumParser::PegiumParser;

  Terminal<> WS{"WS", some(s)};
  Terminal<int> Number{
      "Number", some(d),
      opt::with_converter(
          [](std::string_view text) noexcept -> opt::ConversionResult<int> {
            if (text == "13") {
              return opt::conversion_error<int>("fuzz conversion failure");
            }
            int value = 0;
            const auto [ptr, ec] =
                std::from_chars(text.data(), text.data() + text.size(), value);
            if (ec != std::errc() || ptr != text.data() + text.size()) {
              return opt::conversion_error<int>("invalid integer");
            }
            return opt::conversion_value<int>(value);
          })};
  Rule<Expr> Primary{"Primary",
                     create<NumberExpr>() + assign<&NumberExpr::value>(Number)};
  Infix<BinaryExpr, &BinaryExpr::left, &BinaryExpr::op, &BinaryExpr::right>
      Expression{"Expression", Primary, LeftAssociation("+"_kw | "-"_kw)};
  Rule<Expr> Root{"Root", Expression};
  Skipper skipper = SkipperBuilder().ignore(WS).build();

  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }

  const Skipper &getSkipper() const noexcept override { return skipper; }

  [[nodiscard]] pegium::parser::ParseResult
  parse(pegium::text::TextSnapshot text,
        const pegium::utils::CancellationToken &cancelToken) const override {
    if (services.references.linker == nullptr) {
      throw std::runtime_error("Parser linker service is not available.");
    }
    return PegiumParser::parse(std::move(text), cancelToken);
  }
};

class WorkspaceHarness {
public:
  WorkspaceHarness() {
    pegium::services::installDefaultSharedCoreServices(_shared);
    _shared.workspace.textDocuments = std::make_shared<FuzzTextDocuments>();

    auto services = std::make_unique<pegium::services::CoreServices>(_shared);
    services->languageMetaData.languageId = std::string(kFuzzLanguageId);
    services->languageMetaData.fileExtensions = {".expr"};
    pegium::services::installDefaultCoreServices(*services);
    services->parser = std::make_unique<const FuzzParser>(*services);

    _shared.serviceRegistry->registerServices(std::move(services));
    (void)_shared.serviceRegistry->getServices(std::string(kFuzzUri));
  }

  const pegium::parser::Parser &parser() const {
    const auto &services =
        _shared.serviceRegistry->getServices(std::string(kFuzzUri));
    if (services.parser == nullptr) {
      throw std::runtime_error("Registered fuzz parser service is missing.");
    }
    return *services.parser;
  }

  const pegium::workspace::Document &
  openAndBuild(std::string source, std::string_view expectedSource = {}) {
    if (_opened) {
      throw std::runtime_error("Fuzz workspace document is already open.");
    }
    ++_version;
    auto textDocument = std::make_shared<pegium::workspace::TextDocument>(
        pegium::workspace::TextDocument::create(std::string(kFuzzUri),
                                                std::string(kFuzzLanguageId),
                                                _version,
                                                std::move(source)));
    auto *documents = dynamic_cast<FuzzTextDocuments *>(
        _shared.workspace.textDocuments.get());
    if (documents == nullptr) {
      throw std::runtime_error("Missing fuzz text document store.");
    }
    (void)documents->set(textDocument);
    const auto storedTextDocument = documents->get(kFuzzUri);
    if (storedTextDocument == nullptr) {
      throw std::runtime_error("Failed to open fuzz text document.");
    }
    _documentId =
        _shared.workspace.documents->getOrCreateDocumentId(
            storedTextDocument->uri());
    if (_documentId == pegium::workspace::InvalidDocumentId) {
      throw std::runtime_error("Failed to allocate a fuzz document id.");
    }
    _opened = true;
    return buildChangedDocument(expectedSource.empty()
                                    ? std::string_view{storedTextDocument->getText()}
                                    : expectedSource);
  }

  const pegium::workspace::Document &
  replaceAndBuild(std::string source, std::string_view expectedSource = {}) {
    ensure_open();
    auto *documents = dynamic_cast<FuzzTextDocuments *>(
        _shared.workspace.textDocuments.get());
    if (documents == nullptr) {
      throw std::runtime_error("Missing fuzz text document store.");
    }
    auto textDocument = documents->get(kFuzzUri);
    if (textDocument == nullptr) {
      throw std::runtime_error("Missing open fuzz text document.");
    }

    ++_version;
    const pegium::workspace::TextDocumentContentChangeEvent change{
        .text = std::move(source)};
    (void)pegium::workspace::TextDocument::update(
        *textDocument, std::span(&change, std::size_t{1}), _version);
    return buildChangedDocument(expectedSource.empty()
                                    ? std::string_view{textDocument->getText()}
                                    : expectedSource);
  }

  const pegium::workspace::Document &
  appendAndBuild(std::string suffix, std::string_view expectedSource) {
    ensure_open();
    auto *documents = dynamic_cast<FuzzTextDocuments *>(
        _shared.workspace.textDocuments.get());
    if (documents == nullptr) {
      throw std::runtime_error("Missing fuzz text document store.");
    }
    auto textDocument = documents->get(kFuzzUri);
    if (textDocument == nullptr) {
      throw std::runtime_error("Missing open fuzz text document.");
    }

    ++_version;
    const pegium::workspace::TextDocumentContentChangeEvent change{
        .text = std::string(textDocument->getText()) + suffix};
    (void)pegium::workspace::TextDocument::update(
        *textDocument, std::span(&change, std::size_t{1}), _version);

    return buildChangedDocument(expectedSource);
  }

  void closeAndDelete() {
    ensure_open();
    auto *documents = dynamic_cast<FuzzTextDocuments *>(
        _shared.workspace.textDocuments.get());
    if (documents == nullptr) {
      throw std::runtime_error("Missing fuzz text document store.");
    }
    documents->remove(kFuzzUri);

    const std::array<pegium::workspace::DocumentId, 1> deletedDocumentIds{
        _documentId};
    _shared.workspace.documentBuilder->update({}, deletedDocumentIds);
    if (_shared.workspace.documents->getDocument(_documentId) != nullptr) {
      throw std::runtime_error(
          "Fuzz document was not removed from the workspace.");
    }
    if (documents->get(kFuzzUri) != nullptr) {
      throw std::runtime_error("Closed fuzz text document is still visible.");
    }
    (void)_shared.serviceRegistry->getServices(std::string(kFuzzUri));
    _opened = false;
  }

private:
  void ensure_open() const {
    if (!_opened) {
      throw std::runtime_error("Fuzz workspace document is not open.");
    }
  }

  const pegium::workspace::Document &
  buildChangedDocument(std::string_view expectedSource) const {
    const std::array<pegium::workspace::DocumentId, 1> changedDocumentIds{
        _documentId};
    _shared.workspace.documentBuilder->update(changedDocumentIds, {});

    const auto document = _shared.workspace.documents->getDocument(_documentId);
    if (document == nullptr) {
      throw std::runtime_error("Failed to materialize the fuzz document.");
    }
    const auto &textDocument = document->textDocument();
    if (textDocument.getText() != expectedSource) {
      throw std::runtime_error("Workspace document content diverged from the "
                               "text document content.");
    }
    if (textDocument.version() != _version) {
      throw std::runtime_error("Workspace document version is stale.");
    }

    const auto conversionDiagnostics = std::ranges::count_if(
        document->parseResult.parseDiagnostics, [](const auto &diagnostic) {
          return diagnostic.kind == ParseDiagnosticKind::ConversionError;
        });
    const auto extractedConversionDiagnostics =
        std::ranges::count_if(document->diagnostics, [](const auto &diagnostic) {
          return diagnostic.code.has_value() &&
                 std::holds_alternative<std::string>(*diagnostic.code) &&
                 std::get<std::string>(*diagnostic.code) == "parse.conversion";
        });
    if (conversionDiagnostics != extractedConversionDiagnostics) {
      throw std::runtime_error(
          "Workspace diagnostics diverged from parse conversion diagnostics.");
    }

    return *document;
  }

  pegium::services::SharedCoreServices _shared;
  pegium::workspace::DocumentId _documentId =
      pegium::workspace::InvalidDocumentId;
  std::int64_t _version = 0;
  bool _opened = false;
};

std::string describe_expect_path(const ExpectPath &path) {
  std::string result;
  for (const auto *element : path.elements) {
    if (!result.empty()) {
      result += ">";
    }
    assert(element != nullptr);
    switch (element->getKind()) {
    case grammar::ElementKind::Literal:
      result += "keyword:" +
                std::string(
                    static_cast<const grammar::Literal *>(element)->getValue());
      break;
    case grammar::ElementKind::Assignment: {
      const auto *assignment =
          static_cast<const grammar::Assignment *>(element);
      result += "assignment:" + std::string(assignment->getFeature());
      break;
    }
    case grammar::ElementKind::DataTypeRule:
    case grammar::ElementKind::ParserRule:
    case grammar::ElementKind::TerminalRule:
    case grammar::ElementKind::InfixRule:
      result +=
          "rule:" +
          std::string(
              static_cast<const grammar::AbstractRule *>(element)->getName());
      break;
    default:
      result += "kind:" + std::to_string(static_cast<int>(element->getKind()));
      break;
    }
  }
  if (!result.empty()) {
    return result;
  }
  if (const auto *literal = path.literal(); literal != nullptr) {
    return "keyword:" + std::string(literal->getValue());
  }
  if (const auto *assignment = path.expectedReferenceAssignment();
      assignment != nullptr) {
    return "reference:" +
           std::string(assignment->getType().name()) +
           (assignment->isMultiReference() ? ":multi" : ":single");
  }
  if (const auto *rule = path.expectedRule(); rule != nullptr) {
    return "rule:" + std::string(rule->getName());
  }
  return "unknown";
}

std::string snapshot_to_string(const DocumentSnapshot &snapshot) {
  std::ostringstream stream;
  stream << "{fullMatch=" << snapshot.fullMatch
         << ", hasAst=" << snapshot.hasAst << ", hasCst=" << snapshot.hasCst
         << ", referenceCount=" << snapshot.referenceCount
         << ", parsedLength=" << snapshot.parsedLength
         << ", lastVisibleCursorOffset=" << snapshot.lastVisibleCursorOffset
         << ", failureVisibleCursorOffset="
         << snapshot.failureVisibleCursorOffset
         << ", maxCursorOffset=" << snapshot.maxCursorOffset
         << ", recovered=" << snapshot.recovered
         << ", recoveryCount=" << snapshot.recoveryCount << ", diagnostics=[";
  for (std::size_t index = 0; index < snapshot.diagnostics.size(); ++index) {
    const auto &diagnostic = snapshot.diagnostics[index];
    if (index != 0) {
      stream << ", ";
    }
    stream << "{kind=" << diagnostic.kind << ", offset=" << diagnostic.offset
           << ", begin=" << diagnostic.beginOffset
           << ", end=" << diagnostic.endOffset
           << ", message=" << diagnostic.message << "}";
  }
  stream << "], expectations=[";
  for (std::size_t index = 0; index < snapshot.expectations.size(); ++index) {
    const auto &expectation = snapshot.expectations[index];
    if (index != 0) {
      stream << ", ";
    }
    stream << "{probe=" << expectation.probeOffset
           << ", offset=" << expectation.expectOffset
           << ", reachedAnchor=" << expectation.reachedAnchor << ", frontier=[";
    for (std::size_t frontierIndex = 0;
         frontierIndex < expectation.frontier.size(); ++frontierIndex) {
      if (frontierIndex != 0) {
        stream << ", ";
      }
      stream << expectation.frontier[frontierIndex];
    }
    stream << "]}";
  }
  stream << "]}";
  return stream.str();
}

void ensure_matching_snapshot(std::string_view context,
                              const DocumentSnapshot &direct,
                              const DocumentSnapshot &workspace) {
  if (direct == workspace) {
    return;
  }

  std::ostringstream stream;
  stream << "Direct and workspace parser results diverged for " << context
         << ". direct=" << snapshot_to_string(direct)
         << " workspace=" << snapshot_to_string(workspace);
  throw std::runtime_error(stream.str());
}

std::string read_file(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("Failed to open input file: " + path.string());
  }
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}

std::vector<std::filesystem::path> collect_inputs(int argc, char **argv) {
  std::vector<std::filesystem::path> paths;
  for (int index = 1; index < argc; ++index) {
    const std::string_view arg = argv[index];
    if (arg == "--corpus") {
      if (index + 1 >= argc) {
        throw std::runtime_error("--corpus expects a directory path.");
      }
      const auto root = std::filesystem::path(argv[++index]);
      if (!std::filesystem::exists(root)) {
        throw std::runtime_error("Corpus path does not exist: " +
                                 root.string());
      }
      for (const auto &entry :
           std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file()) {
          paths.push_back(entry.path());
        }
      }
      continue;
    }

    paths.emplace_back(arg);
  }

  std::ranges::sort(paths);
  const auto [uniqueBegin, uniqueEnd] = std::ranges::unique(paths);
  paths.erase(uniqueBegin, uniqueEnd);
  return paths;
}

std::vector<pegium::TextOffset>
collect_probe_offsets(const pegium::workspace::Document &document) {
  const auto textSize =
      static_cast<pegium::TextOffset>(document.textDocument().getText().size());
  std::vector<pegium::TextOffset> offsets{
      0,
      static_cast<pegium::TextOffset>(textSize / 4),
      static_cast<pegium::TextOffset>(textSize / 2),
      static_cast<pegium::TextOffset>((textSize * 3) / 4),
      textSize,
      std::min(document.parseResult.parsedLength, textSize),
      std::min(document.parseResult.lastVisibleCursorOffset, textSize),
      std::min(document.parseResult.failureVisibleCursorOffset, textSize),
      std::min(document.parseResult.maxCursorOffset, textSize),
  };

  for (const auto &diagnostic : document.parseResult.parseDiagnostics) {
    offsets.push_back(std::min(diagnostic.offset, textSize));
    offsets.push_back(std::min(diagnostic.beginOffset, textSize));
    offsets.push_back(std::min(diagnostic.endOffset, textSize));
  }

  std::ranges::sort(offsets);
  const auto [uniqueBegin, uniqueEnd] = std::ranges::unique(offsets);
  offsets.erase(uniqueBegin, uniqueEnd);
  return offsets;
}

DocumentSnapshot snapshot_document(const pegium::workspace::Document &document,
                                   const pegium::parser::Parser &parser) {
  if (document.references.size() != document.parseResult.references.size()) {
    throw std::runtime_error(
        "Document references diverged from parse result references.");
  }

  const auto textSize =
      static_cast<pegium::TextOffset>(document.textDocument().getText().size());
  if (document.parseResult.parsedLength > textSize ||
      document.parseResult.lastVisibleCursorOffset > textSize ||
      document.parseResult.failureVisibleCursorOffset > textSize ||
      document.parseResult.maxCursorOffset > textSize) {
    throw std::runtime_error("Parse offsets escaped the source document.");
  }

  DocumentSnapshot snapshot{
      .fullMatch = document.parseResult.fullMatch,
      .hasAst = document.parseResult.value != nullptr,
      .hasCst = document.parseResult.cst != nullptr,
      .referenceCount = document.references.size(),
      .parsedLength = document.parseResult.parsedLength,
      .lastVisibleCursorOffset = document.parseResult.lastVisibleCursorOffset,
      .failureVisibleCursorOffset =
          document.parseResult.failureVisibleCursorOffset,
      .maxCursorOffset = document.parseResult.maxCursorOffset,
      .recovered = document.parseResult.recoveryReport.hasRecovered,
      .recoveryCount = document.parseResult.recoveryReport.recoveryCount,
  };

  snapshot.diagnostics.reserve(document.parseResult.parseDiagnostics.size());
  for (const auto &diagnostic : document.parseResult.parseDiagnostics) {
    if (diagnostic.kind == ParseDiagnosticKind::ConversionError &&
        diagnostic.message.empty()) {
      throw std::runtime_error("Conversion diagnostics must carry a message.");
    }
    if (diagnostic.beginOffset > diagnostic.endOffset ||
        diagnostic.endOffset > textSize) {
      throw std::runtime_error(
          "Parse diagnostic span escaped the source document.");
    }

    snapshot.diagnostics.push_back({.kind = diagnostic.kind,
                                    .offset = diagnostic.offset,
                                    .beginOffset = diagnostic.beginOffset,
                                    .endOffset = diagnostic.endOffset,
                                    .message = diagnostic.message});
  }

  const auto probeOffsets = collect_probe_offsets(document);
  snapshot.expectations.reserve(probeOffsets.size());
  for (const auto probeOffset : probeOffsets) {
    const auto expect =
        parser.expect(document.textDocument().getText(), probeOffset);
    if (expect.offset > textSize) {
      throw std::runtime_error("Expect offset escaped the source document.");
    }

    ExpectSnapshot expectSnapshot{
        .probeOffset = probeOffset,
        .expectOffset = expect.offset,
        .reachedAnchor = expect.reachedAnchor,
    };
    expectSnapshot.frontier.reserve(expect.frontier.size());
    for (const auto &item : expect.frontier) {
      expectSnapshot.frontier.push_back(describe_expect_path(item));
    }
    std::ranges::sort(expectSnapshot.frontier);
    snapshot.expectations.push_back(std::move(expectSnapshot));
  }

  return snapshot;
}

DocumentSnapshot parse_direct(std::string source) {
  FuzzParser parser;
  auto textDocument = std::make_shared<pegium::workspace::TextDocument>(
      pegium::workspace::TextDocument::create(std::string(kFuzzUri),
                                              std::string(kFuzzLanguageId), 1,
                                              std::move(source)));
  pegium::workspace::Document document(std::move(textDocument));
  document.parseResult = parser.parse(
      pegium::text::TextSnapshot::copy(document.textDocument().getText()), {});
  document.references = document.parseResult.references;
  if (document.parseResult.cst != nullptr) {
    document.parseResult.cst->attachDocument(document);
  }
  return snapshot_document(document, parser);
}

void append_unique_case(std::vector<std::string> &cases,
                        std::string candidate) {
  if (std::ranges::find(cases, candidate) == cases.end()) {
    cases.push_back(std::move(candidate));
  }
}

std::vector<std::string> build_cases(std::string_view seed) {
  if (seed.size() > kMaxSmokeInputBytes) {
    throw std::runtime_error(
        "Smoke fuzz input exceeds the configured size cap.");
  }

  std::vector<std::string> cases;
  cases.reserve(10);
  append_unique_case(cases, std::string(seed));
  append_unique_case(cases, std::string(seed) + "\n");
  append_unique_case(cases, std::string(seed) + "+");

  append_unique_case(cases, "");
  append_unique_case(cases, "1 + 2 - 3 + 5");
  append_unique_case(cases, "13 + 21 - 13 + 8");
  append_unique_case(cases, "1 + + 2 - 4 + 5");
  append_unique_case(cases, "1 +\n 2 - 3 + 5");
  append_unique_case(cases, " \t\n13 + 2");

  if (!seed.empty()) {
    std::string nulTerminated(seed);
    nulTerminated.push_back('\0');
    append_unique_case(cases, std::move(nulTerminated));
  }

  return cases;
}

void exercise_single_parse_case(std::string source, std::string_view context) {
  const auto direct = parse_direct(source);

  WorkspaceHarness workspace;
  const auto &document = workspace.openAndBuild(std::move(source));
  const auto workspaceSnapshot =
      snapshot_document(document, workspace.parser());
  ensure_matching_snapshot(context, direct, workspaceSnapshot);
  workspace.closeAndDelete();
}

void exercise_workspace_lifecycle() {
  WorkspaceHarness workspace;

  const std::string initial = "1 + 2";
  const auto initialDirect = parse_direct(initial);
  auto currentSnapshot =
      snapshot_document(workspace.openAndBuild(initial), workspace.parser());
  ensure_matching_snapshot("workspace lifecycle initial open", initialDirect,
                           currentSnapshot);

  const std::string appended = initial + "\n+ 13";
  const auto appendedDirect = parse_direct(appended);
  currentSnapshot = snapshot_document(
      workspace.appendAndBuild("\n+ 13", appended), workspace.parser());
  ensure_matching_snapshot("workspace lifecycle incremental append",
                           appendedDirect, currentSnapshot);

  const std::string replaced = "1 + + 2 - 4 + 5";
  const auto replacedDirect = parse_direct(replaced);
  currentSnapshot = snapshot_document(workspace.replaceAndBuild(replaced),
                                      workspace.parser());
  ensure_matching_snapshot("workspace lifecycle full replacement",
                           replacedDirect, currentSnapshot);

  workspace.closeAndDelete();

  const std::string reopened = "13 + 21 - 13 + 8";
  const auto reopenedDirect = parse_direct(reopened);
  currentSnapshot =
      snapshot_document(workspace.openAndBuild(reopened), workspace.parser());
  ensure_matching_snapshot("workspace lifecycle reopen", reopenedDirect,
                           currentSnapshot);

  workspace.closeAndDelete();
}

void exercise_input(std::string_view text) {
  const auto cases = build_cases(text);
  for (std::size_t index = 0; index < cases.size(); ++index) {
    exercise_single_parse_case(cases[index],
                               "case[" + std::to_string(index) + "]");
  }
  exercise_workspace_lifecycle();
}

} // namespace

int main(int argc, char **argv) {
  try {
    const auto inputs = collect_inputs(argc, argv);
    if (inputs.empty()) {
      std::string stdinInput{std::istreambuf_iterator<char>(std::cin),
                             std::istreambuf_iterator<char>()};
      exercise_input(stdinInput);
      return 0;
    }

    for (const auto &path : inputs) {
      exercise_input(read_file(path));
    }
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Fuzz target failed: " << error.what() << '\n';
    return 1;
  }
}
