#!/usr/bin/python3
from mkpy.utility import *

modes = {
        'debug': '-O0 -g -Wall',
        'profile_debug': '-O2 -g -pg -Wall',
        'release': '-O2 -g -DNDEBUG -Wall'
        }
mode = store('mode', get_cli_arg_opt('-M,--mode', modes.keys()), 'debug')
C_FLAGS = modes[mode]
GTK3_FLAGS = ex ('pkg-config --cflags --libs gtk+-3.0', ret_stdout=True, echo=False)

ensure_dir ("bin")

def default ():
    target = store_get ('last_snip', default='example_procedure')
    call_user_function(target)

def layouter ():
    ex ('gcc {C_FLAGS} -o bin/layouter layouter.c {GTK3_FLAGS} -lm')

def linear_solver_tests ():
    ex ('gcc {C_FLAGS} -o bin/linear_solver_tests linear_solver_tests.c -lm')

if __name__ == "__main__":
    # Everything above this line will be executed for each TAB press.
    # If --get_completions is set, handle_tab_complete() calls exit().
    handle_tab_complete ()

    pymk_default()

