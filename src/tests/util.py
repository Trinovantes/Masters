import numpy as np
import logging
import colorlog
import re
import os
import collections


logger = logging.getLogger('ClangAutoMarker')
logger.setLevel(logging.INFO)


#------------------------------------------------------------------------------


global_values = dict()


def init_global(key):
    global global_values
    if key not in global_values:
        global_values[key] = {
            'min': 9999,
            'max': -1,
            'min_source': '',
            'max_source': '',
        }


def update_globals(key, value, source):
    init_global(key)

    global global_values
    if value < global_values[key]['min']:
        global_values[key]['min'] = value
        global_values[key]['min_source'] = source
    if value > global_values[key]['max']:
        global_values[key]['max'] = value
        global_values[key]['max_source'] = source


def print_globals():
    global global_values

    max_key_len = max([len(key) for key in global_values])

    for key, data in global_values.items():
        print('Min {:{max_key_len}} {:>10.5f} {}'.format(key, data['min'], data['min_source'], max_key_len=max_key_len))
        print('Max {:{max_key_len}} {:>10.5f} {}'.format(key, data['max'], data['max_source'], max_key_len=max_key_len))


#------------------------------------------------------------------------------


def parse_output_file(student_ids, data_file, src_file_name):
    n = len(student_ids)
    edit_distances = np.zeros(n)
    idx_map = dict((student, i) for i, student in enumerate(student_ids))

    if not os.path.isfile(data_file):
        logger.warn('No data file {} found'.format(data_file))
        return edit_distances

    with open(data_file, 'r') as f:
        for curr_line in f:
            # Check if we found the current student's mark
            path_pattern = '[/[\w-]+]?/{}\.c'.format(src_file_name)
            pattern = '^stuFile:({}) refFile:({}) stuFn:(\w+) refFn:(\w+) diff:(\d+)'.format(path_pattern, path_pattern)
            match = re.search(pattern, curr_line)
            if match is None:
                continue

            curr_student = match.group(1).split('/')[-3].lower()
            dist = int(match.group(5))

            if curr_student not in idx_map:
                logger.warn('parse_output_file: Found student {} in {} but is not registered'.format(curr_student, data_file))
                continue

            i = idx_map[curr_student]
            edit_distances[i] = edit_distances[i] + dist

    return edit_distances


def parse_marks_file(student_ids, marks_file):
    n = len(student_ids)
    marks = np.zeros(n)
    idx_map = dict((student, i) for i, student in enumerate(student_ids))

    if not os.path.isfile(marks_file):
        logger.warn('No marks file {} found'.format(marks_file))
        return marks

    with open(marks_file, 'r') as f:
        for line in f:
            match = re.search('^#(\w+),(\d+),#', line)
            if match is None:
                continue

            student = match.group(1).lower()
            mark = match.group(2)

            if student not in idx_map:
                logger.warn('parse_marks_file: Found student {} in {} but is not registered'.format(student, marks_file))
                continue

            i = idx_map[student]
            marks[i] = float(mark)

    return marks


def parse_execute_file(student_ids, execute_files):
    n = len(student_ids)
    m = len(execute_files)
    execute_errors = np.full((n, m), '', dtype='S10')
    idx_map = dict((student, i) for i, student in enumerate(student_ids))

    for j, execute_file in enumerate(execute_files):
        if not os.path.isfile(execute_file):
            logger.warn('No execute file {} found'.format(execute_file))
            return execute_errors

        with open(execute_file, 'r') as f:
            for line in f:
                match = re.search('^(\w+) ERROR: \[(\w+)\]', line)
                if match is None:
                    continue

                error_type = match.group(1)
                student = match.group(2).lower()

                if student not in idx_map:
                    logger.warn('parse_execute_file: Found student {} in {} but is not registered'.format(student, execute_file))
                    continue

                i = idx_map[student]
                execute_errors[i][j] = error_type

    return execute_errors
