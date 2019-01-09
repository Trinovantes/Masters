#!/usr/bin/env python3


import requests
import logging
import colorlog
import re
import os
import sys
import subprocess
import tempfile
from git import Repo


try:
    from private import GITLAB_TOKEN
except ImportError:
    sys.exit('Failed to import variable GITLAB_TOKEN from private.py. This file is not committed to git because it contains private API keys.')


logger = logging.getLogger('ClangAutoMarker')
logger.setLevel(logging.INFO)


#------------------------------------------------------------------------------


API_ENDPOINT = 'https://git.uwaterloo.ca/api/v3/projects' # Already deprecated API (current v5 uses GraphQL)
REPO_PATTERN = 'ece459\-(\d+)\-a1\-(\w+)'


#------------------------------------------------------------------------------


def clone_repos(term, output_dir):
    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp_dir = tmp_dir + '/'

        # Clone all student repos
        get_students(term, tmp_dir)

        # Copy over c files
        argv = ['rsync', '-rv', '--prune-empty-dirs', '--include', '*/', '--include', '*.c', '--include', 'Makefile', '--exclude', '*', tmp_dir, output_dir]
        logger.info(' '.join(argv))
        if subprocess.call(argv, ) != 0:
            logger.error('Failed to rsync')


def get_students(term, output_dir):
    students = set()
    current_page = 1
    total_pages = 1

    while (current_page <= total_pages):
        # Make API call
        r = requests.get(API_ENDPOINT, headers={
            'Private-Token': GITLAB_TOKEN,
        }, params={
            'page': current_page,
            'per_page': 100,
        })
        assert r.status_code == 200

        # Update params
        current_page += 1
        if total_pages < int(r.headers['X-Total-Pages']):
            total_pages = int(r.headers['X-Total-Pages'])
            logger.warn('Updated total_pages to {}'.format(total_pages))

        # Parse students
        repos = r.json()
        for repo in repos:
            match = re.search(REPO_PATTERN, repo['path'])
            if match is None:
                continue

            if match.group(1) != term:
                continue

            student = match.group(2)
            dest = '{}/{}'.format(output_dir, student)

            if os.path.exists(dest):
                continue

            logger.info("Cloning {}".format(repo['ssh_url_to_repo']))
            Repo.clone_from(repo['ssh_url_to_repo'], dest)


def main():
    clone_repos('1181', 'ece459-a1-w2018')


if __name__ == '__main__':
    ch = colorlog.StreamHandler()
    ch.setFormatter(colorlog.ColoredFormatter('%(log_color)s[%(levelname)s] %(message)s'))
    logger.addHandler(ch)

    main()
