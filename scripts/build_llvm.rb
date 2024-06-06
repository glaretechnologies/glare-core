#
# Builds LLVM on Windows, Mac or Linux.
#
#
# cd %GLARE_CORE_LIBS%/llvm
# mkdir third-party
# cd third-party
# git clone https://github.com/google/benchmark.git
#

require 'fileutils'
require 'net/http'
require './script_utils.rb'
require './cmake.rb'


puts "-------------------------------------
LLVM build

"


# Set up defaults
$vs_version = 2022 # Visual Studio version
$llvm_version = "15.0.7"
$configurations = [ :release, :debug ]
# todo: use CMakeBuild.config_opts
$config_opts = {
	:debug => ["Debug", "_debug"],
	:release => ["Release", ""]
}
$forcerebuild = false
$build_epoch = 1


def printUsage()
	puts "Usage: build_llvm.rb [arguments]"
	puts ""
	puts "Options marked with * are the default."
	puts ""
	puts "\t--release, -R\t\tSpecifies the LLVM release to get. Default is #{$llvm_version}."
	puts ""
	puts "\t--config, -c\t\tSpecifies the config to build. Release, Debug, or Both. Default is Both."
	puts ""
	puts "\t--vsversion, -v\t\tSpecifies the vs version to use. Valid options are: 2012, 2013, 2015, 2017, 2019, 2022. Default is: #{$vs_version}."
	puts ""
	puts "\t--forcerebuild, -f\tForce a rebuild."
	puts ""
	puts "\t--help, -h\t\tShows this help."
	puts ""
end


arg_parser = ArgumentParser.new(ARGV)

arg_parser.options.each do |opt|
	if opt[0] == "--release" || opt[0] == "-R"
		if opt[1] == nil
			puts "Using default version: #{$llvm_version}"
		else
			$llvm_version = opt[1]
		end
	elsif opt[0] == "--help" || opt[0] == "-h"
		printUsage()
		exit 0
	elsif opt[0] == "--config" || opt[0] == "-c"
		if opt[1] == nil
			puts "Using default config."
		else
			config = opt[1].downcase
			if(config != "release" and config != "debug" and config != "both")
				STDERR.puts "Unknown config #{opt[1]}."
				exit(1)
			end
		
			$configurations = []
			$configurations << :debug if config == "both" || config == "debug"
			$configurations << :release if config == "both" || config == "release"
		end
	elsif opt[0] == "--vsversion" || opt[0] == "-v"
		$vs_version = opt[1].to_i
		if not [2015, 2017, 2019, 2022].include?($vs_version)
			puts "Unsupported VS version: #{opt[1]}. Skipping."
			exit 0
		end
	elsif opt[0] == "--forcerebuild" || opt[0] == "-f"
		$forcerebuild = true
	else
		puts "Unrecognised argument: #{opt[0]}"
		exit 1
	end
end


# returns the path it was extracted to
def getLLVMSourceDownloadAndExtract()
	puts "Downloading LLVM release #{$llvm_version}..."

	extension = (Gem::Version.new($llvm_version) >= Gem::Version.new('3.6')) ? "xz" : "gz"

	src_file = "llvm-#{$llvm_version}.src.tar.#{extension}"
	src_folder = (Gem::Version.new($llvm_version) <= Gem::Version.new('3.4')) ? "llvm-#{$llvm_version}" : "llvm-#{$llvm_version}.src"
	
	if(Gem::Version.new($llvm_version) >= Gem::Version.new('11.0.0')) # At some version the downloads moved to github, not sure when.
		downloadFileHTTPSIfNotOnDisk(src_file, "https://github.com/llvm/llvm-project/releases/download/llvmorg-#{$llvm_version}/llvm-#{$llvm_version}.src.tar.xz")
	else
		downloadFileHTTPSIfNotOnDisk(src_file, "https://releases.llvm.org/#{$llvm_version}/#{src_file}")
	end
		
	
	puts "src_file: #{src_file}"
	puts "src_folder: #{src_folder}"
	extractArchiveIfNotExtraced(src_file, src_folder)

	# Revent LLVM builds also need a Cmake archive extracted and renamed, see https://github.com/llvm/llvm-project/issues/53281
	if(Gem::Version.new($llvm_version) >= Gem::Version.new('15.0'))
		cmake_src_file   = "cmake-#{$llvm_version}.src.tar.xz"
		cmake_src_folder = "cmake-#{$llvm_version}.src"
		downloadFileHTTPSIfNotOnDisk(cmake_src_file, "https://github.com/llvm/llvm-project/releases/download/llvmorg-#{$llvm_version}/#{cmake_src_file}")
	
		extractArchiveIfNotExtraced(
			cmake_src_file, # archive
			cmake_src_folder # target dir
		)
		
		FileUtils.rm_r("cmake") if Dir.exist?("cmake")
		FileUtils.mv(cmake_src_folder, "cmake", :verbose=>true)
	end

	# Get benchmark src, which seems to be required, even though we build without benchmark support.
	if !(Dir.exist?("third-party") && Dir.exist?("third-party/benchmark"))
		
		FileUtils.mkdir("third-party") if !Dir.exist?("third-party")

		Dir.chdir("third-party") do 
			print_and_exec_command("git clone https://github.com/google/benchmark.git")
		end
	end
	
	return src_folder
end


def getOutputDirName(configuration, dir_type, vs_version = -1)
	version_string = $llvm_version.gsub(".", "_")
	config_suffix = $config_opts[configuration][1] # todo: use CMakeBuild.config_opts

	if OS.windows?
		if vs_version == -1
			STDERR.puts "VS version not set."
			exit 1
		end
		
		return "llvm_#{version_string}_#{dir_type}_vs#{vs_version}_64#{config_suffix}"
	elsif OS.linux?
		return "llvm_#{version_string}_dylib_#{dir_type}#{config_suffix}"   # Make dylib build
	else
		return "llvm_#{version_string}_#{dir_type}#{config_suffix}"
	end
end


def getBuildDir(configuration, vs_version = -1)
	return getOutputDirName(configuration, "build", vs_version)
end


def getInstallDir(configuration, vs_version = -1)
	return getOutputDirName(configuration, "install", vs_version)
end


def buildLLVM(llvm_src_dir, vs_version = -1)
	$configurations.each do |configuration|
		cmake_build = CMakeBuild.new
		
		build_dir   = "#{$llvm_dir}/#{getBuildDir(configuration, vs_version)}"
		install_dir = "#{$llvm_dir}/#{getInstallDir(configuration, vs_version)}"

		cmake_build.init("LLVM",
			"#{$llvm_dir}/#{llvm_src_dir}",
			build_dir,
			install_dir)
			
		cmake_args = ""
		
		if OS.mac?
			# While we target 10.8, we need to tell oidn to use libc++.
			cmake_args += " -DCMAKE_CXX_FLAGS:STRING=\"-std=c++11 -stdlib=libc++\"" +
				" -DCMAKE_EXE_LINKER_FLAGS:STRING=\"-stdlib=libc++\"" +
				" -DCMAKE_SHARED_LINKER_FLAGS:STRING=\"-stdlib=libc++\"" +
				" -DCMAKE_MODULE_LINKER_FLAGS:STRING=\"-stdlib=libc++\""
		end
		
		if OS.windows?
			cmake_args += " -DCMAKE_CXX_FLAGS:STRING=\"-D_SECURE_SCL=0\"" if configuration == :release
			
			if Gem::Version.new($llvm_version) >= Gem::Version.new('8.0.0')
				cmake_args += " -DLLVM_TEMPORARILY_ALLOW_OLD_TOOLCHAIN=ON "
			end
		end
		
		
		if OS.linux?
			# Required to avoid clashing with system LLVM (used e.g. in Mesa OpenGL drivers)
			cmake_args += " -DLLVM_BUILD_LLVM_DYLIB=TRUE"
		end
		
		cmake_args += " -DLLVM_OPTIMIZED_TABLEGEN=ON"
		
		# Disable a bunch of stuff we don't need, to speed up build
		cmake_args += " -DLLVM_INCLUDE_EXAMPLES=OFF -DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_BENCHMARKS=OFF -DLLVM_BUILD_TOOLS=OFF"
		
		# Enable exception handling, as we currently throw exceptions through LLVM code.  
		cmake_args += " -DLLVM_ENABLE_EH=ON"
		
		# Note: When building with GCC, inheriting from an LLVM class requires LLVM to be built with RTTI. (as long as indigo uses RTTI, which it does)
		# And WinterMemoryManager in VirtualMachine.cpp in Winter trunk inherits from an LLVM class.
		# Also RTTI is required when DLLVM_ENABLE_EH=ON.
		cmake_args += " -DLLVM_ENABLE_RTTI=ON"
		
		if OS.arm64?
			cmake_args += " -DLLVM_TARGETS_TO_BUILD=\"AArch64\""
		else
			cmake_args += " -DLLVM_TARGETS_TO_BUILD=\"X86\""
		end
		
		allow_reconfig = false # can set to true to avoid deleting build dir before build
		cmake_build.configure(configuration, vs_version, cmake_args, allow_reconfig, OS.arm64?)

		targets = OS.windows? ? [] : ["llvm-config"]
		cmake_build.build(targets)
		cmake_build.install($build_epoch)

		# Install llvm-config ourselves
		FileUtils.cp(build_dir + "/bin/llvm-config", install_dir + "/bin/llvm-config") if !OS.windows?
	end
end


$glare_core_libs_dir = ENV['GLARE_CORE_LIBS']
if $glare_core_libs_dir.nil?
	puts "GLARE_CORE_LIBS env var not defined."
	exit(1)
end

# this is cmake, we cant have backslashes.
$glare_core_libs_dir = $glare_core_libs_dir.gsub("\\", "/")


$llvm_dir = "#{$glare_core_libs_dir}/llvm"

FileUtils.mkdir($llvm_dir, :verbose=>true) if !Dir.exist?($llvm_dir)
puts "Chdir to \"#{$llvm_dir}\"."
Dir.chdir($llvm_dir)


# If force rebuild isn't set, skip the builds if the output exists.
if !$forcerebuild
	all_output_exists = true
	$configurations.each do |configuration|
		install_dir = getInstallDir(configuration, $vs_version)
		puts "Checking dir for presence of build: '" + install_dir + "'..."
		build_exists_at_dir = CMakeBuild.checkInstall(install_dir, $build_epoch)
		puts "build exists: " + build_exists_at_dir.to_s
		all_output_exists = false if !build_exists_at_dir
	end
	
	if all_output_exists
		puts "LLVM: Builds are in place, use --forcerebuild to rebuild."
		exit(0)
	end
end


Timer.time {

# Download the source.
llvm_src_dir = getLLVMSourceDownloadAndExtract()

buildLLVM(llvm_src_dir, $vs_version)

}

puts "Total build time: #{Timer.elapsedTime} s"
