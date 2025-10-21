check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

set(headers
    model/pro-sink-app.h
)

set(sources
    model/pro-sink-app.cc
)

build_lib(
    LIBNAME pro-sink-app
    SOURCE_FILES ${sources}
    HEADER_FILES ${headers}
    # --- 关键修改在这里 ---
    # 我们需要链接到所有用到的 ns-3 模块
    LIBRARIES_TO_LINK
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libapplications}
    # ---------------------
)
