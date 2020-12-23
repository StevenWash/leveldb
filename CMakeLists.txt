# Copyright 2017 The LevelDB Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file. See the AUTHORS file for names of contributors.

cmake_minimum_required(VERSION 3.9)
# Keep the version below in sync with the one in db.h
project(leveldb VERSION 1.22.0 LANGUAGES C CXX)

# C standard can be overridden when this is used as a sub-project.
## 与下面的CMAKE_CXX_STANDARD相似
if(NOT CMAKE_C_STANDARD)
  # This project can use C11, but will gracefully decay down to C89.
  set(CMAKE_C_STANDARD 11)
  set(CMAKE_C_STANDARD_REQUIRED OFF)
  set(CMAKE_C_EXTENSIONS OFF)
endif(NOT CMAKE_C_STANDARD)

# C++ standard can be overridden when this is used as a sub-project.
if(NOT CMAKE_CXX_STANDARD)
  # This project requires C++11.
  ## The C++ standard whose features are requested to build this target.
  ##
  ## This property specifies the C++ standard whose features are requested to build this target. For
  ## some compilers, this results in adding a flag such as -std=gnu++11 to the compile line. For
  ## compilers that have no notion of a standard level, such as Microsoft Visual C++ before 2015
  ## Update 3, this has no effect.
  ##
  ## Supported values are 98, 11, 14, 17, and 20.
  set(CMAKE_CXX_STANDARD 11)

  ## Boolean describing whether the value of CXX_STANDARD is a requirement.
  ##
  ## If this property is set to ON, then the value of the CXX_STANDARD target property is treated
  ## as a requirement. If this property is OFF or unset, the CXX_STANDARD target property is treated
  ## as optional and may “decay” to a previous standard if the requested is not available. For
  ## compilers that have no notion of a standard level, such as MSVC, this has no effect.
  set(CMAKE_CXX_STANDARD_REQUIRED ON)

  # Boolean specifying whether compiler specific extensions are requested.
  ##
  ## This property specifies whether compiler specific extensions should be used. For some compilers,
  ## this results in adding a flag such as -std=gnu++11 instead of -std=c++11 to the compile line.
  ## This property is ON by default. The basic C++ standard level is controlled by the CXX_STANDARD
  ## target property.
  set(CMAKE_CXX_EXTENSIONS OFF)
endif(NOT CMAKE_CXX_STANDARD)

if (WIN32)
  set(LEVELDB_PLATFORM_NAME LEVELDB_PLATFORM_WINDOWS)
  # TODO(cmumford): Make UNICODE configurable for Windows.

  ## Add -D define flags to the compilation of source files.
  ##
  ## add_definitions(-DFOO -DBAR ...)
  ## Adds definitions to the compiler command line for targets in the current directory and below
  ## (whether added before or after this command is invoked). This command can be used to add any
  ## flags, but it is intended to add preprocessor definitions.
  add_definitions(-D_UNICODE -DUNICODE)
else (WIN32)
  set(LEVELDB_PLATFORM_NAME LEVELDB_PLATFORM_POSIX)
endif (WIN32)

## Provide an option that the user can optionally select.
##
## option(<variable> "<help_text>" [value])
## Provides an option for the user to select as ON or OFF. If no initial <value> is provided, OFF
## is used. If <variable> is already set as a normal variable then the command does nothing.
## 可通过命令行运行时更改默认值
## 比如 -DLEVELDB_BUILD_TESTS=OFF
## 控制是否编译运行测试程序
option(LEVELDB_BUILD_TESTS "Build LevelDB's unit tests" ON)

## 控制是否编译运行benchmark
option(LEVELDB_BUILD_BENCHMARKS "Build LevelDB's benchmarks" ON)

## 控制是否安装leveldb
option(LEVELDB_INSTALL "Install LevelDB's header and library" ON)

# Load and run CMake code from a file or module.
##
## include(<file|module> [OPTIONAL] [RESULT_VARIABLE <var>]
##                       [NO_POLICY_SCOPE])
## Loads and runs CMake code from the file given. Variable reads and writes access the scope of the
## caller (dynamic scoping). If OPTIONAL is present, then no error is raised if the file does not
## exist. If RESULT_VARIABLE is given the variable <var> will be set to the full filename which has
## been included or NOTFOUND if it failed.
##
## If a module is specified instead of a file, the file with name <modulename>.cmake is searched
## first in CMAKE_MODULE_PATH, then in the CMake module directory. There is one exception to this:
## if the file which calls include() is located itself in the CMake builtin module directory, then
## first the CMake builtin module directory is searched and CMAKE_MODULE_PATH afterwards. See also
## policy CMP0017.
## 引入CheckIncludeFile模块（cmake内部模块）
include(CheckIncludeFile)
## 判断头文件是否存在，结果存在 HAVE_UNISTD_H
check_include_file("unistd.h" HAVE_UNISTD_H)

include(CheckLibraryExists)
check_library_exists(crc32c crc32c_value "" HAVE_CRC32C)
check_library_exists(snappy snappy_compress "" HAVE_SNAPPY)
check_library_exists(tcmalloc malloc "" HAVE_TCMALLOC)

include(CheckCXXSymbolExists)
# Using check_cxx_symbol_exists() instead of check_c_symbol_exists() because
# we're including the header from C++, and feature detection should use the same
# compiler language that the project will use later. Principles aside, some
# versions of do not expose fdatasync() in <unistd.h> in standard C mode
# (-std=c11), but do expose the function in standard C++ mode (-std=c++11).
check_cxx_symbol_exists(fdatasync "unistd.h" HAVE_FDATASYNC)
check_cxx_symbol_exists(F_FULLFSYNC "fcntl.h" HAVE_FULLFSYNC)
check_cxx_symbol_exists(O_CLOEXEC "fcntl.h" HAVE_O_CLOEXEC)

## 如果CMAKE_CXX_COMPILER_ID的值为MSVC则执行if下面的，否则执行else
## 即两种不同的禁用方式
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # Disable C++ exceptions.
  string(REGEX REPLACE "/EH[a-z]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHs-c-")
  add_definitions(-D_HAS_EXCEPTIONS=0)

  # Disable RTTI.
  string(REGEX REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")
else(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # Enable strict prototype warnings for C code in clang and gcc.
  if(NOT CMAKE_C_FLAGS MATCHES "-Wstrict-prototypes")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wstrict-prototypes")
  endif(NOT CMAKE_C_FLAGS MATCHES "-Wstrict-prototypes")

  # Disable C++ exceptions.
  string(REGEX REPLACE "-fexceptions" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")

  # Disable RTTI.
  string(REGEX REPLACE "-frtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

# Test whether -Wthread-safety is available. See
# https://clang.llvm.org/docs/ThreadSafetyAnalysis.html
include(CheckCXXCompilerFlag)
## 检测编译器是否支持-Wthread-safety，结果存在HAVE_CLANG_THREAD_SAFETY
check_cxx_compiler_flag(-Wthread-safety HAVE_CLANG_THREAD_SAFETY)

# Used by googletest.
check_cxx_compiler_flag(-Wno-missing-field-initializers
                        LEVELDB_HAVE_NO_MISSING_FIELD_INITIALIZERS)

include(CheckCXXSourceCompiles)

# Test whether C++17 __has_include is available.
## 判断C++17是否可用，因为__has_include("文件名" 或 <文件名>)是从c++17开始出现
check_cxx_source_compiles("
#if defined(__has_include) &&  __has_include(<string>)
#include <string>
#endif
int main() { std::string str; return 0; }
" HAVE_CXX17_HAS_INCLUDE)

## 通过set来定义两个变量，
set(LEVELDB_PUBLIC_INCLUDE_DIR "include/leveldb")
set(LEVELDB_PORT_CONFIG_DIR "include/port")

## Copy a file to another location and modify its contents.
##
## configure_file(<input> <output>
##               [COPYONLY] [ESCAPE_QUOTES] [@ONLY]
##               [NO_SOURCE_PERMISSIONS]
##               [NEWLINE_STYLE [UNIX|DOS|WIN32|LF|CRLF] ])
## Copies an <input> file to an <output> file and substitutes variable values referenced as @VAR@ or ${VAR} in the input file content.
## Each variable reference will be replaced with the current value of the variable, or the empty string if the variable is not defined.
## 复制文件到指定目录下，其中${}相当于取变量值，变量就是前面定义的变量
## 其中PROJECT_BINARY_DIR指定是编译的二进制目录，可以通过--build参数来指定
configure_file(
  "port/port_config.h.in"
  "${PROJECT_BINARY_DIR}/${LEVELDB_PORT_CONFIG_DIR}/port_config.h"
)

## Add include directories to the build.
##
## include_directories([AFTER|BEFORE] [SYSTEM] dir1 [dir2 ...])
## Add the given directories to those the compiler uses to search for include files.
## Relative paths are interpreted as relative to the current source directory.
include_directories(
  "${PROJECT_BINARY_DIR}/include"
  "."
)

## BUILD_SHARED_LIBS:
##    全局标记，如果启用，将导致add_library（）创建共享库。
##    如果存在且为true，这将导致所有库都构建为共享库，除非将该库显式添加为静态库。
##    通常将此变量作为option（）添加到项目中，以便项目的每个用户都可以决定是否要使用共享库或静态库来构建项目。
if(BUILD_SHARED_LIBS)
  # Only export LEVELDB_EXPORT symbols from the shared library.
  ## 编译的时候全部隐藏接口，只有在代码中显式export出来的才会显示
  add_compile_options(-fvisibility=hidden)
endif(BUILD_SHARED_LIBS)

# Must be included before CMAKE_INSTALL_INCLUDEDIR is used.
include(GNUInstallDirs)

## add_library(<name> [STATIC | SHARED | MODULE]
##            [EXCLUDE_FROM_ALL]
##            [<source>...])
## Adds a library target called <name> to be built from the source files listed in the command invocation.
## (The source files can be omitted here if they are added later using target_sources().)
## 指定要生成的库为leveldb
add_library(leveldb "")

## Add sources to a target.
##
## target_sources(<target>
##  <INTERFACE|PUBLIC|PRIVATE> [items1...]
##  [<INTERFACE|PUBLIC|PRIVATE> [items2...] ...])
# Specifies sources to use when building a target and/or its dependents.
## 指定leveldb库所依赖的源文件（https://cmake.org/cmake/help/latest/command/target_sources.html）
## PRIVATE 表明生成leveldb的编译选项不传递（引用了leveldb的target，不确定就写private），可选值有private\public\interface
target_sources(leveldb
  PRIVATE
    "${PROJECT_BINARY_DIR}/${LEVELDB_PORT_CONFIG_DIR}/port_config.h"
    "db/builder.cc"
    "db/builder.h"
    "db/c.cc"
    "db/db_impl.cc"
    "db/db_impl.h"
    "db/db_iter.cc"
    "db/db_iter.h"
    "db/dbformat.cc"
    "db/dbformat.h"
    "db/dumpfile.cc"
    "db/filename.cc"
    "db/filename.h"
    "db/log_format.h"
    "db/log_reader.cc"
    "db/log_reader.h"
    "db/log_writer.cc"
    "db/log_writer.h"
    "db/memtable.cc"
    "db/memtable.h"
    "db/repair.cc"
    "db/skiplist.h"
    "db/snapshot.h"
    "db/table_cache.cc"
    "db/table_cache.h"
    "db/version_edit.cc"
    "db/version_edit.h"
    "db/version_set.cc"
    "db/version_set.h"
    "db/write_batch_internal.h"
    "db/write_batch.cc"
    "port/port_stdcxx.h"
    "port/port.h"
    "port/thread_annotations.h"
    "table/block_builder.cc"
    "table/block_builder.h"
    "table/block.cc"
    "table/block.h"
    "table/filter_block.cc"
    "table/filter_block.h"
    "table/format.cc"
    "table/format.h"
    "table/iterator_wrapper.h"
    "table/iterator.cc"
    "table/merger.cc"
    "table/merger.h"
    "table/table_builder.cc"
    "table/table.cc"
    "table/two_level_iterator.cc"
    "table/two_level_iterator.h"
    "util/arena.cc"
    "util/arena.h"
    "util/bloom.cc"
    "util/cache.cc"
    "util/coding.cc"
    "util/coding.h"
    "util/comparator.cc"
    "util/crc32c.cc"
    "util/crc32c.h"
    "util/env.cc"
    "util/filter_policy.cc"
    "util/hash.cc"
    "util/hash.h"
    "util/logging.cc"
    "util/logging.h"
    "util/mutexlock.h"
    "util/no_destructor.h"
    "util/options.cc"
    "util/random.h"
    "util/status.cc"

  # Only CMake 3.3+ supports PUBLIC sources in targets exported by "install".
  ## $<{表达式1}:PUBLIC> : 表示如果{表达式1}值为1时，值为PUBLIC，否则为空
  ## $<VERSION_GREATER:CMAKE_VERSION,3.2> : 如果CMAKE_VERSION的值大于3.2，则值为1，否则为0
  ## LEVELDB_PUBLIC_INCLUDE_DIR即为上面设置的变量
  $<$<VERSION_GREATER:CMAKE_VERSION,3.2>:PUBLIC>
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/c.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/cache.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/comparator.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/db.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/dumpfile.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/env.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/export.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/filter_policy.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/iterator.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/options.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/slice.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/status.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/table_builder.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/table.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/write_batch.h"
)

## 如果是win32的环境，再添加几个windows依赖源文件
## 否则添加posix的依赖源文件
if (WIN32)
  target_sources(leveldb
    PRIVATE
      "util/env_windows.cc"
      "util/windows_logger.h"
  )
else (WIN32)
  target_sources(leveldb
    PRIVATE
      "util/env_posix.cc"
      "util/posix_logger.h"
  )
endif (WIN32)

# MemEnv is not part of the interface and could be pulled to a separate library.
target_sources(leveldb
  PRIVATE
    "helpers/memenv/memenv.cc"
    "helpers/memenv/memenv.h"
)

## build和install时候的包含路径是不同的
## 不同的情形下运用不同的变量
## CMAKE_INSTALL_INCLUDEDIR的默认值是 include
target_include_directories(leveldb
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(leveldb
  PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

target_compile_definitions(leveldb
  PRIVATE
    # Used by include/export.h when building shared libraries.
    LEVELDB_COMPILE_LIBRARY
    # Used by port/port.h.
    ${LEVELDB_PLATFORM_NAME}=1
)

## HAVE_CXX17_HAS_INCLUDE 是上面定义的变量
## 如果变量没有值，则新增一个变量LEVELDB_HAS_PORT_CONFIG_H=1
if (NOT HAVE_CXX17_HAS_INCLUDE)
  target_compile_definitions(leveldb
    PRIVATE
      LEVELDB_HAS_PORT_CONFIG_H=1
  )
endif(NOT HAVE_CXX17_HAS_INCLUDE)

## 参考上面的解释
## 如果是构建共享库，则define一个值该值可以在header文件中使用
##   例如 #if defined(LEVELDB_SHARED_LIBRARY)
if(BUILD_SHARED_LIBS)
  target_compile_definitions(leveldb
    PUBLIC
      # Used by include/export.h.
      LEVELDB_SHARED_LIBRARY
  )
endif(BUILD_SHARED_LIBS)

## 如果编译器支持-Wthread-safety，则在编译的时候添加编译选项
##   -Werror会视警告为错误进行提示
if(HAVE_CLANG_THREAD_SAFETY)
  target_compile_options(leveldb
    PUBLIC
      -Werror -Wthread-safety)
endif(HAVE_CLANG_THREAD_SAFETY)

## 上面有检查是否支持crc32，支持的话HAVE_CRC32C为1，否则为空
## 下面同理
if(HAVE_CRC32C)
  ## 如果支持crc32，则链接crc32c的库
  target_link_libraries(leveldb crc32c)
endif(HAVE_CRC32C)
if(HAVE_SNAPPY)
  target_link_libraries(leveldb snappy)
endif(HAVE_SNAPPY)
if(HAVE_TCMALLOC)
  target_link_libraries(leveldb tcmalloc)
endif(HAVE_TCMALLOC)

# Needed by port_stdcxx.h
## 查找指定的的库Threads，且设置为REQUIRED，表明没有找到Threads会报错
find_package(Threads REQUIRED)

## 此处表示找到了Threads库，进行库的链接加载
target_link_libraries(leveldb Threads::Threads)

## 添加可执行文件，指定可执行文件的main文件
add_executable(leveldbutil
  "db/leveldbutil.cc"
)
## 添加可执行文件leveldbutil的依赖库，为上面创建的leveldb的库
target_link_libraries(leveldbutil leveldb)

## 由上面指定的变量值LEVELDB_BUILD_TESTS: 是否编译测试文件
if(LEVELDB_BUILD_TESTS)

  ## 如果编译test文件，必须启动test的相关参数设置
  enable_testing()

  # Prevent overriding the parent project's compiler/linker settings on Windows.
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(install_gtest OFF)
  set(install_gmock OFF)
  set(build_gmock ON)

  # This project is tested using GoogleTest.
  ## 添加外部项目googletest
  add_subdirectory("third_party/googletest")

  # This project uses Google benchmark for benchmarking.
  ## 设置benchmark需要用的参数
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_EXCEPTIONS OFF CACHE BOOL "" FORCE)

  ## 添加外部项目benchmark
  add_subdirectory("third_party/benchmark")

  # GoogleTest triggers a missing field initializers warning.
  if(LEVELDB_HAVE_NO_MISSING_FIELD_INITIALIZERS)
    set_property(TARGET gtest
        APPEND PROPERTY COMPILE_OPTIONS -Wno-missing-field-initializers)
    set_property(TARGET gmock
        APPEND PROPERTY COMPILE_OPTIONS -Wno-missing-field-initializers)
  endif(LEVELDB_HAVE_NO_MISSING_FIELD_INITIALIZERS)

  ## 第一个参数leveldb_test是函数名
  ## 第二个参数test_file是函数的参数
  function(leveldb_test test_file)

    ## get_filename_component(<var> <FileName> <mode> [CACHE])
    ##      Sets <var> to a component of <FileName>, where <mode> is one of:
    ##
    ## DIRECTORY = Directory without file name
    ## NAME      = File name without directory
    ## EXT       = File name longest extension (.b.c from d/a.b.c)
    ## NAME_WE   = File name with neither the directory nor the longest extension
    ## LAST_EXT  = File name last extension (.c from d/a.b.c)
    ## NAME_WLE  = File name with neither the directory nor the last extension
    ## PATH      = Legacy alias for DIRECTORY (use for CMake <= 2.8.11)
    ## 提取 test_file 中的文件名，把目录和后缀都去掉
    ## 例如：your_path/my_test.cc -> my_test
    ## 结果存在 test_target_name 中
    get_filename_component(test_target_name "${test_file}" NAME_WE)

    ## 将处理之后的文件名test_target_name添加为可执行文件
    add_executable("${test_target_name}" "")
    ## 为上面创建的可执行文件设置依赖的文件
    target_sources("${test_target_name}"
      PRIVATE
        "${PROJECT_BINARY_DIR}/${LEVELDB_PORT_CONFIG_DIR}/port_config.h"
        "util/testutil.cc"
        "util/testutil.h"

        "${test_file}"
    )

    ## 链接依赖的执行文件leveldb gmock gtest benchmark
    target_link_libraries("${test_target_name}" leveldb gmock gtest benchmark)

    ## 设置可以通过##if defined(...)方式使用的定义变量
    target_compile_definitions("${test_target_name}"
      PRIVATE
        ${LEVELDB_PLATFORM_NAME}=1
    )

    ## 如果不支持C++17
    if (NOT HAVE_CXX17_HAS_INCLUDE)
      target_compile_definitions("${test_target_name}"
        PRIVATE
          LEVELDB_HAS_PORT_CONFIG_H=1
      )
    endif(NOT HAVE_CXX17_HAS_INCLUDE)

    ## Add a test to the project to be run by ctest(1).
    ##
    ## add_test(NAME <name> COMMAND <command> [<arg>...]
    ##         [CONFIGURATIONS <config>...]
    ##         [WORKING_DIRECTORY <dir>]
    ##         [COMMAND_EXPAND_LISTS])
    ## 运行测试的可执行文件
    add_test(NAME "${test_target_name}" COMMAND "${test_target_name}")
  endfunction(leveldb_test)

  ## 下面都是开始调用上面定义的函数leveldb_test来执行测试文件
  leveldb_test("db/c_test.c")
  leveldb_test("db/fault_injection_test.cc")

  leveldb_test("issues/issue178_test.cc")
  leveldb_test("issues/issue200_test.cc")
  leveldb_test("issues/issue320_test.cc")

  leveldb_test("util/env_test.cc")
  leveldb_test("util/status_test.cc")
  leveldb_test("util/no_destructor_test.cc")

  if(NOT BUILD_SHARED_LIBS)
    leveldb_test("db/autocompact_test.cc")
    leveldb_test("db/corruption_test.cc")
    leveldb_test("db/db_test.cc")
    leveldb_test("db/dbformat_test.cc")
    leveldb_test("db/filename_test.cc")
    leveldb_test("db/log_test.cc")
    leveldb_test("db/recovery_test.cc")
    leveldb_test("db/skiplist_test.cc")
    leveldb_test("db/version_edit_test.cc")
    leveldb_test("db/version_set_test.cc")
    leveldb_test("db/write_batch_test.cc")

    leveldb_test("helpers/memenv/memenv_test.cc")

    leveldb_test("table/filter_block_test.cc")
    leveldb_test("table/table_test.cc")

    leveldb_test("util/arena_test.cc")
    leveldb_test("util/bloom_test.cc")
    leveldb_test("util/cache_test.cc")
    leveldb_test("util/coding_test.cc")
    leveldb_test("util/crc32c_test.cc")
    leveldb_test("util/hash_test.cc")
    leveldb_test("util/logging_test.cc")

    # TODO(costan): This test also uses
    #               "util/env_{posix|windows}_test_helper.h"
    if (WIN32)
      leveldb_test("util/env_windows_test.cc")
    else (WIN32)
      leveldb_test("util/env_posix_test.cc")
    endif (WIN32)
  endif(NOT BUILD_SHARED_LIBS)
endif(LEVELDB_BUILD_TESTS)
## 完成测试文件的执行


## 与上面测试LEVELDB_BUILD_TESTS流程相似
## 是否编译运行benchmark
if(LEVELDB_BUILD_BENCHMARKS)

  ## 同样定义一个benchmark的函数
  function(leveldb_benchmark bench_file)
    get_filename_component(bench_target_name "${bench_file}" NAME_WE)

    add_executable("${bench_target_name}" "")
    target_sources("${bench_target_name}"
      PRIVATE
        "${PROJECT_BINARY_DIR}/${LEVELDB_PORT_CONFIG_DIR}/port_config.h"
        "util/histogram.cc"
        "util/histogram.h"
        "util/testutil.cc"
        "util/testutil.h"

        "${bench_file}"
    )
    target_link_libraries("${bench_target_name}" leveldb gmock gtest)
    target_compile_definitions("${bench_target_name}"
      PRIVATE
        ${LEVELDB_PLATFORM_NAME}=1
    )
    if (NOT HAVE_CXX17_HAS_INCLUDE)
      target_compile_definitions("${bench_target_name}"
        PRIVATE
          LEVELDB_HAS_PORT_CONFIG_H=1
      )
    endif(NOT HAVE_CXX17_HAS_INCLUDE)
  endfunction(leveldb_benchmark)

  if(NOT BUILD_SHARED_LIBS)
    leveldb_benchmark("benchmarks/db_bench.cc")
  endif(NOT BUILD_SHARED_LIBS)

  check_library_exists(sqlite3 sqlite3_open "" HAVE_SQLITE3)
  if(HAVE_SQLITE3)
    leveldb_benchmark("benchmarks/db_bench_sqlite3.cc")
    target_link_libraries(db_bench_sqlite3 sqlite3)
  endif(HAVE_SQLITE3)

  # check_library_exists is insufficient here because the library names have
  # different manglings when compiled with clang or gcc, at least when installed
  # with Homebrew on Mac.
  set(OLD_CMAKE_REQURED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
  list(APPEND CMAKE_REQUIRED_LIBRARIES kyotocabinet)
  check_cxx_source_compiles("
#include <kcpolydb.h>

int main() {
  kyotocabinet::TreeDB* db = new kyotocabinet::TreeDB();
  delete db;
  return 0;
}
  "  HAVE_KYOTOCABINET)
  set(CMAKE_REQUIRED_LIBRARIES ${OLD_CMAKE_REQURED_LIBRARIES})
  if(HAVE_KYOTOCABINET)
    leveldb_benchmark("benchmarks/db_bench_tree_db.cc")
    target_link_libraries(db_bench_tree_db kyotocabinet)
  endif(HAVE_KYOTOCABINET)
endif(LEVELDB_BUILD_BENCHMARKS)
## 完成编译运行benchmark

## 如果上面的变量LEVELDB_INSTALL为ON则开始执行INSTALL
if(LEVELDB_INSTALL)
  install(TARGETS leveldb
    EXPORT leveldbTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
  install(
    FILES
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/c.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/cache.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/comparator.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/db.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/dumpfile.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/env.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/export.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/filter_policy.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/iterator.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/options.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/slice.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/status.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/table_builder.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/table.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/write_batch.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/leveldb"
  )

  include(CMakePackageConfigHelpers)
  configure_package_config_file(
    "cmake/${PROJECT_NAME}Config.cmake.in"
    "${PROJECT_BINARY_DIR}/cmake/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  )
  write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/cmake/${PROJECT_NAME}ConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
  )
  install(
    EXPORT leveldbTargets
    NAMESPACE leveldb::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  )
  install(
    FILES
      "${PROJECT_BINARY_DIR}/cmake/${PROJECT_NAME}Config.cmake"
      "${PROJECT_BINARY_DIR}/cmake/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  )
endif(LEVELDB_INSTALL)

set(SOURCE_FILES apps/hello_world.cpp)
set(INC_DIR /usr/local/include)
add_executable(helloWorld ${SOURCE_FILES})
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath -Wl,/usr/local/lib") #新加行，编译选项，添加库路径
include_directories(${INC_DIR}) #新加行，设置头文件目录
target_link_libraries(helloWorld /usr/local/lib/libleveldb.a) #新加行，链接库，格式为target_link_libraries(工程名 库名)
find_package(Threads REQUIRED)#新加行，依赖于threads库
target_link_libraries(helloWorld Threads::Threads)#新加行，链接库