#!/usr/bin/env bash

# This script builds system python udfs.
# It is a separate script because it depends on environment(local pythons).

# Required environment variables:
# YTSAURUS_SOURCE_PATH - path to the ytsaurus source root.
# YQL_BUILD_PATH - path to the build directory. All artifacts will be placed here.
# BUILD_FLAGS - flags to pass to ya make when building.

set -eux
shopt -s expand_aliases

export YTSAURUS_SOURCE_PATH=$(realpath $YTSAURUS_SOURCE_PATH)
export YQL_BUILD_PATH=$(realpath $YQL_BUILD_PATH)

mkdir -p $YQL_BUILD_PATH

backends=()

for version in 3.8 \
               3.9 \
               3.10 \
               3.11 \
               3.12 \
               3.13 \
               3.14
do
  udf_suffix=$(echo "$version" | tr . _)
  udf_name="python${udf_suffix}"
  python_name="python${version}"
  if ! command -v "${python_name}-config" >/dev/null; then
    echo "Skipping SystemPython ${version}: ${python_name}-config is not installed"
    continue
  fi

  # USE_LOCAL_PYTHON leads to a failure due to a bug in ya.
  # /usr/include/pythonX/pyconfig.h cannot include ARCH/pythonX/pyconfig.h without -I/usr/include
  # Adding symlink /usr/include/pythonX/ARCH/pythonX -> /usr/include/ARCH/pythonX

  INCLUDES=$(${python_name}-config --includes | sed -n 's/^-I\(\S\+\) .*/\1/p')
  ARCH=$(dpkg-architecture -q DEB_BUILD_MULTIARCH)
  if [ ! -e ${INCLUDES}/${ARCH} ]; then
    mkdir -p ${INCLUDES}/${ARCH}
    ln -s $(dirname ${INCLUDES})/${ARCH}/$(basename ${INCLUDES}) ${INCLUDES}/${ARCH};
  fi

  python3 $YTSAURUS_SOURCE_PATH/ya make $YTSAURUS_SOURCE_PATH/yql/essentials/udfs/common/python/system_python/${udf_name} \
    -T ${BUILD_FLAGS} --ignore-recurses -DSTRIP=yes -DUSE_ARCADIA_PYTHON=no -DUSE_LOCAL_PYTHON -DPYTHON_CONFIG=${python_name}-config -DPYTHON_BIN=${python_name} --output $YQL_BUILD_PATH
  if [[ "$BUILD_FLAGS" != *"--bazel-remote-put"* ]]; then
    strip --remove-section=.gnu_debuglink $YQL_BUILD_PATH/yql/essentials/udfs/common/python/system_python/${udf_name}/libsystem${udf_name}_udf.so
  fi
  backends+=(--backend "$version" "$YQL_BUILD_PATH/yql/essentials/udfs/common/python/system_python/${udf_name}/libsystem${udf_name}_udf.so")
done

# Build the version-independent dispatcher in system Python mode, then bundle
# the available backends. Package after stripping: strip discards the appended data.
dispatcher_source="$YTSAURUS_SOURCE_PATH/yql/essentials/udfs/common/python/system_python/python3"
python3 "$YTSAURUS_SOURCE_PATH/ya" make "$dispatcher_source" \
  -T ${BUILD_FLAGS} --ignore-recurses -DSTRIP=yes \
  -DUSE_ARCADIA_PYTHON=no -DUSE_LOCAL_PYTHON \
  -DPYTHON_CONFIG=python3-config -DPYTHON_BIN=python3 \
  --output "$YQL_BUILD_PATH"
dispatcher="$YQL_BUILD_PATH/yql/essentials/udfs/common/python/system_python/python3/libsystempython3_udf.so"
if [[ "$BUILD_FLAGS" != *"--bazel-remote-put"* ]]; then
  strip --remove-section=.gnu_debuglink "$dispatcher"
fi
mv "$dispatcher" "$dispatcher.unpacked"
python3 "$dispatcher_source/pack.py" --dispatcher "$dispatcher.unpacked" \
  "${backends[@]}" --output "$dispatcher"
