
-- vars
local win_unixenv = false
local cygwin = false
local mingw = false
local clang_libcxx = false
local gcc_compat = false
local platform = "x32"

-- this function returns the first result of "find basepath -name filename", this is needed on some platforms to determine the include path of a library
function find_include(filename, base_path)
	if(os.is("windows") and not win_unixenv) then
		return ""
	end
	
	local proc = io.popen("find "..base_path.." -name \""..filename.."\"", "r")
	local path_names = proc:read("*a")
	proc:close()
	
	if(string.len(path_names) == 0) then
		return ""
	end
	
	local newline = string.find(path_names, "\n")
	if newline == nil then
		return ""
	end
	
	return string.sub(path_names, 0, newline-1)
end


-- actual premake info
solution "albion2"
	configurations { "Release", "Debug" }
	defines { "A2E_NET_PROTOCOL=TCP_protocol" }
	
	-- scan args
	local argc = 1
	while(_ARGS[argc] ~= nil) do
		if(_ARGS[argc] == "--env") then
			argc=argc+1
			-- check if we are building with cygwin/mingw
			if(_ARGS[argc] ~= nil and _ARGS[argc] == "cygwin") then
				cygwin = true
				win_unixenv = true
			end
			if(_ARGS[argc] ~= nil and _ARGS[argc] == "mingw") then
				mingw = true
				win_unixenv = true
			end
		end
		if(_ARGS[argc] == "--clang") then
			clang_libcxx = true
		end
		if(_ARGS[argc] == "--gcc") then
			gcc_compat = true
		end
		if(_ARGS[argc] == "--platform") then
			argc=argc+1
			if(_ARGS[argc] ~= nil) then
				platform = _ARGS[argc]
			end
		end
		argc=argc+1
	end
	
	-- os specifics
	if(not os.is("windows") or win_unixenv) then
		if(not cygwin) then
			includedirs { "/usr/include" }
		else
			includedirs { "/usr/include/w32api", "/usr/include/w32api/GL" }
		end
		includedirs { "/usr/local/include", "/usr/include/libxml2", "/usr/include/libxml",
					  "/usr/include/freetype2", "/usr/local/include/freetype2", }
		buildoptions { "-Wall -x c++ -std=c++0x -Wno-trigraphs -Wreturn-type -Wunused-variable" }
		
		if(clang_libcxx) then
			buildoptions { "-stdlib=libc++ -integrated-as" }
			buildoptions { "-Wno-delete-non-virtual-dtor -Wno-overloaded-virtual -Wno-unreachable-code -Wdangling-else" }
			linkoptions { "-fvisibility=default" }
			if(not win_unixenv) then
				linkoptions { "-stdlib=libc++" }
			else
				linkoptions { "-lc++.dll" }
			end
		end
		
		if(gcc_compat) then
			buildoptions { "-Wno-strict-aliasing" }
		end
	end
	
	if(win_unixenv) then
		-- only works with gnu++0x for now ...
		buildoptions { "-std=gnu++0x" }
		defines { "WIN_UNIXENV" }
		if(cygwin) then
			defines { "CYGWIN" }
		end
		if(mingw) then
			defines { "__WINDOWS__", "MINGW" }
			--includedirs { "/mingw/include/GL", "/mingw/include/CL" }
			includedirs { "/mingw/include" }
			libdirs { "/usr/lib", "/usr/local/lib" }
			buildoptions { "-Wno-unknown-pragmas" }
		end
	end
	
	if(os.is("linux") or os.is("bsd") or win_unixenv) then
		links { "OpenCL" }
		libdirs { os.findlib("GL"), os.findlib("xml2"), os.findlib("OpenCL") }
		if(not win_unixenv) then
			links { "GL", "SDL2_image", "Xxf86vm", "xml2" }
			libdirs { os.findlib("SDL2"), os.findlib("SDL2_image"), os.findlib("Xxf86vm") }
			buildoptions { "`sdl2-config --cflags`" }
			linkoptions { "`sdl2-config --libs`" }
		elseif(cygwin) then
			-- link against windows opengl libs on cygwin
			links { "opengl32", "SDL2_image.dll", "xml2" }
			libdirs { "/lib/w32api" }
			buildoptions { "`sdl2-config --cflags | sed -E 's/-Dmain=SDL_main//g'`" }
			linkoptions { "`sdl2-config --libs | sed -E 's/(-lmingw32|-mwindows)//g'`" }
		elseif(mingw) then
			-- link against windows opengl libs on mingw
			links { "opengl32", "SDL2_image", "libxml2" }
			buildoptions { "`sdl2-config --cflags | sed -E 's/-Dmain=SDL_main//g'`" }
			linkoptions { "`sdl2-config --libs`" }
		end
		includedirs { "/usr/include/SDL2", "/usr/local/include/SDL2" }

		if(gcc_compat) then
			if(not mingw) then
				defines { "_GLIBCXX__PTHREADS" }
			end
			defines { "_GLIBCXX_USE_NANOSLEEP" }
		end
	end

	-- prefer system platform
	if(platform == "x64") then
		platforms { "x64", "x32" }
	else
		platforms { "x32", "x64" }
	end
	
	configuration { "x64" }
		defines { "PLATFORM_X64" }

	configuration { "x32" }
		defines { "PLATFORM_X86" }
	

	configuration "Release"
		defines { "NDEBUG" }
		flags { "Optimize" }
		if(not os.is("windows") or win_unixenv) then
			buildoptions { " -O3 -ffast-math" }
		end

	configuration "Debug"
		defines { "DEBUG", "A2E_DEBUG" }
		flags { "Symbols" }
		if(not os.is("windows") or win_unixenv) then
			buildoptions { " -gdwarf-2" }
		end

------------------------------------------------
-- the engine
project "a2elight"
	targetname "a2elight"
	kind "SharedLib"
	language "C++"

	files { "a2elight/src/**.h", "a2elight/src/**.hpp", "a2elight/src/**.cpp" }

	basedir "a2elight/src"
	targetdir "a2elight/lib"
	includedirs { "a2elight/src/",
				  "a2elight/src/cl/",
				  "a2elight/src/core/",
				  "a2elight/src/gui/",
				  "a2elight/src/gui/objects/",
				  "a2elight/src/gui/style/",
				  "a2elight/src/particle/",
				  "a2elight/src/rendering/",
				  "a2elight/src/rendering/renderer/",
				  "a2elight/src/rendering/renderer/gl3/",
				  "a2elight/src/scene/",
				  "a2elight/src/scene/model/" }
	
	if(not os.is("windows") or win_unixenv) then
		prebuildcommands { "./build_version.sh" }
		if(mingw) then
			postbuildcommands { "./../install.sh" }
		end
	else
		prebuildcommands { "./build_version.bat" }
	end
	
	configuration { "x32" }
		if(os.is("windows")) then
			targetdir "a2elight/lib/x86"
		end
	
	configuration { "x64" }
		if(os.is("windows")) then
			targetdir "a2elight/lib/x64"
		end
	
	configuration "Release"
		targetname "a2elight"
	configuration "Debug"
		targetname "a2elightd"

------------------------------------------------
-- bus project

project "kinicles"
	targetname "kinicles"
	kind "ConsoleApp"
	language "C++"

	files { "kinicles/src/**.h", "kinicles/src/**.cpp" }
	if(win_unixenv) then
		files { "kinicles/win/windows.rc" }
	end

	basedir "kinicles"
	targetdir "kinicles/bin"

	includedirs { "/usr/local/include/a2elight", "/usr/include/ni", "/usr/include/nite" }
	libdirs { "/usr/local/lib", "OpenNI" }
	links { "OpenNI", "openal", "fftw3f", "m" }
	if(not win_unixenv) then -- threads are included in the normal .a/.def file
		links { "fftw3f_threads" }
	end
	
	if((os.is("linux") or os.is("bsd")) and not win_unixenv) then
		defines { "linux" }
		if(platform == "x64") then
			defines { "__x86_64__" }
		else
			defines { "i386" }
		end
	end
	
	if(gcc_compat and win_unixenv) then
		-- silence gcc warnings in openni/nite headers
		buildoptions { "-Wno-reorder", "-Wno-unused-but-set-variable" }
	end
	buildoptions { "-Wno-reorder" } -- silences openni warnings ...

	configuration "Release"
		links { "a2elight" }
		targetname "kinicles"
	configuration "Debug"
		links { "a2elightd" }
		targetname "kiniclesd"

