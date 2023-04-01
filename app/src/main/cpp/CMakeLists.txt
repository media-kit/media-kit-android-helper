# This file is a part of media_kit (https://github.com/alexmercerind/media_kit).
#
# Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
# All rights reserved.
# Use of this source code is governed by MIT license that can be found in the LICENSE file.

cmake_minimum_required(VERSION 3.22.1)

project("mediakitandroidhelper")

add_library(mediakitandroidhelper SHARED native-lib.cpp)

find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(
        mediakitandroidhelper
        ${log-lib}
        ${android-lib}
)
