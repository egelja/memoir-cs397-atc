#!/bin/bash -e

if [ $# -lt 1 ] ; then
    echo "This script will run MemOIR optimizations and lower to LLVM IR." ;
    echo "  USAGE: `basename $0` <INPUT IR FILE> <OUTPUT IR FILE> [<OPTIONS, ...>]" ;
fi

GIT_ROOT=`git rev-parse --show-toplevel` ;
LIB_DIR=${GIT_ROOT}/install/lib ;

source ${GIT_ROOT}/enable ;

INPUT_IR_FILE="$1" ;
OUTPUT_IR_FILE="$2" ;

echo "Running MemOIR optimization pipeline (I: ${INPUT_IR_FILE}, O: ${OUTPUT_IR_FILE})" ;

# Normalize the bitcode
${GIT_ROOT}/compiler/scripts/normalize.sh ${INPUT_IR_FILE} ${OUTPUT_IR_FILE} ;

# Lower the bitcode
${GIT_ROOT}/compiler/scripts/lower.sh ${INPUT_IR_FILE} ${OUTPUT_IR_FILE} ;
