pegium
==========

[![SonarCloud analysis](https://github.com/ydaveluy/pegium/actions/workflows/sonarcloud.yml/badge.svg)](https://github.com/ydaveluy/pegium/actions/workflows/sonarcloud.yml)
[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=ydaveluy_pegium&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=ydaveluy_pegium)

Pegium is very strongly inspired by
[Langium](https://github.com/eclipse-langium/langium), and many core concepts
are intentionally very similar. The main difference is that Pegium centers on a
PEG-based parser DSL in C++, instead of Langium's TypeScript grammar and parser
stack.

Pegium is a C++20 toolkit for building parsers, AST/CST pipelines, validation,
references, formatters, and language servers.

Documentation
-------------

- [User documentation](https://ydaveluy.github.io/pegium/)
- [Documentation sources](docs/index.md)
- [Getting started](docs/getting-started/index.md)
- [Examples overview](docs/examples/index.md)

Examples
--------

- [Arithmetics](examples/arithmetics/README.md)
- [Domainmodel](examples/domainmodel/README.md)
- [Requirements](examples/requirements/README.md)
- [Statemachine](examples/statemachine/README.md)

Build
-----

```bash
cmake -S . -B build
cmake --build build -j
```

Documentation locally
---------------------

```bash
python3 -m pip install -r requirements-docs.txt
mkdocs serve
```

License
-------

MIT license (© 2024-2026 Yannick Daveluy)
