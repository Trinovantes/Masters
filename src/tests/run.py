#!/usr/bin/env python3


import argparse
import sys
import os
import subprocess
import shutil
import logging
import colorlog
import re


logger = logging.getLogger('ClangAutoMarker')
logger.setLevel(logging.DEBUG)


#------------------------------------------------------------------------------


CURRENT_DIR    = os.path.dirname(os.path.realpath(__file__))
LLVM_BUILD_DIR = CURRENT_DIR + '/../llvm-5.0.1.src/build'
CAM_TOOL_EXEC  = LLVM_BUILD_DIR + '/bin/clang-automarker'
CXX_HEADERS    = LLVM_BUILD_DIR + '/include/c++/v1'

CLANG_FLAGS = [
    '-g',             # debug symbols
    '-O0',            # no optimizations
    '-w',             # disable all warnings
    '-I', CXX_HEADERS # c++ headers
]


#------------------------------------------------------------------------------


def compile_llvm():
    logger.info("Compiling LLVM");
    argv = ['cmake', '--build', '.']
    logger.debug(' '.join(argv))
    return subprocess.call(argv, cwd=LLVM_BUILD_DIR)


def run_compiler(reference_solution_files, student_solution_file, marker, assn_cxx_flags, output_stream, debug_stream):
    argv = [CAM_TOOL_EXEC]
    argv.extend(['-cam-reference-solution={}'.format(','.join(reference_solution_files))])
    argv.extend(['-cam-marker={}'.format(marker)])
    argv.extend([student_solution_file])

    argv.extend(['--'])
    argv.extend(assn_cxx_flags)
    argv.extend(CLANG_FLAGS)

    logger.debug(' '.join(argv))
    return subprocess.call(argv, stdout=output_stream, stderr=debug_stream)


def run_a1(reference_folders, student_folder, output_stream, debug_stream):
    logger.info("Running a1 on Student:{} Solutions:{}".format(student_folder, reference_folders))

    cflags = ['-lpng', '-lcurl']
    parts = [
        {
            'marker': 'a1nbio',
            'file': 'paster_nbio.c'
        },
        {
            'marker': 'a1parallel',
            'file': 'paster_parallel.c'
        }
    ]

    for part in parts:
        reference_solutions = ['{}/{}'.format(rf, part['file']) for rf in reference_folders]
        student_solution = '{}/{}'.format(student_folder, part['file'])

        ret = run_compiler(reference_solutions, student_solution, part['marker'], cflags, output_stream, debug_stream)
        if ret != 0:
            logger.error("Failed to mark {}".format(student_folder))


def parse_args():
    parser = argparse.ArgumentParser(description='AutoMarker for ECE459 A1')
    parser.add_argument('--noBuild', default=False, action='store_true',
                        help="Do not build anything")
    parser.add_argument('--outputFile',
                        help="File to pip stdout (results)")
    parser.add_argument('--debugFile',
                        help="File to pipe stderr (debug info)")
    parser.add_argument('--student', default='ece459-a1/sample-solutions/student1/src',
                        help="Student solution directory to mark")
    parser.add_argument('--reference', default=['ece459-a1/sample-solutions/student2/src'], action='append',
                        help="Reference solutions' directory")
    return parser.parse_args()


def main():
    args = parse_args()

    if not args.noBuild:
        if compile_llvm() != 0:
            exit(1)

    output_stream = None
    if args.outputFile is not None:
        output_stream = open(args.outputFile, 'w')

    debug_stream = None
    if args.debugFile is not None:
        debug_stream = open(args.debugFile, 'w')

    run_a1(args.reference, args.student, output_stream, debug_stream)


if __name__ == '__main__':
    handler = colorlog.StreamHandler()
    handler.setFormatter(colorlog.ColoredFormatter('%(log_color)s[%(levelname)s] %(message)s'))
    logger.addHandler(handler)

    main()
