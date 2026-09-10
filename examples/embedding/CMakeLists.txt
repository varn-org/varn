# minimal consumer that links the installed varn package.
# build and install varn as a package first:
#   python3 varn.py lib --prefix <prefix> --install
# then point this project at it:
#   cmake -B build -S . -DCMAKE_PREFIX_PATH=<prefix> && cmake --build build && ./build/embed
cmake_minimum_required(VERSION 3.20)
project(varn_embedding_example C)

find_package(varn REQUIRED)

add_executable(embed main.c)
target_link_libraries(embed PRIVATE varn::varn)
