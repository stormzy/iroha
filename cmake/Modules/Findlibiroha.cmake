add_library(libiroha UNKNOWN IMPORTED)

find_path(libiroha_INCLUDE_DIR interfaces/transaction.hpp)
mark_as_advanced(ed25519_INCLUDE_DIR)

find_library(libiroha_LIBRARY libiroha)
mark_as_advanced(libiroha_LIBRARY)

find_package_handle_standard_args(libiroha DEFAULT_MSG
        libiroha_INCLUDE_DIR
        libiroha_LIBRARY
        )

set(URL https://github.com/hyperledger/libiroha)
set(VERSION a69e78eeedb3bb2bd7663e0f501eaed3a06a0f49) # Latest develop

if (NOT libiroha_FOUND)
    find_package(Git REQUIRED)
    externalproject_add(libiroha_
        GIT_REPOSITORY ${URL}
        GIT_TAG        ${VERSION}
        CMAKE_ARGS
            -DBOOST_ROOT=/Users/nick/Desktop/work/soramitsu/libs/install/
        INSTALL_COMMAND "" # remove install step
        TEST_COMMAND "" # remove test step
        UPDATE_COMMAND "" # remove update step
    )

    externalproject_get_property(libiroha_ binary_dir)
    externalproject_get_property(libiroha_ source_dir)
    set(libiroha_INCLUDE_DIR ${source_dir})
    set(libiroha_LIBRARY ${binary_dir}/${CMAKE_STATIC_LIBRARY_PREFIX}libiroha${CMAKE_STATIC_LIBRARY_SUFFIX})
    file(MAKE_DIRECTORY ${libiroha_INCLUDE_DIR})
    link_directories(${binary_dir})

    add_dependencies(libiroha libiroha_)
endif ()

set_target_properties(libiroha PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${libiroha_INCLUDE_DIR}
        IMPORTED_LOCATION ${libiroha_LIBRARY}
        )
