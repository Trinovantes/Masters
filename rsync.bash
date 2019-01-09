#!/usr/bin/env bash

RSYNC_OPTIONS='-avrzm --delete'

rsync $RSYNC_OPTIONS             \
    --include='*/'               \
    --include='*.tex'            \
    --include='*.bib'            \
    --include='*.c'              \
    --include='*.png'            \
    --include='*.jpg'            \
    --include='Makefile'         \
    --include='.gitignore'       \
    --exclude='*'                \
    '../Thesis/'                 \
    'thesis/'

rsync $RSYNC_OPTIONS \
    --exclude='*.src/'                           \
    --exclude='tests/ece459-a1/w2017-solutions/' \
    --exclude='tests/ece459-a1/w2018-solutions/' \
    --include='*/'                               \
    \
    --include='*.cpp'                        \
    --include='*.c'                          \
    --include='*.h'                          \
    --include='*.py'                         \
    --include='CMakeLists.txt'               \
    --include='tests/ece459-a1/output.png'   \
    --include='Makefile'                     \
    --include='.gitignore'                   \
    --exclude='*'                            \
    '../ClangAutoMarker/'                    \
    'src/'
