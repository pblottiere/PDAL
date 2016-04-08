#!/bin/bash

builddir=$1
destdir=$2
DATE=$(shell date +'%y.%m.%d %H:%M:%S')

git clone git@github.com:PDAL/pdal.github.io.git $destdir/pdaldocs
cd $destdir/pdaldocs
git checkout gh-pages


cd $builddir/html
cp -rf * $destdir/proj4docs

cd $destdir/proj4docs
git config user.email "pdal@hobu.net"
git config user.name "PDAL Travis docsbot"

git add -A
git commit -m "update with results of commit https://github.com/PDAL/PDAL/commit/$TRAVIS_COMMIT for $(DATE)"
git push origin master

