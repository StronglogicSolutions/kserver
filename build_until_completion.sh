#!/usr/bin/env bash

mode=$1
type=$2

if [ -z "${mode}" ]; then
  mode=CIRCLECI
fi
if [ -z "${type}" ]; then
  type=Release
fi

echo "Making ${type} build of KServer in ${mode}"

while ! ./build.sh "${mode}" "${type}"
do
  echo "Building KServer.."
  sleep 1
done

echo "Build complete!"


