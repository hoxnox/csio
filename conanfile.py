from conans import ConanFile, CMake

class SnappyStreamConan(ConanFile):
    name = "csio"
    version = "0.1.3"
    requires = ""
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "dzip": [True, False]}
    default_options = "shared=False", "dzip=False", "zlib:shared=False", "zmq:shared=False", "zmq:encryption=None"
    generators = "cmake"
    exports = ("include/*.h", 
               "src/*.h", "src/*.c",
               "src/*.hpp", "src/*.cpp",
               "src/csio_config.cfg",
               "cmake/ext/nx_utils.cmake",
               "CMakeLists.txt",
               "README.markdown",
               "LICENSE")
    url = "https://github.com/hoxnox/csio.git"

    def requirements(self):
        if self.options.dzip:
            self.requires("zlib/1.2.11@conan/stable")
            self.requires("zmq/4.3.1@bincrafters/stable")
        else:
            self.requires("zlib/1.2.11@conan/stable")

    def build(self):
        cmake = CMake(self.settings)
        other_opts = ""
        if self.options.dzip:
            other_opts = "-DWITH_dzip=1"
        else:
            other_opts = "-DWITH_dzip=0"
        self.run('cmake %s -DWITH_CONAN=1 -DCMAKE_INSTALL_PREFIX=./distr %s %s' % 
                (self.conanfile_directory, cmake.command_line, other_opts))
        self.run("cmake --build . %s" % cmake.build_config)
        self.run("make install")


    def package(self):
        if self.options.dzip:
            self.copy("dzip", dst="bin", src="distr/bin")
        self.copy("*.h", dst="include", src="distr/include")
        self.copy("*.a", dst="lib", src="distr/lib")
        self.copy("*.lib", dst="lib", src="distr/lib")
        self.copy("*.dll", dst="lib", src="distr/lib")

    def package_info(self):
        self.cpp_info.libs = ["csio"]

