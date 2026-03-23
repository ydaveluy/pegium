# Multiple Languages

This guide is about integrating multiple dependent languages in one Pegium
workspace.

One common reason to split a language is that one file type defines concepts
while another one consumes them. Pegium's `requirements` example shows exactly
that setup with `.req` and `.tst` files sharing one workspace.

## What is the core idea?

In Pegium, multiple languages usually share the same
`pegium::SharedServices`, while each concrete language gets its own
`pegium::Services` instance.

That means:

- documents, indexing, and workspace infrastructure stay shared
- each language still chooses its own parser
- each language can install its own formatter and validators
- all registered languages become visible to the same service registry

## A living example

Look at `examples/requirements/src/RequirementsModule.cpp`.

The example builds one service factory for requirements files and another one
for test files:

```cpp
std::unique_ptr<RequirementsLangServices>
make_requirements_services(const pegium::SharedServices &sharedServices,
                           std::string languageId) {
  auto services = pegium::services::makeDefaultServices<RequirementsLangServices>(
      sharedServices, std::move(languageId));
  services->parser =
      std::make_unique<const requirements::parser::RequirementsParser>(*services);
  services->languageMetaData.fileExtensions = {".req"};
  services->lsp.formatter = std::make_unique<lsp::RequirementsFormatter>(*services);
  validation::registerRequirementsValidationChecks(*services);
  return services;
}
```

The second factory follows the same pattern but swaps parser, extensions,
formatter, and validator for the `tests-lang` language.

## Registration

Once each language has its own `Services` object, register all of them in the
shared registry:

```cpp
bool register_language_services(pegium::SharedServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      make_requirements_services(sharedServices, "requirements-lang"));
  sharedServices.serviceRegistry->registerServices(
      make_tests_services(sharedServices, "tests-lang"));
  return true;
}
```

This is the important step that makes both languages available to document
loading, indexing, and LSP requests.

## What usually changes when you split a language?

Expect to review these points for each language:

- parser type
- language id
- file extensions
- formatter
- validation registration
- editor integration such as syntax highlighting or VS Code contributions

Keep the shared services common unless you intentionally want isolated
workspaces.

## When should you not split?

Do not split a language just because the grammar has several entry rules. Keep
one language when the files share the same lifecycle, the same editor behavior,
and the same symbol space.

Create multiple languages when the file types have clearly distinct identities
or when separate parsers and editor behavior make the model easier to maintain.

## Related pages

- [Examples: Requirements](../examples/requirements.md)
- [Scoping](scoping/index.md)
- [Configuration Services](../reference/configuration-services.md)
