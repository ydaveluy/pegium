#ifndef PEGIUM_RULE_MACROS_H
#define PEGIUM_RULE_MACROS_H

#define TERM(NAME, ...)                                                        \
  Terminal<__VA_ARGS__> NAME { #NAME }
#define RULE(NAME, ...)                                                        \
  Rule<__VA_ARGS__> NAME { #NAME }
  
#endif // PEGIUM_RULE_MACROS_H