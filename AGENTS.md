# AGENTS.md

## Repository expectations

- build with -j32
- never run `ctest` while a build is still running; wait for the active build to finish successfully first
- run all tests with ctest 

## User architecture preferences

- When an invariant says a value must exist, prefer `const&` over nullable pointers in APIs and stored metadata. Keep pointers only for genuinely optional data.
- Favor reducing public API surface when helpers can be internalized without hurting clarity.
- ne pas faire de null check sur les pointeurs qui ne sont jamais null par design
- vérifier comment c'est implémenté dans langium lorsque c'est applicable pour ne pas réinventer ni diverger : /home/yannick/git/langium
- ne pas mentionner explicitement langium dans les commentaires et la doc pegium excepté dans le readme principal
- documenter les API public : documentation courte mais utile
- les optimisations doivent restées génériques et ne pas dépendre d'un langage donné
- ne pas utiliser de thread_local
- pas de wrappers qui ne font qu’un appel indirect de plus ou trampoline
- éviter tout check nullptr si l'élément n'est pas null par design
- utiliser C++20 autant que possible
- pas de IIFE si pas de gain en lisibilité
- utiliser void* pour type erasure
- const_cast interdit
- lorsqu'il y a une erreur toujours investiguer pour trouver l'origine du problème plutot que de corriger un symptome
- pegium doit etre 100% générique et ne pas dépendre de la grammaire d'un language spécifique. Rien d'hardcodé dans le parser/recovery
- le parse nominal ne doit en aucun cas avoir un surcout lié au recovery
