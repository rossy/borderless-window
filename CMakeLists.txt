
cmake_minimum_required(VERSION 3.20)

project(borderless-window 
	VERSION 1.0
	LANGUAGES C
)

add_executable(${PROJECT_NAME} WIN32 borderless-window.c)

target_link_libraries(${PROJECT_NAME} PRIVATE
	UxTheme.lib
	Dwmapi.lib
) 