from conans import ConanFile, CMake
import os

channel = os.getenv("CONAN_CHANNEL", "testing")
username = os.getenv("CONAN_USERNAME", "hoxnox")

class SnappyStreamTestConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = "csio/0.1.2@%s/%s" % (username, channel)
    default_options = "csio:dzip=True"
    generators = "cmake"

    def build(self):
        cmake = CMake(self.settings)
        self.run('cmake "%s" %s' % (self.conanfile_directory, cmake.command_line))
        self.run("cmake --build . %s" % cmake.build_config)

    def imports(self):
        self.copy("*.dll", "bin", "bin")
        self.copy("dzip", "bin", "bin")
        self.copy("*.dylib", "bin", "lib")

    def test(self):
        os.chdir("bin")
        self.run(".%stest" % os.sep)
