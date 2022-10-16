
add_rules("mode.debug", "mode.release")

set_targetdir("bin")
set_objectdir("bin-int")
set_rundir(".")

set_languages("clatest")
set_warnings("allextra", "error")
add_defines("_CRT_SECURE_NO_WARNINGS")

add_requires("glfw", "vulkan-headers", "vulkan-loader")
add_requires("glslang", {configs = {binaryonly = true}});

target("vulkan-renderer")
    set_kind("binary")
    add_files("src/**.c")
    add_files("shaders/**.vert", "shaders/**.frag")

    add_rules("utils.glsl2spv", {outputdir = "bin"})
    add_packages("glfw", "vulkan-headers", "vulkan-loader", "glslang")
