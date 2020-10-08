local GlobExtension = require("tundra.syntax.glob")
Build {
	ReplaceEnv = {
		OBJECTROOT = "target",
	},
	Env = {
		CPPDEFS = {
			{ "TARGET_PC_DEV_DEBUG", "TARGET_PC", "PLATFORM_64BIT"; Config = "win64-*-debug-dev" },
			{ "TARGET_PC_DEV_RELEASE", "TARGET_PC", "PLATFORM_64BIT"; Config = "win64-*-release-dev" },
			{ "TARGET_PC_TEST_DEBUG", "TARGET_PC", "PLATFORM_64BIT"; Config = "win64-*-debug-test" },
			{ "TARGET_PC_TEST_RELEASE", "TARGET_PC", "PLATFORM_64BIT"; Config = "win64-*-release-test" },
			{ "TARGET_MAC_DEV_DEBUG", "TARGET_MAC", "PLATFORM_64BIT"; Config = "macosx-*-debug-dev" },
			{ "TARGET_MAC_DEV_RELEASE", "TARGET_MAC", "PLATFORM_64BIT"; Config = "macosx-*-release-dev" },
			{ "TARGET_MAC_TEST_DEBUG", "TARGET_MAC", "PLATFORM_64BIT"; Config = "macosx-*-debug-test" },
			{ "TARGET_MAC_TEST_RELEASE", "TARGET_MAC", "PLATFORM_64BIT"; Config = "macosx-*-release-test" },
		},
	},
	Units = function ()
		-- Recursively globs for source files relevant to current build-id
		local function SourceGlob(dir)
			return FGlob {
				Dir = dir,
				Extensions = { ".c", ".cpp", ".s", ".asm" },
				Filters = {
					{ Pattern = "_win32"; Config = "win64-*-*" },
					{ Pattern = "_mac"; Config = "macosx-*-*" },
					{ Pattern = "_test"; Config = "*-*-*-test" },
				}
			}
		end
		local xbase_library = StaticLibrary {
			Name = "xbase",
			Config = "*-*-*-*",
			Sources = { SourceGlob("../xbase/source/main/cpp") },
			Includes = { "../xbase/source/main/include","../xunittest/source/main/include" },
		}
		local xallocator_library = StaticLibrary {
			Name = "xallocator",
			Config = "*-*-*-*",
			Sources = { SourceGlob("source/main/cpp") },
			Includes = { "source/main/include","../xbase/source/main/include" },
		}
		local xunittest_library = StaticLibrary {
			Name = "xunittest",
			Config = "*-*-*-test",
			Sources = { SourceGlob("../xunittest/source/main/cpp") },
			Includes = { "../xunittest/source/main/include" },
		}
		local xentry_library = StaticLibrary {
			Name = "xentry",
			Config = "*-*-*-*",
			Sources = { SourceGlob("../xentry/source/main/cpp") },
			Includes = { "../xentry/source/main/include" },
		}
		local unittest = Program {
			Name = "xallocator_test",
			Config = "*-*-*-test",
			Sources = { SourceGlob("source/test/cpp") },
			Includes = { "source/main/include","source/test/include","../xunittest/source/main/include","../xentry/source/main/include","../xbase/source/main/include","source/main/include" },
			Depends = { xbase_library,xallocator_library,xunittest_library,xentry_library },
		}
		Default(unittest)
	end,
	Configs = {
		Config {
			Name = "macosx-clang",
			Env = {
			PROGOPTS = { "-lc++" },
			CXXOPTS = {
				"-std=c++11",
				"-arch x86_64",
				"-Wno-new-returns-null",
				"-Wno-missing-braces",
				"-Wno-unused-function",
				"-Wno-unused-variable",
				"-Wno-unused-result",
				"-Wno-write-strings",
				"-Wno-c++11-compat-deprecated-writable-strings",
				"-Wno-null-dereference",
				"-Wno-format",
				"-fno-strict-aliasing",
				"-fno-omit-frame-pointer",
			},
		},
		DefaultOnHost = "macosx",
		Tools = { "clang" },
		},
		Config {
			ReplaceEnv = {
				OBJECTROOT = "target",
			},
			Name = "linux-gcc",
			DefaultOnHost = "linux",
			Tools = { "gcc" },
		},
		Config {
			ReplaceEnv = {
				OBJECTROOT = "target",
			},
			Name = "win64-msvc",
			Env = {
				PROGOPTS = { "/SUBSYSTEM:CONSOLE" },
				CXXOPTS = { },
			},
			DefaultOnHost = "windows",
			Tools = { "msvc-vs2017" },
		},
	},
	SubVariants = { "dev", "test" },
}
