# CMake generated Testfile for 
# Source directory: D:/TEAM/EMPCommon/SolverHub/src_t/tests
# Build directory: D:/TEAM/EMPCommon/SolverHub/src_t/build/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if("${CTEST_CONFIGURATION_TYPE}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(test_loadfromfile "D:/TEAM/EMPCommon/SolverHub/src_t/build/tests/Debug/test_loadfromfile.exe")
  set_tests_properties(test_loadfromfile PROPERTIES  _BACKTRACE_TRIPLES "D:/TEAM/EMPCommon/SolverHub/src_t/tests/CMakeLists.txt;24;add_test;D:/TEAM/EMPCommon/SolverHub/src_t/tests/CMakeLists.txt;31;add_solver_test;D:/TEAM/EMPCommon/SolverHub/src_t/tests/CMakeLists.txt;0;")
elseif("${CTEST_CONFIGURATION_TYPE}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(test_loadfromfile "D:/TEAM/EMPCommon/SolverHub/src_t/build/tests/Release/test_loadfromfile.exe")
  set_tests_properties(test_loadfromfile PROPERTIES  _BACKTRACE_TRIPLES "D:/TEAM/EMPCommon/SolverHub/src_t/tests/CMakeLists.txt;24;add_test;D:/TEAM/EMPCommon/SolverHub/src_t/tests/CMakeLists.txt;31;add_solver_test;D:/TEAM/EMPCommon/SolverHub/src_t/tests/CMakeLists.txt;0;")
elseif("${CTEST_CONFIGURATION_TYPE}" MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test(test_loadfromfile "D:/TEAM/EMPCommon/SolverHub/src_t/build/tests/MinSizeRel/test_loadfromfile.exe")
  set_tests_properties(test_loadfromfile PROPERTIES  _BACKTRACE_TRIPLES "D:/TEAM/EMPCommon/SolverHub/src_t/tests/CMakeLists.txt;24;add_test;D:/TEAM/EMPCommon/SolverHub/src_t/tests/CMakeLists.txt;31;add_solver_test;D:/TEAM/EMPCommon/SolverHub/src_t/tests/CMakeLists.txt;0;")
elseif("${CTEST_CONFIGURATION_TYPE}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test(test_loadfromfile "D:/TEAM/EMPCommon/SolverHub/src_t/build/tests/RelWithDebInfo/test_loadfromfile.exe")
  set_tests_properties(test_loadfromfile PROPERTIES  _BACKTRACE_TRIPLES "D:/TEAM/EMPCommon/SolverHub/src_t/tests/CMakeLists.txt;24;add_test;D:/TEAM/EMPCommon/SolverHub/src_t/tests/CMakeLists.txt;31;add_solver_test;D:/TEAM/EMPCommon/SolverHub/src_t/tests/CMakeLists.txt;0;")
else()
  add_test(test_loadfromfile NOT_AVAILABLE)
endif()
subdirs("../_deps/googletest-build")
