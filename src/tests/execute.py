#!/usr/bin/env python3


import os
import subprocess
import logging
import colorlog
import pathlib
from multiprocessing import Pool
from contextlib import closing
from functools import partial

import constants


logger = logging.getLogger('ClangAutoMarker')
logger.setLevel(logging.INFO)


#------------------------------------------------------------------------------


class CompilationError(Exception):
    def __init__(self, argv):
        self.argv = argv


class ExecutionError(Exception):
    def __init__(self, argv):
        self.argv = argv


class ImagePathError(Exception):
    def __init__(self):
        pass


class ImageDiffError(Exception):
    def __init__(self, argv):
        self.argv = argv


#------------------------------------------------------------------------------


def find_png(curr_dir):
    img = None

    for f in os.listdir(curr_dir):
        if not f.endswith('.png'):
            continue
        if img is not None:
            raise ImagePathError()
        img = '{}/{}'.format(curr_dir, f)

    if img is None:
        raise ImagePathError()

    return img


#------------------------------------------------------------------------------


def validate_ece459_a1(cwd, executable):
    TIMEOUT = 60 * 5 # Kill student process after 5min
    REFERENCE_IMAGE = '{}/ece459-a1/output.png'.format(constants.ROOT_DIR)

    cwd = '{}/bin'.format(cwd)

    with open(os.devnull, 'w') as dev_null:
        argv = [executable]
        if subprocess.call(argv, cwd=cwd, stdout=dev_null, stderr=dev_null, timeout=TIMEOUT) != 0:
            raise ExecutionError(argv)

        argv = ['diff', REFERENCE_IMAGE, find_png(cwd)]
        if subprocess.call(argv, cwd=cwd, timeout=TIMEOUT, stdout=dev_null) != 0:
            raise ImageDiffError(argv)


#------------------------------------------------------------------------------


def make_prog(cwd, make_rule):
    with open(os.devnull, 'w') as dev_null:
        # First clean student files (ignore any errors caused by non-existent bin/)
        argv = ['make', 'clean']
        subprocess.call(argv, cwd=cwd, stdout=dev_null, stderr=dev_null)

        # Create bin/ if it doesn't exist
        pathlib.Path('{}/bin'.format(cwd)).mkdir(exist_ok=True)

        # Compile
        argv = ['make', make_rule]
        if subprocess.call(argv, cwd=cwd, stdout=dev_null, stderr=dev_null) != 0:
            raise CompilationError(argv)


def execute_a1(student_solution, prog):
    student = student_solution['label']
    cwd = student_solution['root']

    try:
        logger.info('Processing: {}'.format(student))
        make_prog(cwd, prog['make_rule'])
        prog['validator'](cwd, prog['executable'])

    except subprocess.TimeoutExpired:
        logger.error('TIMEOUT: [{}]'.format(student))
    except CompilationError as err:
        logger.error('COMPILER ERROR: [{}] "{}"'.format(student, ' '.join(err.argv)))
    except ExecutionError as err:
        logger.error('EXECUTION ERROR: [{}] "{}"'.format(student, ' '.join(err.argv)))
    except ImagePathError:
        logger.error('PATH ERROR: [{}]'.format(student))
    except ImageDiffError as err:
        logger.error('DIFF ERROR: [{}] "{}"'.format(student, ' '.join(err.argv)))


def batch_execute_a1(prog):
    with closing(Pool()) as p:
        parallel_fn = partial(execute_a1, prog=prog)
        p.map(parallel_fn, constants.TEST_SOLUTIONS)
        p.terminate()


def main():
    batch_execute_a1({
        'make_rule': 'bin/{}'.format(constants.SRC_FILE_NAME),
        'executable': './{}'.format(constants.SRC_FILE_NAME),
        'validator': validate_ece459_a1,
    })


if __name__ == '__main__':
    ch = colorlog.StreamHandler()
    ch.setFormatter(colorlog.ColoredFormatter('%(log_color)s[%(levelname)s] %(message)s'))
    ch.setLevel(logging.INFO)
    logger.addHandler(ch)

    fh = logging.FileHandler('output/execute_log_{}.txt'.format(constants.SRC_FILE_NAME))
    fh.setFormatter(logging.Formatter('%(message)s'))
    fh.setLevel(logging.INFO)
    logger.addHandler(fh)

    main()
