#
# Builds LibreSSL on Windows, Mac or Linux.
#
# Copyright Glare Technologies Limited 2023 -
#
#
# Requirements
# ------------
# You need to have ruby installed.
#
# GLARE_CORE_LIBS environment variable should be defined, to something like c:\programming.
# This is the directory the LibreSSL directory will be made in.
#
# On Windows: You need to have 7-zip installed, with 7z.exe on your path.
#
#
#
# Manual LibreSSL build instructions
# ----------------------------------
# If you can't get this script to work for whatever reason, you can build manually using these instructions.
# These are for Windows, but should be similar (and easier) on Mac and Linux.
#
# Define GLARE_CORE_LIBS environment variable, to something like "C:\programming"
# 
# 	cd $env:GLARE_CORE_LIBS
# 	mkdir LibreSSL
# 
# Download https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-3.5.2.tar.gz, extract in LibreSSL dir, so that
# the directory $env:GLARE_CORE_LIBS/LibreSSL/libressl-3.5.2 exists and has ChangeLog etc. in it.
# 
# 
# 	cd $env:GLARE_CORE_LIBS/LibreSSL
# 	
# Building on Windows: patch the source code by replacing some LibreSSL files: (NOTE: you will need to substitute in the directory of your checked out glare-core source code here) 
# 	cp n:/glare-core/trunk/libressl_patches/3.5.2/posix_win.c  libressl-3.5.2/crypto/compat/posix_win.c
# 	cp n:/glare-core/trunk/libressl_patches/3.5.2/tls_config.c libressl-3.5.2/tls/tls_config.c
#
#
# 	mkdir libressl-3.5.2-x64-vs2022-build
# 	cd libressl-3.5.2-x64-vs2022-build
# 	cmake ../libressl-3.5.2 -DCMAKE_INSTALL_PREFIX:STRING="../libressl-3.5.2-x64-vs2022-install"
# 	
# Open .\LibreSSL.sln in Visual Studio
# Changed to Release config
# build with F7
# build INSTALL project (which is not built by default)
# close Visual Studio
# 
# cd ..
# mkdir libressl-3.5.2-x64-vs2022-build-debug
# cd libressl-3.5.2-x64-vs2022-build-debug
# cmake ../libressl-3.5.2 -DCMAKE_INSTALL_PREFIX:STRING="../libressl-3.5.2-x64-vs2022-install-debug"
# 
# Open .\LibreSSL.sln in Visual Studio
# Leave in Debug config
# build with F7
# build INSTALL project (which is not built by default)
# close Visual Studio
#

require 'fileutils'
require 'net/http'
require './script_utils.rb'
require './cmake.rb'


puts "-------------------------------------
LibreSSL build

"

$libressl_version = "3.5.2"
$vs_version = 2022
$configurations = [ :release, :debug ]
$forcerebuild = false
$build_epoch = 0


def printUsage()
	puts "Usage: build_libressl.rb [arguments]"
	puts ""
	puts "\t--release, -R\t\tSpecifies the LibreSSL release to get. Default is #{$libressl_version}."
	puts ""
	puts "\t--vsversion, -v\t\tSpecifies the Visual Studio version. Default is #{$vs_version}."
	puts ""
	puts "\t--config, -c\t\tSpecifies the config to build. Release, Debug, or Both. Default is Both."
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
			puts "Using default version: #{$libressl_version}"
		else
			$libressl_version = opt[1]
		end
	elsif opt[0] == "--vsversion" || opt[0] == "-v"
		$vs_version = opt[1].to_i
		if not [2013, 2015, 2017, 2019, 2022].include?($vs_version)
			puts "Unsupported VS version: #{opt[1]}. Skipping."
			exit 0
		end
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
	elsif opt[0] == "--forcerebuild" || opt[0] == "-f"
		$forcerebuild = true
	elsif opt[0] == "--help" || opt[0] == "-h"
		printUsage()
		exit 0
	else
		puts "Warning: Unrecognised argument: #{opt[0]}" # We can have args here from build.rb which we don't handle.
		#exit 1
	end
end


def getLibreSSLSource()
	puts "Downloading LibreSSL release #{$libressl_version}..."
	
	downloadFileHTTPSIfNotOnDisk($libressl_source_file, "https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/#{$libressl_source_file}")
	
	extractArchiveIfNotExtraced($libressl_source_file, $libressl_source_name, true)
	
	patchSource() if OS.windows?
end



# Patch source to fix an issue with closing the socket while it's doing a blocking call.
# See https://github.com/libressl-portable/portable/issues/266
# Basically we want to avoid calling read() on a socket.  Instead just return the WSA error code.
def patchSource()
	puts "Patching source.."
	src_dir = $libressl_source_name
	
	if File.exist?("#{src_dir}/glare-patch.success")
		puts "Already patched, skipping."
		return
	end

	if $libressl_version == "3.3.5" || $libressl_version == "3.5.2"
		FileUtils.cp($glare_core_dir + "/libressl_patches/" + $libressl_version + "/posix_win.c",  src_dir + "/crypto/compat/posix_win.c")
		FileUtils.cp($glare_core_dir + "/libressl_patches/" + $libressl_version + "/tls_config.c", src_dir + "/tls/tls_config.c")
	else
		puts "Don't have patches for this version of libressl! (#{$libressl_version})"
		exit 1
	end
	#puts "src_dir: #{src_dir}"

	#path = src_dir + "/crypto/compat/posix_win.c"
	#puts "Patching '#{path}'..."

	#contents = File.open(path).read()

	#puts "contents: #{contents}"

	#new_content = contents.gsub("(err == WSAENOTSOCK || err == WSAEBADF", "/*GLARE NEWCODE*/0 && (err == WSAENOTSOCK || err == WSAEBADF")

	#if new_content == contents
	#	puts "Patching failed, failed to find code to be replaced."
	#	exit(1)
	#end
	
	#FileUtils.cp($cyberspace_trunk_dir + "/libressl_patches/posix_win.c", src_dir + "/crypto/compat/posix_win.c")
	#FileUtils.cp($cyberspace_trunk_dir + "/libressl_patches/tls_config.c", src_dir + "/tls/tls_config.c")

	#File.open(path, 'w') { |file| file.write(new_content) }
	
	FileUtils.touch("#{src_dir}/glare-patch.success")

	#puts "new_content: #{new_content}"
	puts "Done patching source."
end


def getOutputDirName(configuration, dir_type, vs_version = -1)
	config_suffix = CMakeBuild.config_opts[configuration][1] # Will add a suffix like "-debug" for some configurations
	
	if OS.windows?
		if vs_version == -1
			STDERR.puts "VS version not set."
			exit 1
		end
		
		return "libressl-#{$libressl_version}-x64-vs#{vs_version}-#{dir_type}#{config_suffix}"
	else
		return "libressl-#{$libressl_version}-#{dir_type}#{config_suffix}"
	end
end


def getBuildDir(configuration, vs_version = -1)
	getOutputDirName(configuration, "build", vs_version)
end


def getInstallDir(configuration, vs_version = -1)
	getOutputDirName(configuration, "install", vs_version)
end


def buildLibreSSL(configurations, vs_version)
	configurations.each do |configuration|
		cmake_build = CMakeBuild.new
		
		cmake_build.init("LibreSSL",
			"#{$libs_libressl_dir}/#{$libressl_source_name}",
			"#{$libs_libressl_dir}/#{getBuildDir(configuration, vs_version)}",
			"#{$libs_libressl_dir}/#{getInstallDir(configuration, vs_version)}")
		
		cmake_build.configure(configuration, vs_version, "", false, OS.arm64?)
		cmake_build.build()
		cmake_build.install($build_epoch)
	end
end


$glare_core_dir = Dir.pwd() + "/.."

$libressl_source_name = "libressl-#{$libressl_version}"
$libressl_source_file = "#{$libressl_source_name}.tar.gz"

$libs_dir = ENV['GLARE_CORE_LIBS']
if $libs_dir.nil?
	STDERR.puts "GLARE_CORE_LIBS environment var not defined, please define first."
	exit(1)
end

$libs_libressl_dir = "#{$libs_dir}/LibreSSL"

FileUtils.mkdir($libs_libressl_dir, :verbose=>true) if !Dir.exist?($libs_libressl_dir)
puts "Changing dir to \"#{$libs_libressl_dir}\"."
Dir.chdir($libs_libressl_dir)



# If force rebuild isn't set, skip the builds if the output exists.
$do_build = true

if !$forcerebuild
	all_output_exists = true
	$configurations.each do |configuration|
		install_dir = getInstallDir(configuration, $vs_version)
		all_output_exists = false if !CMakeBuild.checkInstall(install_dir, $build_epoch)
	end
	
	if all_output_exists
		puts "LibreSSL: Builds are already in place, use --forcerebuild to rebuild."
		$do_build = false
	end
end

if $do_build
	Timer.time {

	# Download the source.
	getLibreSSLSource()

	buildLibreSSL($configurations, $vs_version)

	}

	puts "Total build time: #{Timer.elapsedTime} s"
end
