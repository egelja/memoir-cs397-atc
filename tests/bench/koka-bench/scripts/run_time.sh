#!/bin/bash

CMD=${1} ;

OUT=runs.out ;

echo "" > runs.out ;

NUM_RUNS=10 ;
for ((i=1; i <= NUM_RUNS; i++))
do
    echo "Run ${i}" ;
    /usr/bin/time --verbose ${CMD} ;
    echo "" ;
done
