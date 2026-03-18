# Configuration Services

Pegium configures language behavior through explicit service objects.

Use this page when you want the shortest explanation of how a language is wired
together in Pegium.

## Main idea

Instead of hiding language wiring behind a generator-specific container, Pegium
uses `SharedServices` plus per-language `Services`.

This is where you configure:

- the parser
- reference and scope services
- validation services
- workspace services
- LSP providers

## Recommended entry point

For most projects, start with:

```cpp
auto services = pegium::services::makeDefaultServices(
    sharedServices, "my-language");

services->parser = std::make_unique<const my::parser::MyParser>(*services);
```

Then replace only the language-specific pieces you actually need.

## What usually gets customized

The common language-specific overrides are:

- `services->parser`
- `services->references.scopeProvider`
- `services->references.scopeComputation`
- `services->validation.validationRegistry`
- `services->lsp.formatter`
- selected LSP providers such as hover, rename, or completion

This explicit style is one of the main differences from generator-heavy
language frameworks: Pegium makes the wiring visible in normal C++ code.

## Related pages

- [Start Here](start-here.md)
- [Services and Modules](services-and-modules.md)
- [Default LSP Services](lsp-services.md)
- [Completion Provider](completion-provider.md)
