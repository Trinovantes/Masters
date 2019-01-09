import os
from util import parse_marks_file


#------------------------------------------------------------------------------
# Course Data
#------------------------------------------------------------------------------


ROOT_DIR = os.path.dirname(os.path.realpath(__file__))


def get_course_data():
    course_data = {
        'ece459-a1-w2017': {
            'label'         : 'ece459-a1-w2017',
            'marks_file'    : 'ece459-a1/w2017-a1-grades.csv',
            'solutions_dir' : 'ece459-a1/w2017-solutions',
            'distance_dir'  : 'ece459-a1/w2017-edit-distances',
            'execute_files' : [
                'ece459-a1/execute_log_human_w2017.txt',
                'ece459-a1/execute_log_paster_nbio_w2017.txt',
                'ece459-a1/execute_log_paster_parallel_w2017.txt',
            ],

            # To be filled later
            'students' : [],
            'solutions' : [{}],
        },
        'ece459-a1-w2018': {
            'label'         : 'ece459-a1-w2018',
            'marks_file'    : 'ece459-a1/w2018-a1-grades.csv',
            'solutions_dir' : 'ece459-a1/w2018-solutions',
            'distance_dir'  : 'ece459-a1/w2018-edit-distances',
            'execute_files' : [
                'ece459-a1/execute_log_human_w2018.txt',
                'ece459-a1/execute_log_paster_nbio_w2018.txt',
                'ece459-a1/execute_log_paster_parallel_w2018.txt',
            ],
        },
    }

    for course in course_data:
        students_dir = course_data[course]['solutions_dir']
        students = [s for s in os.listdir(students_dir) if os.path.isdir('./{}/{}'.format(students_dir, s))]
        students = sorted(students)

        course_data[course]['students'] = students
        course_data[course]['solutions'] = [{
            'label': student,
            'root': '{}/{}/{}'.format(ROOT_DIR, students_dir, student),
            'src': '{}/{}/{}/src'.format(ROOT_DIR, students_dir, student),
        } for student in students]

    return course_data


COURSE_DATA = get_course_data()


#------------------------------------------------------------------------------
# Public Constants
#------------------------------------------------------------------------------


# Minimum edit distance to be considered valid (for some reason, the same file have a non-zero edit distance and thus must be excluded)
MIN_EDIT_DISTANCE = 50

# Out of 100, this is the min difference between predicted to actual mark before we consider our prediction is a false positive
# i.e. student received higher mark than they earned
FALSE_POSITIVE_THRESHOLD = 10

SRC_FILE_NAME = 'paster_nbio'

TEST_COURSE    = COURSE_DATA['ece459-a1-w2018']
TEST_STUDENTS  = TEST_COURSE['students']
TEST_DIR       = TEST_COURSE['distance_dir']
TEST_SOLUTIONS = TEST_COURSE['solutions']

TRAINING_COURSE    = COURSE_DATA['ece459-a1-w2017']
TRAINING_STUDENTS  = TRAINING_COURSE['students']
TRAINING_DIR       = TRAINING_COURSE['distance_dir']
TRAINING_SOLUTIONS = TEST_COURSE['solutions']


def get_reference_solutions():
    marks = parse_marks_file(TRAINING_STUDENTS, TRAINING_COURSE['marks_file'])
    reference_solutions = []

    for i, student in enumerate(TRAINING_STUDENTS):
        if marks[i] == 100:
            reference_solutions.append({
                'label': student,
                'root': '{}/{}/{}'.format(ROOT_DIR, TRAINING_DIR, student),
                'src': '{}/{}/{}/src'.format(ROOT_DIR, TRAINING_DIR, student),
            })

    return reference_solutions


# A subset of training solutions that only received 100
REFERENCE_SOLUTIONS = get_reference_solutions()
