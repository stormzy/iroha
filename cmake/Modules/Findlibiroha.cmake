# Copyright Soramitsu Co., Ltd. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0

include(FetchContent)
FetchContent_Declare(
        libiroha
        GIT_REPOSITORY https://github.com/hyperledger/libiroha
        GIT_TAG        04e4468f40361d701f7ca13f25ce6775d6e491ac
)

FetchContent_GetProperties(libiroha)
if(NOT libiroha_POPULATED)
    set(TESTING OFF)
    FetchContent_Populate(libiroha)
    add_subdirectory(${libiroha_SOURCE_DIR} ${libiroha_BINARY_DIR})
endif()

set(EP_PREFIX ${libiroha_SOURCE_DIR}/external)
set_directory_properties(PROPERTIES
        EP_PREFIX ${EP_PREFIX}
    )

include_directories(${libiroha_SOURCE_DIR})
