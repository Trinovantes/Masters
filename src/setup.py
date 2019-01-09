#!/usr/bin/python3


import logging
import colorlog

from pathlib import Path
import tarfile
import subprocess
import os


#------------------------------------------------------------------------------


LLVM_VERSION = '5.0.1'
LLVM_URL = 'http://releases.llvm.org/{}/llvm-{}.src.tar.xz'.format(LLVM_VERSION, LLVM_VERSION)
LLVM_DIR = 'llvm-{}.src'.format(LLVM_VERSION)
LLVM_SUBPROJECTS = [
    [
        'http://releases.llvm.org/{}/cfe-{}.src.tar.xz'.format(LLVM_VERSION, LLVM_VERSION),
        '/tools/clang',
    ],
    [
        'http://releases.llvm.org/{}/libcxx-{}.src.tar.xz'.format(LLVM_VERSION, LLVM_VERSION),
        '/projects/libcxx',
    ],
    [
        'http://releases.llvm.org/{}/libcxxabi-{}.src.tar.xz'.format(LLVM_VERSION, LLVM_VERSION),
        '/projects/libcxxabi',
    ],
]

BUILD_DIR   = LLVM_DIR + '/build'
BUILD_CACHE = BUILD_DIR + '/CMakeCache.txt'

CLANG_DIR = LLVM_DIR + '/tools/clang'
CLANG_LIBTOOLING_DIR = CLANG_DIR + '/tools'
CLANG_LIBTOOLING_FILE = CLANG_LIBTOOLING_DIR + '/CMakeLists.txt'
CLANG_LIBTOOLING_TOOLS = [
    # 1st param: tool name
    # 2nd param: current directory relative to this script
    ['clang-automarker', 'AutoMarker']
]


#------------------------------------------------------------------------------


logger = logging.getLogger()
logger.setLevel(logging.DEBUG)

handler = colorlog.StreamHandler()
handler.setFormatter(colorlog.ColoredFormatter('%(log_color)s[%(levelname)s] %(message)s'))
logger.addHandler(handler)


#------------------------------------------------------------------------------


def download(url):
    logger.info('Downloading "{}"'.format(url))

    file_name = url.split('/')[-1]

    if Path(file_name).is_file():
        logger.warn('{} already exists; skipping download'.format(file_name))
        return file_name

    subprocess.call(['wget', url])
    return file_name


#------------------------------------------------------------------------------


def extract(src):
    logger.info('Extracting "{}"'.format(src))

    extracted_dir = src.split('.tar')[0]
    if Path(extracted_dir).is_dir():
        logger.warn('{} already exists; skipping extraction'.format(extracted_dir))
        return extracted_dir

    archive = tarfile.open(src)
    archive.extractall()

    return extracted_dir


#------------------------------------------------------------------------------


def link(src, dest):
    src = os.getcwd() + '/' + src
    logger.info('Linking "{}" to "{}"'.format(src, dest))

    if os.path.islink(dest):
        logger.warn('{} already exists; skipping symlink'.format(dest))
        return

    os.symlink(src, dest)


#------------------------------------------------------------------------------


def update_libtooling_cmake(tool_name):
    logger.info('Adding "{}" to "{}"'.format(tool_name, CLANG_LIBTOOLING_FILE))

    with open(CLANG_LIBTOOLING_FILE, 'r+') as cmake:
        contents = cmake.read()
        new_command = 'add_clang_subdirectory({})'.format(tool_name)
        if new_command in contents:
            logger.warn('{} already exists in {}; skipping this tool'.format(tool_name, CLANG_LIBTOOLING_FILE))
            return

        cmake.write(new_command + '\n')


#------------------------------------------------------------------------------


def main():
    llvm_tar = download(LLVM_URL)
    llvm_src = extract(llvm_tar)

    for project in LLVM_SUBPROJECTS:
        tar = download(project[0])
        src = extract(tar)
        link(src, LLVM_DIR + project[1])

    for tool in CLANG_LIBTOOLING_TOOLS:
        tool_dest = CLANG_LIBTOOLING_DIR + '/' + tool[0]
        tool_src = tool[1]
        link(tool_src, tool_dest)
        update_libtooling_cmake(tool[0])

    subprocess.call(['mkdir', '-p', BUILD_DIR])
    subprocess.call(['rm', '-f', BUILD_CACHE])
    subprocess.call(['cmake', '..'], cwd=BUILD_DIR)


#------------------------------------------------------------------------------


if __name__ == '__main__':
    main()

