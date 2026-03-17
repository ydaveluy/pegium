# Services and Modules

Pegium groups language behavior into service objects instead of forcing each
feature to be wired manually.

## Core entry points

- `pegium::services::SharedServices`
- `pegium::services::Services`
- `pegium::services::makeDefaultServices(...)`
- `pegium::services::DefaultLanguageService`

`makeDefaultServices(...)` is the usual starting point. It creates a language
service object, installs the default core services, and installs the default
LSP services.

## Service layout

`Services` inherits the core services and LSP services, and also exposes
`sharedServices` for cross-language and workspace-level infrastructure.

In practice:

- use `Services` for language-specific customization
- use `sharedServices` when the concern truly belongs to shared workspace state

## Recommended composition pattern

1. start with `makeDefaultServices(...)`
2. replace or extend the language-specific providers
3. keep shared infrastructure such as documents and index management untouched
   unless you need framework-level customization

Typical example:

```cpp
auto services = pegium::services::makeDefaultServices(
    sharedServices, "my-language", std::move(parser));

services->lsp.formatter = std::make_unique<lsp::MyFormatter>(*services);
```

## Examples of language-specific overrides

- register a custom formatter
- register a validation registry or validator
- provide a custom scope provider
- swap a completion or hover provider
