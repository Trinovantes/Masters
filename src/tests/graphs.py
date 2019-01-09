#!/usr/bin/env python3


import os
import re
import pathlib
import numpy as np
from sklearn import preprocessing

from util import parse_marks_file, parse_output_file, parse_execute_file
from mark import get_edit_distances
import constants

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter


#------------------------------------------------------------------------------
# Helper functions to generate graphs for thesis
#------------------------------------------------------------------------------


def visualize_edit_distances():
    base_dir = 'output/edit-dist-hist'
    pathlib.Path(base_dir).mkdir(exist_ok=True)

    # Get data
    normalized_edit_distances, student_edit_distances, invalid_training_students = get_edit_distances(constants.SRC_FILE_NAME, constants.TRAINING_STUDENTS, constants.TRAINING_DIR)

    # Collect marks from each solution output
    for solution_idx, solution in enumerate(constants.REFERENCE_SOLUTIONS):
        edit_distances = student_edit_distances[:,solution_idx]
        norm_edit_distances = normalized_edit_distances[:,solution_idx]

        plt.figure()

        plt.subplot(211)
        plt.title('Raw Edit Distance')
        plt.xlabel('Edit Distance')
        plt.ylabel('Number of Students')
        plt.hist(edit_distances, [x * 20 for x in range(51)])

        plt.subplot(212)
        plt.title('Normalized Edit Distance')
        plt.xlabel('Edit Distance')
        plt.ylabel('Number of Students')
        plt.hist(norm_edit_distances)

        plt.tight_layout()
        plt.savefig('{}/{}.png'.format(base_dir, solution['label']))
        plt.close()


def visualize_actual_marks():
    students_2017_dir = 'ece459-a1/w2017-solutions'
    students_2018_dir = 'ece459-a1/w2018-solutions'
    students_2017 = [s for s in os.listdir(students_2017_dir) if os.path.isdir('./{}/{}'.format(students_2017_dir, s))]
    students_2018 = [s for s in os.listdir(students_2018_dir) if os.path.isdir('./{}/{}'.format(students_2018_dir, s))]
    actual_marks_2017 = parse_marks_file(students_2017, 'ece459-a1/w2017-a1-grades.csv')
    actual_marks_2018 = parse_marks_file(students_2018, 'ece459-a1/w2018-a1-grades.csv')

    def to_percent(y, position):
        # Ignore the passed in position. This has the effect of scaling the default tick locations.
        return '{:.0f}%'.format(100 * y)

    plt.figure()

    plt.subplot(211)
    plt.title('Human Mark Distribution in 2017')
    plt.xlabel('Mark')
    plt.ylabel('Percentage of Students')
    plt.gca().yaxis.set_major_formatter(FuncFormatter(to_percent))

    weights = np.ones_like(actual_marks_2017) / float(len(actual_marks_2017))
    plt.hist(actual_marks_2017, [x * 5 for x in range(21)], weights=weights)

    plt.subplot(212)
    plt.title('Human Mark Distribution in 2018')
    plt.xlabel('Mark')
    plt.ylabel('Percentage of Students')
    plt.gca().yaxis.set_major_formatter(FuncFormatter(to_percent))

    weights = np.ones_like(actual_marks_2018) / float(len(actual_marks_2018))
    plt.hist(actual_marks_2018, [x * 5 for x in range(21)], weights=weights)

    # plt.subplots_adjust(hspace=0.5)
    plt.tight_layout()
    plt.savefig('output/human_marks.png')
    plt.close()


def visualize_enrollment():
    def autolabel(rects):
        for rect in rects:
            height = rect.get_height()
            plt.text(rect.get_x() + rect.get_width() / 2., 1.02 * height, '%d' % int(height), ha='center', va='bottom')

    x = np.arange(5)
    enrollment = [
        # Coop + non-coop students
        2709 + 460,
        3063 + 467,
        3216 + 450,
        3613 + 458,
        3915 + 467,
    ]
    years = [2013 + x for x in range(5)]

    plt.figure()
    plt.xlabel('Year')
    plt.ylabel('Number of Students')
    bar = plt.bar(x, enrollment, align='center')
    autolabel(bar)
    plt.xticks(x, years)
    plt.ylim(0, 5000)

    plt.tight_layout()
    plt.savefig('output/enrollment')
    plt.close()


#------------------------------------------------------------------------------
# Misc Helpers
#------------------------------------------------------------------------------


def find_student_predicted_marks(target):
    with open('output/marks.csv', 'r') as f:
        matches = []
        for line in f:
            match = re.search('^(\w+)\s+,\s+(\d+),\s+(\d+),\s+(\d+),\s+(\d+),\s+(\d+)', line)
            if match is None:
                continue

            student = match.group(1).lower()
            if student != target:
                continue

            matches.append(match)

        return matches


def pick_random_students(n, seed=0):
    np.random.seed(seed)

    for course in constants.COURSE_DATA.values():
        students = course['students']
        idx_map = dict((student, i) for i, student in enumerate(students))
        max_name_len = np.max([len(s) for s in students])
        total = len(students)
        picked_indices = np.random.choice(np.arange(total), n, replace=False)
        picked_students = [students[i] for i in picked_indices]

        actual_marks = parse_marks_file(students, course['marks_file'])
        execution_errors = parse_execute_file(students, course['execute_files'])

        print(course['label'])
        for student in sorted(picked_students):
            error = ""
            for e in execution_errors[idx_map[student]]:
                error += e.decode('utf-8')

            pred_marks = ""
            matches = find_student_predicted_marks(student)
            for j in range(5):
                t = 0
                for m in matches:
                    t += float(m.group(j + 2)) * 0.5

                pred_marks += '{:>3.0f}, , ,'.format(t)

            print('{:<{max_name_len}}, {:<10s}, {:>3}, {}'.format(student, error.strip(), int(actual_marks[idx_map[student]]), pred_marks, max_name_len=max_name_len))

        print()


#------------------------------------------------------------------------------
# Main
#------------------------------------------------------------------------------


if __name__ == '__main__':
    visualize_edit_distances()
    visualize_actual_marks()
    visualize_enrollment()

    pick_random_students(20)
