cmake_minimum_required(VERSION 3.16)
include(${CMAKE_CURRENT_LIST_DIR}/cpp.cmake)

if(NOT DEFINED install_dir)
	set(install_dir bin)
endif()

function(set_project_to_run_from_install)
	set(vcxproj_user_file "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.vcxproj.user")
	get_target_property(output_name ${PROJECT_NAME} OUTPUT_NAME)

	if(NOT EXISTS ${vcxproj_user_file})
		file(WRITE ${vcxproj_user_file}
			"<?xml version=\"1.0\" encoding=\"utf-8\"?>
				<Project ToolsVersion=\"Current\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">
				  <PropertyGroup>
					<LocalDebuggerWorkingDirectory>${CMAKE_INSTALL_PREFIX}/bin</LocalDebuggerWorkingDirectory>
					<DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>
					<LocalDebuggerCommand>${CMAKE_INSTALL_PREFIX}/bin/${output_name}.exe</LocalDebuggerCommand>
				  </PropertyGroup>
				</Project>")
	endif()
endfunction()


function(deploy_qt)
	cmake_parse_arguments(PARSE_ARGV 0 deploy_qt "NOPLUGINS" "" "BINARIES")

	set(qt5bin ${Qt5Core_DIR}/../../../bin)
	find_program(WINDEPLOYQT_COMMAND windeployqt PATHS ${qt5bin} NO_DEFAULT_PATH)

	set(args
		"--no-translations \
		--verbose 0 \
		--webenginewidgets \
		--websockets \
		--libdir dlls \
		--no-compiler-runtime")

	if(${deploy_qt_NOPLUGINS})
		set(args "${args} --no-plugins")
	else()
		set(args "${args} --plugindir qtplugins")
	endif()

	set(bin "${CMAKE_INSTALL_PREFIX}/bin")

	set(deploys "")
	foreach(binary ${deploy_qt_BINARIES})
		set(deploys "${deploys}
			EXECUTE_PROCESS(
				COMMAND ${qt5bin}/windeployqt.exe ${args} ${binary}
				WORKING_DIRECTORY ${bin})")
	endforeach()

	install(CODE "
		${deploys}

		file(REMOVE_RECURSE ${bin}/platforms)
		file(REMOVE_RECURSE ${bin}/styles)
		file(REMOVE_RECURSE ${bin}/dlls/imageformats)
	")

	if(NOT ${deploy_qt_NOPLUGINS})
		install(CODE "
			file(RENAME ${bin}/qtplugins/platforms ${bin}/platforms)
			file(RENAME ${bin}/qtplugins/styles ${bin}/styles)
			file(RENAME ${bin}/qtplugins/imageformats ${bin}/dlls/imageformats)
			file(REMOVE_RECURSE ${bin}/qtplugins)
		")
	endif()
endfunction()


macro(do_project)
	do_cpp_project()
endmacro()


macro(do_src)
	cpp_pre_target()

	add_executable(${PROJECT_NAME} WIN32 ${input_files})
	set_project_to_run_from_install()

	if(DEFINED executable_name)
		set_target_properties(${PROJECT_NAME} PROPERTIES
			OUTPUT_NAME ${executable_name})
	endif()

	cpp_post_target()

	install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${install_dir})
	install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION pdb)
endmacro()
