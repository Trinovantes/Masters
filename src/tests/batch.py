#!/usr/bin/env python3


import os
from multiprocessing import Pool
from contextlib import closing
from functools import partial

from run import compile_llvm, run_a1
import constants


#------------------------------------------------------------------------------


def evaluate(reference_solution, student_solutions):
    label           = reference_solution['label']
    solution_dirs   = [reference_solution['src']]
    output_file     = 'output/{}.stdout'.format(label)
    output_hist_img = 'output/{}-hist.png'.format(label)
    output_dist_img = 'output/{}-dist.png'.format(label)
    output_mark_img = 'output/{}-mark.png'.format(label)

    # Can't parallelize by student solutions because they share the same output stream
    # Can only parallelize by reference solution
    with open(output_file, 'w') as output_stream, open(os.devnull, 'w') as debug_stream:
        for student_solution in student_solutions:
            run_a1(solution_dirs, student_solution['src'], output_stream, debug_stream)


def batch_a1(reference_solutions, student_solutions):
    with closing(Pool()) as p:
        parallel_fn = partial(evaluate, student_solutions=student_solutions)
        p.map(parallel_fn, reference_solutions)
        p.terminate()


def main():
    compile_llvm()
    batch_a1(constants.REFERENCE_SOLUTIONS, constants.TEST_SOLUTIONS)


if __name__ == '__main__':
    main()
