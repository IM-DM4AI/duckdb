# IMLane deploy

## Container create

```shell
# Pull images
docker pull apache/arrow-dev:amd64-ubuntu-20.04-r-4.4

# Run container 
# GPU
docker run --name imlane  --cap-add sys_ptrace --shm-size 40GB --gpus all  -it apache/arrow-dev:amd64-ubuntu-20.04-r-4.4 /bin/bash
# No GPU
docker run --name imlane  --cap-add sys_ptrace --shm-size 40GB -it apache/arrow-dev:amd64-ubuntu-20.04-r-4.4 /bin/bash

```

## Based lib install

```shell
# Update pip
python3 -m pip install --upgrade pip

mkdir /home/third_party

# Boost
cd /home/third_party

wget https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.gz

tar -zxvf boost-1.84.0.tar.gz

cd boost-1.84.0

./bootstrap.sh

./b2 install cxxflags="-std=c++17"

# Arrow
cd /home/third_party

git clone -b release-16.1.0-rc1 https://github.com/apache/arrow.git

cd arrow

git submodule update --init

## CPP
cd /home/third_party/arrow/cpp

mkdir build && cd build

cmake -DARROW_CSV=ON -DARROW_JSON=ON -DARROW_FILESYSTEM=ON -DARROW_COMPUTE=ON -DARROW_DATASET=ON -DARROW_PARQUET=ON -DPARQUET_REQUIRE_ENCRYPTION=ON -DARROW_BUILD_SHARED=ON  ..

make -j && make install
### ls /usr/local/lib/ | grep libarrow 
### Check install successful


## Python
cd /home/third_party/arrow/python

PYARROW_WITH_COMPUTE=1 PYARROW_PARALLEL=32 PYARROW_WITH_CSV=1 PYARROW_WITH_JSON=1 PYARROW_WITH_FILESYSTEM=1 PYARROW_WITH_DATASET=1 PYARROW_WITH_PARQUET=1 python setup.py build_ext --bundle-arrow-cpp bdist_wheel

pip install dist/pyarrow-16.1.0-cp38-cp38-linux_x86_64.whl
```

## IMLane compile

```shell
cd /home

git clone -b deploy https://github.com/AhoyDM4AI/duckdb.git

cd /home/duckdb

BUILD_PYTHON=1 GEN=ninja make

# Install dycacher by yourself
cd /home/duckdb/examples/embedded-c++
pip install -r requirements.txt

mkdir /home/duckdb/examples/embedded-c++/build
cd /home/duckdb/examples/embedded-c++/build


cmake -DCMAKE_BUILD_TYPE=release ..
make -j

# Test
python /home/duckdb/examples/embedded-c++/example.py
```

##  Other (Maybe)

`src\include\imlane\scheduler\scheduler.hpp` need to modify the variable `START_SERVER_COMMAND` to correct path

`\home\duckdb\examples\embedded-c++\imbridge\udf_server.cpp` need to modify the variable `file` path to correct path



package

```shell
docker cp /home/lulinjun/llj/workspace/deploy/duckdb imlane:/home
docker cp /home/lulinjun/llj/workspace/deploy/third_party/ imlane:/home

pip install /home/third_party/dycacher-0.98.5-py3-none-any.whl
```

