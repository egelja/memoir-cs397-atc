#!/bin/bash

for ITERS in 1000000 ;
do
		
		echo "Baseline O1" ;
		./baseline_O1 ${ITERS} ;

		echo "Baseline O2" ;
		./baseline_O2 ${ITERS} ;

		echo "Baseline O3" ;
		./baseline_O3 ${ITERS} ;


		
		echo "Object Inlining O1" ;
		./optimized_O1 ${ITERS} ;

		echo "Object Inlining O2" ;
		./optimized_O2 ${ITERS} ;

		echo "Object Inlining O3" ;
		./optimized_O3 ${ITERS} ;

    
    echo "Object Inlining with Reorder O1" ;
		./reorder_O1 ${ITERS} ;

		echo "Object Inlining with Reorder O2" ;
		./reorder_O2 ${ITERS} ;

		echo "Object Inlining with Reorder O3" ;
		./reorder_O3 ${ITERS} ;
done
