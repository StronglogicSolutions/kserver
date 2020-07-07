#!/usr/bin/env bash
BUILD_ENV=$1
# BUILD_TYPE=$2
if [[ -z "$BUILD_ENV" ]]; then
  BUILD_ENV="LOCAL"
fi

if [[ -z "$BUILD_TYPE" ]]; then
  BUILD_TYPE="RELEASE"
fi

BUILD_ENV=${BUILD_ENV} cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} . && make -j8

