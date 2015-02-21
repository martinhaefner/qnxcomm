cmake_minimum_required(VERSION 2.8)

include_directories(. ../userspace)

  
add_subdirectory(${GTEST_ROOT} gtest)

enable_testing()

include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})


add_executable(unittests 
   setup.cpp 
   msgsend.cpp
   msgsend_noreply.cpp
   msgsendv.cpp
   msgsend_noreplyv.cpp
   msgsendpulse.cpp
   timeout.cpp
   servercrash.cpp
   multithreaded.cpp
   poll.cpp
   disconnect.cpp
)

add_executable(testapp testapp.cpp )
add_executable(testfork fork.cpp )
add_executable(testabort abort.cpp )
add_executable(crashapp crashapp.cpp )

target_link_libraries(unittests gtest gtest_main qnxcomm rt)
target_link_libraries(testapp qnxcomm rt)
target_link_libraries(testfork qnxcomm rt)
target_link_libraries(testabort qnxcomm rt)
target_link_libraries(crashapp qnxcomm rt)

# This is so you can do 'make test' to see all your tests run, instead of
# manually running the executable runUnitTests to see those specific tests.
add_test(name "qnxcomm unit tests" command unittests)
