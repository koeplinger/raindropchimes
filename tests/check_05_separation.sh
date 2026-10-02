#!/bin/sh
# The three parts stay apart (docs/REVAMP_PLAN.md, section 5.5):
#   compose/ includes nothing from synth/ or output/;
#   synth/ and output/ include nothing from compose/ and never touch a random generator;
#   output/ sees only frames;
#   score/ stands alone.
fail=0

complain() { echo "separation broken: $1"; fail=1; }

# Lines of code (not comment lines) matching a pattern.
code_lines() {
    pattern=$1; shift
    grep -n "$pattern" "$@" /dev/null | grep -v ':[0-9]*:[[:space:]]*\(/\*\|\*\|//\)'
}

SYSTEM_RANDOM='\(^\|[^A-Za-z_]\)\(rand\|srand\|random\|srandom\|drand48\|rand_r\)[ ]*('

code_lines '#include "\(synth\|output\)/' src/compose/*.[ch] && complain "compose/ includes synth/ or output/"
code_lines '#include "compose/' src/synth/*.[ch] src/output/*.[ch] && complain "synth/ or output/ includes compose/"
code_lines '#include "\(score\|compose\|synth\)/' src/output/*.[ch] && complain "output/ includes more than its own headers"
code_lines '#include "\(compose\|synth\|output\)/' src/score/*.[ch] && complain "score/ includes another part"
code_lines 'rng_\|Rng' src/synth/*.[ch] src/output/*.[ch] src/score/*.[ch] && complain "the generator is used outside compose/"
code_lines "$SYSTEM_RANDOM" src/*/*.[ch] src/main.c && complain "the system's own random generator is called"

[ $fail -eq 0 ] && echo "the parts are separate"
exit $fail
