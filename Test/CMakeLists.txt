FILE(GLOB_RECURSE LibraryList "../3rd_party/*.lib")

add_executable(CppOverdubTest test.cpp)

target_include_directories(CppOverdubTest PUBLIC "../3rd_party/")
target_link_libraries(CppOverdubTest PUBLIC ${LibraryList} stdc++exp)
