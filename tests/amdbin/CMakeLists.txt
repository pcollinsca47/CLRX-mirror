####
#  CLRadeonExtender - Unofficial OpenCL Radeon Extensions Library
#  Copyright (C) 2014-2015 Mateusz Szpakowski
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2.1 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this library; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
####

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

ADD_EXECUTABLE(GalliumBinGen GalliumBinGen.cpp)
TARGET_LINK_LIBRARIES(GalliumBinGen ${CMAKE_THREAD_LIBS_INIT} CLRXAmdBin CLRXUtils)
ADD_TEST(GalliumBinGen GalliumBinGen)

ADD_EXECUTABLE(AmdBinGen AmdBinGen.cpp)
TARGET_LINK_LIBRARIES(AmdBinGen ${CMAKE_THREAD_LIBS_INIT} CLRXAmdBin CLRXUtils)
ADD_TEST(AmdBinGen AmdBinGen)

ADD_EXECUTABLE(AmdBinLoading AmdBinLoading.cpp)
TARGET_LINK_LIBRARIES(AmdBinLoading ${CMAKE_THREAD_LIBS_INIT} CLRXAmdBin CLRXUtils)
ADD_TEST(AmdBinLoading AmdBinLoading)