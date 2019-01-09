# ClangAutoMarker

> This thesis presents a novel approach to automatically mark programming assignments. We hypothesize that correct student solution ASTs will be more similar to reference solution ASTs than incorrect student solutions and that their similarities can be quantitatively measured. Our approach first preprocesses the ASTs before computing their tree edit distances. We then aggregate the student's set of edit distances from every reference solution into a final mark for the student. We have implemented our approach in our ClangAutoMarker tool. Our experiments demonstrate promising potential for reducing a human marker's workload but further refinements are needed before its accuracy can be suitable for a live classroom.

**Note:** I cannot publish my test data (student code/grades) due to copyright/privacy issues.

## Compiling Code

We first need to download, extract, and symlink the appropriate LLVM/Clang source code. Go inside `code` directory and run:
```
./setup.py
```

**Warning:** Compling from scratch will take a few hours and require approximately 50GB of free space.

The next step compiles and runs the tool on dummy inputs. Go inside `code/tests` directory and run:
```
./run.py
```

## Compiling Thesis

Go inside `thesis` directory and run:
```
make
```
