#! /bin/sh

IMAGE=pdal-lopocs:jessie
DEB=pdal-lopocs-0.1-jessie.deb

# build image and pdal package for lopocs branch
docker build -f Dockerfile.lopocs.jessie -t $IMAGE .

# get the package on host
docker run -d -v $PWD:/tmp $IMAGE cp /PDAL/build/$DEB /tmp

# clean
ID=$(docker ps -aq --filter ancestor=$IMAGE)
docker stop $ID
docker rm $ID
docker rmi $IMAGE
