FILE(GLOB_RECURSE LibraryList "../3rd_party/*.lib")
link_directories("../3rd_party")
link_libraries(${LibraryList})

add_executable(CppOverdubTest test.cpp)

target_include_directories(CppOverdubTest PUBLIC "../3rd_party/")
target_link_libraries(CppOverdubTest PUBLIC ${LibraryList} "F:\\Program Files\\mingw64\\lib\\libstdc++exp.a")
