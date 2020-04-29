#!/usr/bin/env bash
BUILD_ENV=$1

if [[ -z "$BUILD_ENV" ]]; then
  BUILD_ENV="LOCAL"
fi

BUILD_ENV=${BUILD_ENV} cmake . && make -j8

