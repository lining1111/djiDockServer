#重新定义当前目标的源文件的__FILE__宏
function(redefine_file_macro targetname)
    #获取当前目标的所有源文件
    get_target_property(source_files "${targetname}" SOURCES)
    #遍历源文件
    foreach (sourcefile ${source_files})
        #获取当前源文件的编译参数
        get_property(defs SOURCE "${sourcefile}"
                PROPERTY COMPILE_DEFINITIONS)
        #获取当前文件的绝对路径
        get_filename_component(filepath "${sourcefile}" ABSOLUTE)
        #将绝对路径中的项目路径替换成空,得到源文件相对于项目路径的相对路径
        string(REPLACE .. "" relpath ${filepath})
        #将我们要加的编译参数(__FILE__定义)添加到原来的编译参数里面
        list(APPEND defs "__FILE__=\"${relpath}\"")
        #重新设置源文件的编译参数
        set_property(
                SOURCE "${sourcefile}"
                PROPERTY COMPILE_DEFINITIONS ${defs}
        )
    endforeach ()
endfunction()

#需要过滤的目录
macro(SUBDIRLISTINCLUDE result curdir depth filtration_dirlist...)
    # 检查 depth 是否有效
    if (depth LESS 1)
        message(FATAL_ERROR "depth must be at least 1")
    endif ()

    # 动态生成递归模式
    set(patterns "")
    foreach (i RANGE 1 ${depth})
        # 生成当前深度的模式
        set(pattern "${curdir}")
        foreach (j RANGE 1 ${i})
            set(pattern "${pattern}/*")
        endforeach ()
        list(APPEND patterns "${pattern}")
    endforeach ()

    # 使用动态生成的模式进行递归查找
    FILE(GLOB_RECURSE children LIST_DIRECTORIES true RELATIVE ${curdir} ${patterns})

    #message(DEBUG "\n children: ${children}")
    set(dirlist "")
    foreach (child ${children})
        if ((IS_DIRECTORY ${curdir}/${child}))
            set(flags 0)
            foreach (filtration_dir ${filtration_dirlist}) #过滤目录
                string(FIND ${child} ${filtration_dir} index)
                if (${index} EQUAL 0)
                    set(flags 1)
                endif ()
            endforeach ()
            if (${flags} EQUAL 0)
                LIST(APPEND dirlist ${child})
            endif ()
        endif ()
    endforeach ()
    set(${result} ${dirlist})
endmacro()
