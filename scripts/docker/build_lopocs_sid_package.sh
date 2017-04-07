#! /bin/sh

IMAGE=pdal-lopocs:sid
DEB=pdal-lopocs-0.1-sid.deb

# build image and pdal package for lopocs branch
docker build -f Dockerfile.lopocs.sid -t $IMAGE .

# get the package on host
docker run -d -v $PWD:/tmp $IMAGE cp /PDAL/build/$DEB /tmp

# clean
ID=$(docker ps -aq --filter ancestor=$IMAGE)
docker stop $ID
docker rm $ID
docker rmi $IMAGE
