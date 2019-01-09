#!/usr/bin/env python3


import os
import logging
import colorlog
import re
import numpy as np
from scipy import stats

from util import parse_output_file, parse_marks_file, parse_execute_file
import constants

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

from sklearn.cluster import KMeans
from sklearn.mixture import GaussianMixture
from sklearn import decomposition
from sklearn import datasets
from sklearn import preprocessing
from sklearn import metrics
import hdbscan


logger = logging.getLogger('ClangAutoMarker')
logger.setLevel(logging.INFO)


#------------------------------------------------------------------------------


# Graphing
plt.style.use('seaborn')
CMAP = 'tab10'
MARKER_SIZE = 40

# KM
NUM_COMPONENTS = 6
NUM_CLUSTERS = 4

# GM
NUM_SAMPLES = 100 # How round the contours are
LEVELS = 10 # How many shades to use

# HDBSCAN
CLUSTER_SIZE = 10


#------------------------------------------------------------------------------
# Edit Distances
#------------------------------------------------------------------------------


def get_edit_distances(src_file_name, student_ids, edit_distances_dir):
    num_students = len(student_ids) # Samples
    num_solutions = len(constants.REFERENCE_SOLUTIONS) # Features
    student_edit_distances = np.zeros((num_students, num_solutions))
    invalid_students_idx = []
    invalid_students = []

    # Collect marks from each solution output
    for solution_idx, solution in enumerate(constants.REFERENCE_SOLUTIONS):
        solution_label = solution['label']
        data_file = '{}/{}.stdout'.format(edit_distances_dir, solution_label)

        # Collect raw edit distances
        edit_distances = parse_output_file(student_ids, data_file, src_file_name)

        for student_idx, student in enumerate(student_ids):
            student_edit_distances[student_idx][solution_idx] = edit_distances[student_idx]

    # Invalidate students who have 0 edit distances (cannot be processed by the frontend)
    for student_idx, student in enumerate(student_ids):
        if np.max(student_edit_distances[student_idx]) < constants.MIN_EDIT_DISTANCE:
            invalid_students_idx.append(student_idx)
            invalid_students.append(student)

    for offset, idx in enumerate(invalid_students_idx):
        student_edit_distances = np.delete(student_edit_distances, idx - offset, 0)

    # Normalize remaining edit distances
    normalized_edit_distances = preprocessing.MaxAbsScaler().fit_transform(student_edit_distances)

    return normalized_edit_distances, student_edit_distances, invalid_students


def preprocess_training_test_data(src_file_name, training_course, test_course):
    training_edit_distances, _, invalid_training_students = get_edit_distances(src_file_name, training_course['students'], training_course['distance_dir'])
    test_edit_distances, _, invalid_test_students = get_edit_distances(src_file_name, test_course['students'], test_course['distance_dir'])

    # Dimension reduction
    pca = decomposition.PCA()
    pca.fit(training_edit_distances)

    training_data = pca.transform(training_edit_distances)
    training_data = training_data[:,:NUM_COMPONENTS]

    test_data = pca.transform(test_edit_distances)
    test_data = test_data[:,:NUM_COMPONENTS]

    return training_data, invalid_training_students, test_data, invalid_test_students


#------------------------------------------------------------------------------
# Helpers
#------------------------------------------------------------------------------


def bound_mark(mark):
    return min(100, max(mark, 0))


def compare_cluster_actual_marks(clusters, num_clusters, invalid_test_students, test_course):
    actual_marks = parse_marks_file(test_course['students'], test_course['marks_file'])

    cluster_actual_marks = []
    for i in range(num_clusters):
        cluster_actual_marks.append([])

    offset = 0
    for i, student in enumerate(test_course['students']):
        if student in invalid_test_students:
            offset += 1
            continue
        else:
            i = i - offset

        curr_cluster = clusters[i]
        cluster_actual_marks[curr_cluster].append(actual_marks[i])

    print('Clusters: {}'.format(num_clusters))
    for i in range(num_clusters):
        c = cluster_actual_marks[i]
        if len(c) > 0:
            print('n:{:3} mean:{:.2f} std:{:.2f} median:{:.2f} min:{} max:{}'.format(len(c), np.mean(c), np.std(c), np.median(c), np.min(c), np.max(c)))
        else:
            print('n:{:3}'.format(len(c)))

    return cluster_actual_marks


def visualize_marks(predicted_marks, title, output_file):
    plt.figure()
    plt.title(title)
    plt.xlabel('Mark')
    plt.ylabel('Number of Students')
    plt.xlim((0, 100))
    plt.hist(predicted_marks, [x * 5 for x in range(21)])

    plt.tight_layout()
    plt.savefig(output_file)
    plt.close()


def visualize_marks_with_actual(predicted_marks, title, output_file, invalid_test_students, test_course):
    actual_marks = parse_marks_file(test_course['students'], test_course['marks_file'])

    plt.figure()
    plt.title(title)
    plt.xlabel('Actual Mark')
    plt.ylabel('Predicted Mark')
    plt.xlim(0, 110)
    plt.ylim(0, 110)

    offset = 0
    for i, student in enumerate(test_course['students']):
        if student in invalid_test_students:
            offset += 1
            continue
        else:
            i = i - offset

        x = int(actual_marks[i])
        y = int(predicted_marks[i])
        plt.scatter(x, y, s=MARKER_SIZE, c='k')

    plt.tight_layout()
    plt.savefig(output_file)
    plt.close()


#------------------------------------------------------------------------------
# Visualize Parameters
#------------------------------------------------------------------------------


def visualize_elbow(src_file_name):
    logger.info('visualize_elbow: {}'.format(src_file_name))
    training_data, _, invalid_training_students = get_edit_distances(src_file_name, constants.TRAINING_STUDENTS, constants.TRAINING_DIR)

    ssd = {}
    for num_clusters in range(1, 10):
        km = KMeans(n_clusters=num_clusters)
        km.fit_predict(training_data)
        ssd[num_clusters] = km.inertia_

    plt.figure()
    plt.title('Using Elbow Method to find Optimal Number of Clusters')
    plt.xlabel('Number of Clusters')
    plt.ylabel('Sum of Squared Distances')
    plt.plot(list(ssd.keys()), list(ssd.values()))

    plt.tight_layout()
    plt.savefig('output/elbow.png')
    plt.close()


def visualize_explained_variance(src_file_name):
    logger.info('visualize_explained_variance: {}'.format(src_file_name))
    training_data, _, invalid_training_students = get_edit_distances(src_file_name, constants.TRAINING_STUDENTS, constants.TRAINING_DIR)

    # Dimension reduction
    pca = decomposition.PCA()
    pca.fit(training_data)
    cum_sum_explained_var = np.cumsum(pca.explained_variance_ratio_)

    plt.figure()
    plt.plot(cum_sum_explained_var)
    plt.title('Explained Variance Ratio')
    plt.xlim(0, 25)
    plt.ylim(0.75, 1)
    plt.xlabel('Number of Components')
    plt.ylabel('Cumulative Explained Variance')

    plt.tight_layout()
    plt.savefig('output/explained_variance_ratio.png')
    plt.close()


#------------------------------------------------------------------------------
# K-Means
#------------------------------------------------------------------------------


def run_clustering_km(src_file_name, training_data, invalid_training_students, training_course, test_data, invalid_test_students, test_course):
    km = KMeans(n_clusters=NUM_CLUSTERS)
    km.fit(training_data)
    km_results = km.predict(test_data)
    km_centers = km.cluster_centers_

    min_x = np.min(training_data[:,0])
    max_x = np.max(training_data[:,0])
    min_y = np.min(training_data[:,1])
    max_y = np.max(training_data[:,1])

    # Need to make x,y scales the same for the visual distance relative to the
    # marker from km_centers to make sense
    l = min(min_x, min_y)
    h = max(max_x, max_y)

    plt.figure()
    plt.xlim((l, h))
    plt.ylim((l, h))
    plt.tick_params(axis='both', which='both', bottom=False, top=False, labelbottom=False, right=False, left=False, labelleft=False)

    for size in range(3):
        # Show ranges from center
        plt.scatter(km_centers[:,0], km_centers[:,1], s=(2000 + (size * 5000)), color='grey', alpha=0.3, edgecolors=None)

    highlight_full_marks = True
    if highlight_full_marks:
        actual_marks = parse_marks_file(test_course['students'], test_course['marks_file'])
        colors = [
            ['#cab2d6', '#6a3d9a'],
            ['#b2df8a', '#33a02c'],
            ['#fdbf6f', '#ff7f00'],
            ['#a6cee3', '#1f78b4'],
        ]
        offset = 0
        for i, student in enumerate(test_course['students']):
            if student in invalid_test_students:
                offset += 1
                continue
            else:
                i = i - offset

            if actual_marks[i] == 100:
                color = colors[km_results[i]][1]
            else:
                color = colors[km_results[i]][0]

            plt.scatter(test_data[i, 0], test_data[i, 1], s=MARKER_SIZE / 2, c=color)
            # plt.text(test_data[i, 0], test_data[i, 1], student, fontsize=9)
    else:
        plt.scatter(test_data[:,0], test_data[:,1], c=km_results, s=MARKER_SIZE, cmap=CMAP)

    plt.tight_layout()
    plt.savefig('output/marking_{}_{}_km.png'.format(src_file_name, test_course['label']))
    plt.close()

    #************************
    # Generate Marks
    #************************

    dist_from_cluster_center = []
    offset = 0
    for i, student in enumerate(test_course['students']):
        if student in invalid_test_students:
            offset += 1
            continue
        else:
            i = i - offset

        cluster_id = km_results[i]
        cluster_pt = km_centers[cluster_id]
        student_pt = test_data[i]

        dist = np.linalg.norm(cluster_pt - student_pt)
        dist_from_cluster_center.append(dist)

    mean = np.mean(dist_from_cluster_center)
    std = np.std(dist_from_cluster_center)
    median = np.median(dist_from_cluster_center)
    final_marks = []

    offset = 0
    for i, student in enumerate(test_course['students']):
        if student in invalid_test_students:
            offset += 1
            continue
        else:
            i = i - offset

        d = dist_from_cluster_center[i]
        m = 100 - (abs(d - mean) / std) * 20
        final_marks.append(bound_mark(m))

    title = 'Predicted Marks with K-Means'
    compare_cluster_actual_marks(km_results, NUM_CLUSTERS, invalid_test_students, test_course)
    visualize_marks(final_marks, title, 'output/marking_{}_{}_km_hist.png'.format(src_file_name, test_course['label']))
    visualize_marks_with_actual(final_marks, title, 'output/marking_{}_{}_km_scatter.png'.format(src_file_name, test_course['label']), invalid_test_students, test_course)
    return final_marks


#------------------------------------------------------------------------------
# Gaussian Mixture
#------------------------------------------------------------------------------


def run_clustering_gm(src_file_name, training_data, invalid_training_students, training_course, test_data, invalid_test_students, test_course):
    gm = GaussianMixture(n_components=NUM_CLUSTERS)
    gm.fit(training_data)
    gm_results = gm.predict(test_data)

    min_x = np.min(training_data[:,0])
    max_x = np.max(training_data[:,0])
    min_y = np.min(training_data[:,1])
    max_y = np.max(training_data[:,1])

    x = np.linspace(min_x, max_x, NUM_SAMPLES)
    y = np.linspace(min_y, max_y, NUM_SAMPLES)
    x, y = np.meshgrid(x, y)
    x_flat = x.ravel()
    y_flat = y.ravel()

    samples = []
    for i in range(len(x_flat)):
        sample = np.zeros(NUM_COMPONENTS)
        sample[0] = x_flat[i]
        sample[1] = y_flat[i]
        samples.append(sample)

    z = gm.score_samples(samples)
    z = z.reshape((NUM_SAMPLES, NUM_SAMPLES))

    # Visualize the clusters
    plt.figure()
    plt.xlim((min_x, max_x))
    plt.ylim((min_y, max_y))
    plt.tick_params(axis='both', which='both', bottom=False, top=False, labelbottom=False, right=False, left=False, labelleft=False)
    plt.contourf(x, y, z, LEVELS, cmap='Blues_r')
    plt.scatter(test_data[:,0], test_data[:,1], c=gm_results, s=MARKER_SIZE, cmap=CMAP)

    plt.tight_layout()
    plt.savefig('output/marking_{}_{}_gm.png'.format(src_file_name, test_course['label']))
    plt.close()

    #***************
    # Generate Marks
    #***************

    final_marks = []
    scores = gm.predict_proba(test_data)

    offset = 0
    for i, student in enumerate(test_course['students']):
        if student in invalid_test_students:
            offset += 1
            continue
        else:
            i = i - offset

        m = scores[i][gm_results[i]] * 100
        final_marks.append(bound_mark(m))

    title = 'Predicted Marks with Gaussian Mixture'
    compare_cluster_actual_marks(gm_results, NUM_CLUSTERS, invalid_test_students, test_course)
    visualize_marks(final_marks, title, 'output/marking_{}_{}_gm_hist.png'.format(src_file_name, test_course['label']))
    visualize_marks_with_actual(final_marks, title, 'output/marking_{}_{}_gm_scatter.png'.format(src_file_name, test_course['label']), invalid_test_students, test_course)
    return final_marks


#------------------------------------------------------------------------------
# HDBSCAN
#------------------------------------------------------------------------------


def run_clustering_hdb(src_file_name, training_data, invalid_training_students, training_course, test_data, invalid_test_students, test_course):
    min_x = np.min(training_data[:,0])
    max_x = np.max(training_data[:,0])
    min_y = np.min(training_data[:,1])
    max_y = np.max(training_data[:,1])

    plt.figure()

    curr_cluster_size = 5
    curr_plt_idx = 1

    # Visualize varying the min_curr_cluster_size parameter
    while curr_cluster_size <= 20:
        clusterer = hdbscan.HDBSCAN(min_cluster_size=curr_cluster_size, prediction_data=True).fit(training_data)
        hdb_results, strengths = hdbscan.approximate_predict(clusterer, test_data)

        ax = plt.subplot(2, 2, curr_plt_idx)
        ax.set_title('Min Cluster Size: {}'.format(curr_cluster_size))
        ax.set_xlim((min_x, max_x))
        ax.set_ylim((min_y, max_y))
        ax.tick_params(axis='both', which='both', bottom=False, top=False, labelbottom=False, right=False, left=False, labelleft=False)
        ax.scatter(test_data[:,0], test_data[:,1], c=hdb_results, s=(MARKER_SIZE / 2), cmap=CMAP)

        curr_cluster_size += 5
        curr_plt_idx += 1

    plt.tight_layout(rect=[0, 0, 1, 0.95]) # [left, bottom, right, top] in normalized (0, 1) figure coordinates
    plt.savefig('output/marking_{}_{}_hdb.png'.format(src_file_name, test_course['label']))
    plt.close()

    #***************
    # Generate Marks
    #***************

    clusterer = hdbscan.HDBSCAN(min_cluster_size=CLUSTER_SIZE, prediction_data=True).fit(training_data)
    hdb_results, strengths = hdbscan.approximate_predict(clusterer, test_data)
    num_clusters = clusterer.labels_.max() + 2 # +1 due to 0-based indexing, +1 due to "-1" label for outliers

    final_marks = []
    offset = 0
    for i, student in enumerate(test_course['students']):
        if student in invalid_test_students:
            offset += 1
            continue
        else:
            i = i - offset

        m = strengths[i] * 100
        final_marks.append(bound_mark(m))

    title = 'Predicted Marks with HDBSCAN'
    compare_cluster_actual_marks(hdb_results, num_clusters, invalid_test_students, test_course)
    visualize_marks(final_marks, title, 'output/marking_{}_{}_hdb_hist.png'.format(src_file_name, test_course['label']))
    visualize_marks_with_actual(final_marks, title, 'output/marking_{}_{}_hdb_scatter.png'.format(src_file_name, test_course['label']), invalid_test_students, test_course)
    return final_marks


#------------------------------------------------------------------------------
# Normalized Min Dist
#------------------------------------------------------------------------------


def run_min_normalized_dist(src_file_name, training_data, invalid_training_students, training_course, test_data, invalid_test_students, test_course):
    test_edit_distances, _, invalid_test_students = get_edit_distances(src_file_name, test_course['students'], test_course['distance_dir'])
    final_marks = []

    offset = 0
    for i, student in enumerate(test_course['students']):
        if student in invalid_test_students:
            offset += 1
            continue
        else:
            i = i - offset

        m = (1 - np.min(test_edit_distances[i])) * 100
        final_marks.append(bound_mark(m))

    title = 'Predicted Marks with Minimum Normalized Edit Distance'
    visualize_marks(final_marks, title, 'output/marking_{}_{}_min_dist_hist.png'.format(src_file_name, test_course['label']))
    visualize_marks_with_actual(final_marks, title, 'output/marking_{}_{}_min_dist_scatter.png'.format(src_file_name, test_course['label']), invalid_test_students, test_course)
    return final_marks


#------------------------------------------------------------------------------
# Random
#------------------------------------------------------------------------------


def run_random(src_file_name, training_data, invalid_training_students, training_course, test_data, invalid_test_students, test_course):
    np.random.seed(0)

    m = parse_marks_file(training_course['students'], training_course['marks_file'])
    m = [x for x in m if x > 0]
    mean = np.mean(m)
    std = np.std(m)

    final_marks = []
    for i, student in enumerate(test_course['students']):
        m = np.random.normal(mean, std)
        final_marks.append(bound_mark(m))

    title = 'Predicted Marks with Uniform Random $\mu={:.2f}$ $\sigma={:.2f}$'.format(mean, std)
    visualize_marks(final_marks, title, 'output/marking_{}_{}_rng_hist.png'.format(src_file_name, test_course['label']))
    visualize_marks_with_actual(final_marks, title, 'output/marking_{}_{}_rng_scatter.png'.format(src_file_name, test_course['label']), invalid_test_students, test_course)
    return final_marks


#------------------------------------------------------------------------------
# Always Full Marks
#------------------------------------------------------------------------------


def run_always_full_marks(src_file_name, training_data, invalid_training_students, training_course, test_data, invalid_test_students, test_course):
    n = len(test_course['students'])
    final_marks = np.full(n, 100)
    title = 'Always Full Marks'

    visualize_marks(final_marks, title, 'output/marking_{}_{}_always_full_hist.png'.format(src_file_name, test_course['label']))
    visualize_marks_with_actual(final_marks, title, 'output/marking_{}_{}_always_full_scatter.png'.format(src_file_name, test_course['label']), invalid_test_students, test_course)
    return final_marks


#------------------------------------------------------------------------------
# Mark
#------------------------------------------------------------------------------


def is_false_positive(predicted_mark, actual_mark, cutoff, execution_errors):
    if predicted_mark >= cutoff:
        final_mark = 100
    else:
        final_mark = predicted_mark

    # False positive if student receives more mark than they earned
    if final_mark - actual_mark >= constants.FALSE_POSITIVE_THRESHOLD:
        # Not a false positive if actual_mark < epsilon (i.e. really low mark due to missing submission or other issue)
        if actual_mark < constants.FALSE_POSITIVE_THRESHOLD:
            return False

        # Not a false positive if we can detect error via automated execution
        for error in execution_errors:
            if bool(error.strip()):
                return False

        return True

    return False


def run_marking_algorithms():
    training_course = constants.COURSE_DATA['ece459-a1-w2017']

    cutoffs = [90, 95]
    markers = [
        {
            'label' : 'Always Full Marks',
            'file'  : 'full-marks',
            'runner': run_always_full_marks,
        },
        {
            'label' : 'Minimum Distance',
            'file'  : 'min-dist',
            'runner': run_min_normalized_dist,
        },
        {
            'label' : 'K-Means',
            'file'  : 'km',
            'runner': run_clustering_km,
        },
        {
            'label' : 'Gaussian Mixture',
            'file'  : 'gm',
            'runner': run_clustering_gm,
        },
        {
            'label' : 'HDBSCAN',
            'file'  : 'hdb',
            'runner': run_clustering_hdb,
        },
        {
            'label' : 'Uniform Random',
            'file'  : 'rng',
            'runner': run_random,
        },
    ]
    assignments = [
        {
            'label': 'Non-Blocking IO Assignment',
            'key': 'paster_nbio',
        },
        {
            'label': 'Parallel Processing Assignment',
            'key': 'paster_parallel',
        },
    ]
    class_labels = [
        '2017',
        '2018',
    ]
    stat_labels = [
        {
            'label': '\\tspace Total Students',
            'key': 'total',
        },
        {
            'label': '\\tspace Non Automatable',
            'key': 'manual',
        },
        {
            'label': '\\tspace \\tspace Below Cutoff',
            'key': 'uncertain',
        },
        {
            'label': '\\tspace \\tspace Processing Error',
            'key': 'invalid',
        },
        {
            'label': '\\tspace Automatable',
            'key': 'automated',
        },
        {
            'label': '\\tspace \\tspace False Positives',
            'key': 'false_positives',
        },
        {
            'label': '\\tspace \\tspace Mean Sq Error',
            'key': 'mse',
        },
    ]
    max_label_len = np.max([len(x['label']) for x in stat_labels]) + 1

    stats = [
        # One for each marker
        # [
        #     # One for each assignment
        #     [
        #         # One for each class
        #         [
        #             # One for each cutoff
        #             {
        #                 'predicted_marks': [],
        #                 'actual_marks': [],
        #                 'mse' : int,
        #                 'total' : int,
        #                 'manual': int,
        #                     'invalid': int,
        #                     'uncertain': int,
        #                 'automated': int,
        #                     'false_positives': int,
        #             }
        #         ]
        #     ]
        # ]
    ]

    final_marks = {
        assn['key']:{
            k:{
                marker['label']:None
                for marker in markers
            }
            for k in constants.COURSE_DATA.keys()
        }
        for assn in assignments
    }
    final_invalids = {
        assn['key']:{
            k:None
            for k in constants.COURSE_DATA.keys()
        }
        for assn in assignments
    }

    for marker in markers:
        curr_marker_stats = []
        stats.append(curr_marker_stats)

        for assignment in assignments:
            curr_assn_stats = []
            curr_marker_stats.append(curr_assn_stats)

            src_file_name = assignment['key']
            for test_course_label in sorted(constants.COURSE_DATA.keys()):
                curr_class_stats = []
                curr_assn_stats.append(curr_class_stats)
                test_course = constants.COURSE_DATA[test_course_label]

                # Get data
                idx_map = dict((student, i) for i, student in enumerate(test_course['students']))
                training_data, invalid_training_students, test_data, invalid_test_students = preprocess_training_test_data(src_file_name, training_course, test_course)

                # Compute mark for this marker/assignment/class combination
                predicted_marks = marker['runner'](src_file_name, training_data, invalid_training_students, training_course, test_data, invalid_test_students, test_course)
                marks_actual = parse_marks_file(test_course['students'], test_course['marks_file'])
                execution_errors = parse_execute_file(test_course['students'], test_course['execute_files'])

                final_marks[src_file_name][test_course_label]['actual'] = marks_actual
                final_marks[src_file_name][test_course_label][marker['label']] = predicted_marks
                final_invalids[src_file_name][test_course_label] = invalid_test_students

                # Collect statistics
                for c, cutoff in enumerate(cutoffs):
                    curr_cutoff_stats = {
                        'predicted_marks': [],
                        'actual_marks': [],
                        'mse' : 0,
                        'total' : len(test_course['students']),
                        'manual': len(invalid_test_students),
                            'invalid': len(invalid_test_students),
                            'uncertain': 0,
                        'automated': 0,
                            'false_positives': 0,
                    }
                    curr_class_stats.append(curr_cutoff_stats)

                    offset = 0
                    for student in test_course['students']:
                        if student in invalid_test_students:
                            offset += 1
                            continue
                        else:
                            i = idx_map[student] - offset

                        predicted_mark = predicted_marks[i]

                        if predicted_mark >= cutoff:
                            predicted_mark = 100 # Higher than cutoffs get full marks
                            curr_cutoff_stats['predicted_marks'].append(predicted_mark)
                            curr_cutoff_stats['actual_marks'].append(marks_actual[i])

                            curr_cutoff_stats['automated'] += 1
                            if is_false_positive(predicted_mark, marks_actual[i], cutoff, execution_errors[i]):
                                curr_cutoff_stats['false_positives'] += 1
                        else:
                            curr_cutoff_stats['manual'] += 1
                            curr_cutoff_stats['uncertain'] += 1

                    if curr_cutoff_stats['automated'] == 0:
                        curr_cutoff_stats['mse'] = 0
                    else:
                        curr_cutoff_stats['mse'] = int(metrics.mean_squared_error(curr_cutoff_stats['actual_marks'], curr_cutoff_stats['predicted_marks']))

    # Hardcode value for always full marks
    full_marks_idx = 0
    for assn_idx, assignment in enumerate(assignments):
        for class_idx, class_label in enumerate(class_labels):
            for cutoff_idx, cutoff in enumerate(cutoffs):
                stats[full_marks_idx][assn_idx][class_idx][cutoff_idx]['automated'] += stats[full_marks_idx][assn_idx][class_idx][cutoff_idx]['manual']
                stats[full_marks_idx][assn_idx][class_idx][cutoff_idx]['manual'] = 0
                stats[full_marks_idx][assn_idx][class_idx][cutoff_idx]['invalid'] = 0

    # Print latex
    for marker_idx, marker_stats in enumerate(stats):
        marker = markers[marker_idx]
        with open('output/table_{}.tex'.format(marker['file']), 'w') as f:
            def w(line):
                f.write(line + '\n')

            w("\\begin{tabular}{lrrrr} \\toprule")
            w("& \multicolumn{2}{c}{2017 Class} & \multicolumn{2}{c}{2018 Class} \\\\")
            w("& 90 Cutoff & 95 Cutoff & 90 Cutoff & 95 Cutoff \\\\")
            w("\midrule")

            first = True
            for i, assn_stats in enumerate(marker_stats):
                if first:
                    first = False
                else:
                    f.write('\\\\\n')

                f.write('\multicolumn{{5}}{{l}}{{ \\textbf{{{}}} }}\\\\\n'.format(assignments[i]['label']))
                for stat_label in stat_labels:
                    # Do not print this statistic in latex table
                    if stat_label['key'] == 'mse':
                        continue

                    f.write('{:<{max_label_len}s}'.format(stat_label['label'], max_label_len=max_label_len))
                    for class_stats in assn_stats:
                        for cutoff_stats in class_stats:
                            f.write('& {:>5d} '.format(cutoff_stats[stat_label['key']]))
                    f.write('\\\\\n')

            w("\\bottomrule")
            w("\\end{tabular}")


    def visualize_bar(numerator_key, denominator_key, y_label, image_key, image_label):
        for assn_idx, assignment in enumerate(assignments):
            rates = []

            for class_idx, class_label in enumerate(class_labels):
                for cutoff_idx, cutoff in enumerate(cutoffs):
                    curr_rate = {
                        'label': '{} Cutoff {}'.format(cutoff, class_label),
                        'marker_rate': [],
                    }

                    rates.append(curr_rate)

                    for marker_idx, marker in enumerate(markers):
                        s = stats[marker_idx][assn_idx][class_idx][cutoff_idx]
                        numerator = s[numerator_key]

                        if denominator_key is None:
                            r = numerator
                        else:
                            denominator = s[denominator_key]
                            if denominator == 0:
                                r = 0
                            else:
                                r = int(numerator / denominator * 100)

                        curr_rate['marker_rate'].append(r)

            fig, ax = plt.subplots()
            ax_indices = np.arange(len(markers))
            rects = []
            rects_labels = []
            width = 0.2
            mid = len(rates) / 2
            colors = ['#e66101','#fdb863','#5e3c99','#b2abd2']

            for i, rate in enumerate(rates):
                rect = ax.bar(ax_indices + width * (i - mid + 1), rate['marker_rate'], width, color=colors[i])
                rects.append(rect)
                rects_labels.append(rate['label'])

            ax.set_ylabel(y_label)
            ax.set_xticks(ax_indices + width / len(rects))
            ax.set_xticklabels([marker['label'] for marker in markers])
            ax.set_title('{} for {}'.format(image_label, assignment['label']))
            ax.legend(rects, rects_labels)

            fig.set_figheight(4.2) # Inches
            plt.tight_layout()
            plt.savefig('output/bar_single_{}_{}'.format(image_key, assignment['key']))


    def visualize_stacked_double_bar(bottom_key, total_key, y_label, image_key, image_label):
        for assn_idx, assignment in enumerate(assignments):
            rates = []

            for class_idx, class_label in enumerate(class_labels):
                for cutoff_idx, cutoff in enumerate(cutoffs):
                    curr_rate = {
                        'label': '{} Cutoff {}'.format(cutoff, class_label),
                        'bottom_segment': [],
                        'top_segment': [],
                    }

                    rates.append(curr_rate)

                    for marker_idx, marker in enumerate(markers):
                        s = stats[marker_idx][assn_idx][class_idx][cutoff_idx]
                        bottom = s[bottom_key]
                        total = s[total_key]
                        curr_rate['bottom_segment'].append(bottom)
                        curr_rate['top_segment'].append(total - bottom)
                        

            fig, ax = plt.subplots()
            ax_indices = np.arange(len(markers))
            bottom_rects = []
            top_rects = []
            rects_labels = []
            width = 0.2
            mid = len(rates) / 2
            bottom_colors = ['#1F78B4', '#E31A1C', '#33A02C', '#FF7F00']
            top_colors =    ['#A6CEE3', '#FB9A99', '#B2DF8A', '#FDBF6F']

            for i, rate in enumerate(rates):
                bottom_rect = ax.bar(ax_indices + width * (i - mid + 1), rate['bottom_segment'], width, color=bottom_colors[i])
                top_rect = ax.bar(ax_indices + width * (i - mid + 1), rate['top_segment'], width, color=top_colors[i], bottom=rate['bottom_segment'])
                bottom_rects.append(bottom_rect)
                top_rects.append(top_rect)
                rects_labels.append(rate['label'])

            ax.set_ylabel(y_label)
            ax.set_xticks(ax_indices + width / len(rates))
            ax.set_xticklabels([marker['label'] for marker in markers])
            ax.set_title('{} for {}'.format(image_label, assignment['label']))
            ax.legend(bottom_rects, rects_labels)

            fig.set_figheight(4.2) # Inches
            plt.tight_layout()
            plt.savefig('output/bar_double_{}_{}'.format(image_key, assignment['key']))


    def visualize_stacked_triple_bar(bottom_key, top1_key, top2_key, y_label, image_label, height=4):
        for class_idx, class_label in enumerate(class_labels):
            rates = []

            for assn_idx, assignment in enumerate(assignments):
                for cutoff_idx, cutoff in enumerate(cutoffs):
                    curr_rate = {
                        'label': '{} Cutoff : {}'.format(cutoff, assignment['label']),
                        'set1_segment': [],
                        'set2_segment': [],
                        'set3_segment': [],
                    }

                    rates.append(curr_rate)

                    for marker_idx, marker in enumerate(markers):
                        s = stats[marker_idx][assn_idx][class_idx][cutoff_idx]
                        set1 = s[bottom_key]
                        set2 = s[top1_key]
                        set3 = s[top2_key]
                        curr_rate['set1_segment'].append(set1)
                        curr_rate['set2_segment'].append(set2 - set1)
                        curr_rate['set3_segment'].append(set3 - set2)

            fig, ax = plt.subplots()
            ax_indices = np.arange(len(markers))
            set1_rects = []
            set2_rects = []
            set3_rects = []
            rects_labels = []
            width = 0.2
            mid = len(rates) / 2
            set1_colors = ['#FF7F00', '#FF7F00', '#FF7F00', '#FF7F00']
            set2_colors = ['#A6CEE3', '#1F78B4', '#B2DF8A', '#33A02C']
            set3_colors = ['#95a5a6', '#95a5a6', '#95a5a6', '#95a5a6']

            for i, rate in enumerate(rates):
                set1_rect = ax.bar(ax_indices + width * (i - mid + 1), rate['set1_segment'], width, color=set1_colors[i])
                set2_rect = ax.bar(ax_indices + width * (i - mid + 1), rate['set2_segment'], width, color=set2_colors[i], bottom=rate['set1_segment'])
                set3_rect = ax.bar(ax_indices + width * (i - mid + 1), rate['set3_segment'], width, color=set3_colors[i], bottom=(np.array(rate['set1_segment']) + np.array(rate['set2_segment'])))
                set1_rects.append(set1_rect)
                set2_rects.append(set2_rect)
                set3_rects.append(set3_rect)

            ax.set_ylabel(y_label)
            ax.set_xticks(ax_indices + width / len(rates))
            ax.set_xticklabels([marker['label'] for marker in markers])
            ax.set_title('Experimental Results for Class of {}'.format(class_label))
            ax.legend(set2_rects, rects_labels)

            fig.set_figheight(height) # Inches
            plt.tight_layout()

            if height == 4:
                plt.savefig('output/bar_triple_{}_{}'.format(image_label, class_label))
            else:
                plt.savefig('output/bar_triple_{}_{}_{}'.format(image_label, class_label, height))


    visualize_bar('automated', 'total', 'Automation Rate (%)', 'automated', 'Automation Rate')
    visualize_bar('false_positives', None, 'Number of False Positives', 'false_positives', 'False Positives')
    visualize_stacked_double_bar('automated', 'total', 'Number of Assignments', 'automated', 'Automation Rate')
    visualize_stacked_double_bar('false_positives', 'automated', 'Number of Assignments', 'false_positives', 'False Positives')
    visualize_stacked_triple_bar('false_positives', 'automated', 'total', 'Number of Assignments', 'results')
    visualize_stacked_triple_bar('false_positives', 'automated', 'total', 'Number of Assignments', 'results', 3)

    # Print results
    with open('output/marks.csv', 'w') as f:
        marks_actual = parse_marks_file(test_course['students'], test_course['marks_file'])

        for assn_label, assn_marks in final_marks.items():
            for class_label, class_marks in assn_marks.items():
                f.write('{} {}\n'.format(assn_label, class_label))
                f.write('Student')
                f.write(', Actual')
                for marker in markers:
                    f.write(', {}'.format(marker['label']))
                f.write('\n')

                course = constants.COURSE_DATA[class_label]
                students = course['students']
                n = len(students)
                max_student_len = np.max([len(student) for student in students])
                invalid_test_students = final_invalids[assn_label][class_label]

                offset = 0
                for i, student in enumerate(students):
                    if student in invalid_test_students:
                        offset += 1
                        continue
                    else:
                        i = i - offset

                    f.write('{:<{max_student_len}}'.format(students[i], max_student_len=max_student_len))
                    f.write(', {:>4}'.format(int(class_marks['actual'][i])))
                    for marker in markers:
                        marker_marks = class_marks[marker['label']]
                        f.write(', {:>4}'.format(int(marker_marks[i])))
                    f.write('\n')
                f.write('\n')



#------------------------------------------------------------------------------
# Main
#------------------------------------------------------------------------------


def main():
    visualize_elbow('paster_nbio')
    visualize_explained_variance('paster_nbio')
    run_marking_algorithms()


if __name__ == '__main__':
    handler = colorlog.StreamHandler()
    handler.setFormatter(colorlog.ColoredFormatter('%(log_color)s[%(levelname)s] %(message)s'))
    logger.addHandler(handler)

    main()
