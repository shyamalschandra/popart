#!/bin/bash -e

set -e

if [ ! -f "view.txt" ]
then
  echo "Run 'bash poponnx/ci/test.sh' from the poponnx_view directory."
  exit 1
fi

source ./poponnx/ci/utils.sh

if [ $# -gt 0 ]
then
  PYBIN=$1
else
  PYBIN=python3
fi

# Use the virtualenv for building
VE="${PWD}/../external/poponnx_build_python_${PYBIN}"
source ${VE}/bin/activate

cd build/build/poponnx
make package_and_move

echo "Done"
