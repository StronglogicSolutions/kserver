#!/usr/bin/env bash

mode=$1

if [ ! -z "${mode}" ]; then
  mode=RELEASE
fi

while ! BUILD_TYPE="${mode}" ./build.sh
do
  echo "Building KServer.."
  sleep 1
done

echo "Build complete!"

