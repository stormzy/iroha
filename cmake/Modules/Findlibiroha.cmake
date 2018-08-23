## Copyright Soramitsu Co., Ltd. All Rights Reserved.
## SPDX-License-Identifier: Apache-2.0
#
## This dependecy is NOT working correctly at the moment
#
#add_library(shared_model_interfaces UNKNOWN IMPORTED)
#add_library(shared_model_proto_backend UNKNOWN IMPORTED)
#
#find_path(shared_model_interfaces_INCLUDE_DIR interfaces/transaction.hpp)
#mark_as_advanced(shared_model_interfaces_INCLUDE_DIR)
#
#find_library(shared_model_interfaces_LIBRARY shared_model_interfaces)
#mark_as_advanced(shared_model_interfaces_LIBRARY)
#
#find_path(shared_model_proto_backend_INCLUDE_DIR interfaces/transaction.hpp)
#mark_as_advanced(shared_model_proto_backend_INCLUDE_DIR)
#
#find_library(shared_model_proto_backend_LIBRARY shared_model_proto_backend)
#mark_as_advanced(shared_model_proto_backend_LIBRARY)
#
#find_package_handle_standard_args(libiroha DEFAULT_MSG
#        shared_model_interfaces_INCLUDE_DIR
#        shared_model_interfaces_LIBRARY
#        shared_model_proto_backend_INCLUDE_DIR
#        shared_model_proto_backend_LIBRARY
#        )
#
#set(URL https://github.com/hyperledger/libiroha)
#set(VERSION 04e4468f40361d701f7ca13f25ce6775d6e491ac) # Latest develop
#
#if (NOT libiroha_FOUND)
#    find_package(Git REQUIRED)
#    externalproject_add(libiroha_
#        GIT_REPOSITORY ${URL}
#        GIT_TAG        ${VERSION}
#        CMAKE_ARGS
#            -DBOOST_ROOT=${BOOST_ROOT}
#            -DTESTING=OFF
#        INSTALL_COMMAND "" # remove install step
#        TEST_COMMAND "" # remove test step
#        UPDATE_COMMAND "" # remove update step
#    )
#
#    externalproject_get_property(libiroha_ binary_dir source_dir)
#
#    set(shared_model_interfaces_INCLUDE_DIR ${source_dir})
#    set(shared_model_proto_backend_INCLUDE_DIR ${source_dir})
#
#    set(shared_model_interfaces_LIBRARY ${binary_dir}/interfaces/${CMAKE_STATIC_LIBRARY_PREFIX}shared_model_interfaces${CMAKE_STATIC_LIBRARY_SUFFIX})
#    set(shared_model_proto_backend_LIBRARY ${binary_dir}/backend/protobuf/${CMAKE_STATIC_LIBRARY_PREFIX}shared_model_proto_backend${CMAKE_STATIC_LIBRARY_SUFFIX})
#
#    file(MAKE_DIRECTORY ${shared_model_interfaces_INCLUDE_DIR})
#    file(MAKE_DIRECTORY ${shared_model_proto_backend_INCLUDE_DIR})
#    include_directories(${binary_dir}/schema)
#
#    add_dependencies(shared_model_interfaces libiroha_)
#    add_dependencies(shared_model_proto_backend libiroha_)
#endif ()
#
#set_target_properties(shared_model_interfaces PROPERTIES
#        INTERFACE_INCLUDE_DIRECTORIES ${shared_model_interfaces_INCLUDE_DIR}
#        IMPORTED_LOCATION ${shared_model_interfaces_LIBRARY}
#        )
#set_target_properties(shared_model_proto_backend PROPERTIES
#        INTERFACE_INCLUDE_DIRECTORIES ${shared_model_proto_backend_INCLUDE_DIR}
#        IMPORTED_LOCATION ${shared_model_proto_backend_LIBRARY}
#        )
#
#target_link_libraries(shared_model_interfaces INTERFACE
#        boost
#        )
#target_link_libraries(shared_model_proto_backend INTERFACE
#        boost
#        )
include(FetchContent)
FetchContent_Declare(
        libiroha
        GIT_REPOSITORY https://github.com/hyperledger/libiroha
        GIT_TAG        04e4468f40361d701f7ca13f25ce6775d6e491ac
)

FetchContent_GetProperties(libiroha)
if(NOT libiroha_POPULATED)
    FetchContent_Populate(libiroha)
    add_subdirectory(${libiroha_SOURCE_DIR} ${libiroha_BINARY_DIR})
endif()

include_directories(${libiroha_SOURCE_DIR})
