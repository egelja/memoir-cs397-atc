#!/bin/bash

if test $# -lt 1 ; then
  echo "USAGE: `basename $0` DESTINATION" ;
  exit 1;
fi
if test -e $1 ; then
  exit 1;
fi

git clone git@github.com:scampanoni/noelle.git $1 ;
cd $1 ; 

make ;
