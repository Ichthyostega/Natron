# ***** BEGIN LICENSE BLOCK *****
# This file is part of Natron <http://www.natron.fr/>,
# Copyright (C) 2013-2017 INRIA and Alexandre Gauthier-Foichat
#
# Natron is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# Natron is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Natron.  If not, see <http://www.gnu.org/licenses/gpl-2.0.html>
# ***** END LICENSE BLOCK *****

TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG += moc rcc
CONFIG += boost opengl qt cairo python shiboken pyside 
CONFIG += static-gui static-engine static-host-support static-breakpadclient static-libmv static-openmvg static-ceres static-libtess
QT += gui core opengl network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets concurrent

CONFIG += openmvg-flags glad-flags

!noexpat: CONFIG += expat

INCLUDEPATH += /usr/src/googletest/googletest/include
INCLUDEPATH += /usr/src/googletest/googletest
INCLUDEPATH += /usr/src/googletest/googlemock/include
INCLUDEPATH += /usr/src/googletest/googlemock


QMAKE_CLEAN += ofxTestLog.txt test_dot_generator0.jpg

include(../global.pri)

SOURCES += \
    /usr/src/googletest/googletest/src/gtest-all.cc \
    /usr/src/googletest/googletest/src/gtest_main.cc \
    /usr/src/googletest/googlemock/src/gmock-all.cc \
    BaseTest.cpp \
    Hash64_Test.cpp \
    Image_Test.cpp \
    Lut_Test.cpp \
    KnobFile_Test.cpp \
    Curve_Test.cpp \
    Tracker_Test.cpp \
    wmain.cpp

HEADERS += \
    BaseTest.h
