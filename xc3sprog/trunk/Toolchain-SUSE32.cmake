set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_C_COMPILER gcc -m32)
set(CMAKE_CXX_COMPILER g++ -m32)
set(CMAKE_FIND_ROOT_PATH /usr/lib)
set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS OFF)
