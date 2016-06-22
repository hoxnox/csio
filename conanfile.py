from conans import ConanFile, CMake

class SnappyStreamConan(ConanFile):
    name = "csio"
    version = "0.1.2"
    requires = ""
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "dzip": [True, False]}
    default_options = "shared=False", "dzip=False", "glog:shared=False", "zlib:shared=False", "libzmq:shared=False"
    generators = "cmake"
    exports = "*"

    def requirements(self):
        if self.options.dzip:
            self.requires("zlib/1.2.8@lasote/stable")
            self.requires("libzmq/4.1.5@memsharded/stable")
            self.requires("glog/0.3.4@dwerner/testing")
        else:
            self.requires("zlib/1.2.8@lasote/stable")

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

    def package_info(self):
        self.cpp_info.libs = ["csio"]

