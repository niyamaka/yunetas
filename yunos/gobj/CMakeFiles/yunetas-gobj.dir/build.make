# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.25

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /yuneta/development/yuneta/yunetas

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /yuneta/development/yuneta/yunetas/yunos

# Include any dependencies generated for this target.
include gobj/CMakeFiles/yunetas-gobj.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include gobj/CMakeFiles/yunetas-gobj.dir/compiler_depend.make

# Include the progress variables for this target.
include gobj/CMakeFiles/yunetas-gobj.dir/progress.make

# Include the compile flags for this target's objects.
include gobj/CMakeFiles/yunetas-gobj.dir/flags.make

gobj/CMakeFiles/yunetas-gobj.dir/src/00_http_parser.c.o: gobj/CMakeFiles/yunetas-gobj.dir/flags.make
gobj/CMakeFiles/yunetas-gobj.dir/src/00_http_parser.c.o: /yuneta/development/yuneta/yunetas/gobj/src/00_http_parser.c
gobj/CMakeFiles/yunetas-gobj.dir/src/00_http_parser.c.o: gobj/CMakeFiles/yunetas-gobj.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/yuneta/development/yuneta/yunetas/yunos/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object gobj/CMakeFiles/yunetas-gobj.dir/src/00_http_parser.c.o"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT gobj/CMakeFiles/yunetas-gobj.dir/src/00_http_parser.c.o -MF CMakeFiles/yunetas-gobj.dir/src/00_http_parser.c.o.d -o CMakeFiles/yunetas-gobj.dir/src/00_http_parser.c.o -c /yuneta/development/yuneta/yunetas/gobj/src/00_http_parser.c

gobj/CMakeFiles/yunetas-gobj.dir/src/00_http_parser.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/yunetas-gobj.dir/src/00_http_parser.c.i"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /yuneta/development/yuneta/yunetas/gobj/src/00_http_parser.c > CMakeFiles/yunetas-gobj.dir/src/00_http_parser.c.i

gobj/CMakeFiles/yunetas-gobj.dir/src/00_http_parser.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/yunetas-gobj.dir/src/00_http_parser.c.s"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /yuneta/development/yuneta/yunetas/gobj/src/00_http_parser.c -o CMakeFiles/yunetas-gobj.dir/src/00_http_parser.c.s

gobj/CMakeFiles/yunetas-gobj.dir/src/ghttp_parser.c.o: gobj/CMakeFiles/yunetas-gobj.dir/flags.make
gobj/CMakeFiles/yunetas-gobj.dir/src/ghttp_parser.c.o: /yuneta/development/yuneta/yunetas/gobj/src/ghttp_parser.c
gobj/CMakeFiles/yunetas-gobj.dir/src/ghttp_parser.c.o: gobj/CMakeFiles/yunetas-gobj.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/yuneta/development/yuneta/yunetas/yunos/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object gobj/CMakeFiles/yunetas-gobj.dir/src/ghttp_parser.c.o"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT gobj/CMakeFiles/yunetas-gobj.dir/src/ghttp_parser.c.o -MF CMakeFiles/yunetas-gobj.dir/src/ghttp_parser.c.o.d -o CMakeFiles/yunetas-gobj.dir/src/ghttp_parser.c.o -c /yuneta/development/yuneta/yunetas/gobj/src/ghttp_parser.c

gobj/CMakeFiles/yunetas-gobj.dir/src/ghttp_parser.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/yunetas-gobj.dir/src/ghttp_parser.c.i"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /yuneta/development/yuneta/yunetas/gobj/src/ghttp_parser.c > CMakeFiles/yunetas-gobj.dir/src/ghttp_parser.c.i

gobj/CMakeFiles/yunetas-gobj.dir/src/ghttp_parser.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/yunetas-gobj.dir/src/ghttp_parser.c.s"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /yuneta/development/yuneta/yunetas/gobj/src/ghttp_parser.c -o CMakeFiles/yunetas-gobj.dir/src/ghttp_parser.c.s

gobj/CMakeFiles/yunetas-gobj.dir/src/gobj.c.o: gobj/CMakeFiles/yunetas-gobj.dir/flags.make
gobj/CMakeFiles/yunetas-gobj.dir/src/gobj.c.o: /yuneta/development/yuneta/yunetas/gobj/src/gobj.c
gobj/CMakeFiles/yunetas-gobj.dir/src/gobj.c.o: gobj/CMakeFiles/yunetas-gobj.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/yuneta/development/yuneta/yunetas/yunos/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building C object gobj/CMakeFiles/yunetas-gobj.dir/src/gobj.c.o"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT gobj/CMakeFiles/yunetas-gobj.dir/src/gobj.c.o -MF CMakeFiles/yunetas-gobj.dir/src/gobj.c.o.d -o CMakeFiles/yunetas-gobj.dir/src/gobj.c.o -c /yuneta/development/yuneta/yunetas/gobj/src/gobj.c

gobj/CMakeFiles/yunetas-gobj.dir/src/gobj.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/yunetas-gobj.dir/src/gobj.c.i"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /yuneta/development/yuneta/yunetas/gobj/src/gobj.c > CMakeFiles/yunetas-gobj.dir/src/gobj.c.i

gobj/CMakeFiles/yunetas-gobj.dir/src/gobj.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/yunetas-gobj.dir/src/gobj.c.s"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /yuneta/development/yuneta/yunetas/gobj/src/gobj.c -o CMakeFiles/yunetas-gobj.dir/src/gobj.c.s

gobj/CMakeFiles/yunetas-gobj.dir/src/istream.c.o: gobj/CMakeFiles/yunetas-gobj.dir/flags.make
gobj/CMakeFiles/yunetas-gobj.dir/src/istream.c.o: /yuneta/development/yuneta/yunetas/gobj/src/istream.c
gobj/CMakeFiles/yunetas-gobj.dir/src/istream.c.o: gobj/CMakeFiles/yunetas-gobj.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/yuneta/development/yuneta/yunetas/yunos/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building C object gobj/CMakeFiles/yunetas-gobj.dir/src/istream.c.o"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT gobj/CMakeFiles/yunetas-gobj.dir/src/istream.c.o -MF CMakeFiles/yunetas-gobj.dir/src/istream.c.o.d -o CMakeFiles/yunetas-gobj.dir/src/istream.c.o -c /yuneta/development/yuneta/yunetas/gobj/src/istream.c

gobj/CMakeFiles/yunetas-gobj.dir/src/istream.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/yunetas-gobj.dir/src/istream.c.i"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /yuneta/development/yuneta/yunetas/gobj/src/istream.c > CMakeFiles/yunetas-gobj.dir/src/istream.c.i

gobj/CMakeFiles/yunetas-gobj.dir/src/istream.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/yunetas-gobj.dir/src/istream.c.s"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /yuneta/development/yuneta/yunetas/gobj/src/istream.c -o CMakeFiles/yunetas-gobj.dir/src/istream.c.s

gobj/CMakeFiles/yunetas-gobj.dir/src/parse_url.c.o: gobj/CMakeFiles/yunetas-gobj.dir/flags.make
gobj/CMakeFiles/yunetas-gobj.dir/src/parse_url.c.o: /yuneta/development/yuneta/yunetas/gobj/src/parse_url.c
gobj/CMakeFiles/yunetas-gobj.dir/src/parse_url.c.o: gobj/CMakeFiles/yunetas-gobj.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/yuneta/development/yuneta/yunetas/yunos/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Building C object gobj/CMakeFiles/yunetas-gobj.dir/src/parse_url.c.o"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT gobj/CMakeFiles/yunetas-gobj.dir/src/parse_url.c.o -MF CMakeFiles/yunetas-gobj.dir/src/parse_url.c.o.d -o CMakeFiles/yunetas-gobj.dir/src/parse_url.c.o -c /yuneta/development/yuneta/yunetas/gobj/src/parse_url.c

gobj/CMakeFiles/yunetas-gobj.dir/src/parse_url.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/yunetas-gobj.dir/src/parse_url.c.i"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /yuneta/development/yuneta/yunetas/gobj/src/parse_url.c > CMakeFiles/yunetas-gobj.dir/src/parse_url.c.i

gobj/CMakeFiles/yunetas-gobj.dir/src/parse_url.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/yunetas-gobj.dir/src/parse_url.c.s"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /yuneta/development/yuneta/yunetas/gobj/src/parse_url.c -o CMakeFiles/yunetas-gobj.dir/src/parse_url.c.s

gobj/CMakeFiles/yunetas-gobj.dir/src/gobj_environment.c.o: gobj/CMakeFiles/yunetas-gobj.dir/flags.make
gobj/CMakeFiles/yunetas-gobj.dir/src/gobj_environment.c.o: /yuneta/development/yuneta/yunetas/gobj/src/gobj_environment.c
gobj/CMakeFiles/yunetas-gobj.dir/src/gobj_environment.c.o: gobj/CMakeFiles/yunetas-gobj.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/yuneta/development/yuneta/yunetas/yunos/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Building C object gobj/CMakeFiles/yunetas-gobj.dir/src/gobj_environment.c.o"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT gobj/CMakeFiles/yunetas-gobj.dir/src/gobj_environment.c.o -MF CMakeFiles/yunetas-gobj.dir/src/gobj_environment.c.o.d -o CMakeFiles/yunetas-gobj.dir/src/gobj_environment.c.o -c /yuneta/development/yuneta/yunetas/gobj/src/gobj_environment.c

gobj/CMakeFiles/yunetas-gobj.dir/src/gobj_environment.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/yunetas-gobj.dir/src/gobj_environment.c.i"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /yuneta/development/yuneta/yunetas/gobj/src/gobj_environment.c > CMakeFiles/yunetas-gobj.dir/src/gobj_environment.c.i

gobj/CMakeFiles/yunetas-gobj.dir/src/gobj_environment.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/yunetas-gobj.dir/src/gobj_environment.c.s"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /yuneta/development/yuneta/yunetas/gobj/src/gobj_environment.c -o CMakeFiles/yunetas-gobj.dir/src/gobj_environment.c.s

gobj/CMakeFiles/yunetas-gobj.dir/src/log_udp_handler.c.o: gobj/CMakeFiles/yunetas-gobj.dir/flags.make
gobj/CMakeFiles/yunetas-gobj.dir/src/log_udp_handler.c.o: /yuneta/development/yuneta/yunetas/gobj/src/log_udp_handler.c
gobj/CMakeFiles/yunetas-gobj.dir/src/log_udp_handler.c.o: gobj/CMakeFiles/yunetas-gobj.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/yuneta/development/yuneta/yunetas/yunos/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "Building C object gobj/CMakeFiles/yunetas-gobj.dir/src/log_udp_handler.c.o"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT gobj/CMakeFiles/yunetas-gobj.dir/src/log_udp_handler.c.o -MF CMakeFiles/yunetas-gobj.dir/src/log_udp_handler.c.o.d -o CMakeFiles/yunetas-gobj.dir/src/log_udp_handler.c.o -c /yuneta/development/yuneta/yunetas/gobj/src/log_udp_handler.c

gobj/CMakeFiles/yunetas-gobj.dir/src/log_udp_handler.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/yunetas-gobj.dir/src/log_udp_handler.c.i"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /yuneta/development/yuneta/yunetas/gobj/src/log_udp_handler.c > CMakeFiles/yunetas-gobj.dir/src/log_udp_handler.c.i

gobj/CMakeFiles/yunetas-gobj.dir/src/log_udp_handler.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/yunetas-gobj.dir/src/log_udp_handler.c.s"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /yuneta/development/yuneta/yunetas/gobj/src/log_udp_handler.c -o CMakeFiles/yunetas-gobj.dir/src/log_udp_handler.c.s

gobj/CMakeFiles/yunetas-gobj.dir/src/helpers.c.o: gobj/CMakeFiles/yunetas-gobj.dir/flags.make
gobj/CMakeFiles/yunetas-gobj.dir/src/helpers.c.o: /yuneta/development/yuneta/yunetas/gobj/src/helpers.c
gobj/CMakeFiles/yunetas-gobj.dir/src/helpers.c.o: gobj/CMakeFiles/yunetas-gobj.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/yuneta/development/yuneta/yunetas/yunos/CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "Building C object gobj/CMakeFiles/yunetas-gobj.dir/src/helpers.c.o"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT gobj/CMakeFiles/yunetas-gobj.dir/src/helpers.c.o -MF CMakeFiles/yunetas-gobj.dir/src/helpers.c.o.d -o CMakeFiles/yunetas-gobj.dir/src/helpers.c.o -c /yuneta/development/yuneta/yunetas/gobj/src/helpers.c

gobj/CMakeFiles/yunetas-gobj.dir/src/helpers.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/yunetas-gobj.dir/src/helpers.c.i"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /yuneta/development/yuneta/yunetas/gobj/src/helpers.c > CMakeFiles/yunetas-gobj.dir/src/helpers.c.i

gobj/CMakeFiles/yunetas-gobj.dir/src/helpers.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/yunetas-gobj.dir/src/helpers.c.s"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && /usr/bin/clang $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /yuneta/development/yuneta/yunetas/gobj/src/helpers.c -o CMakeFiles/yunetas-gobj.dir/src/helpers.c.s

# Object files for target yunetas-gobj
yunetas__gobj_OBJECTS = \
"CMakeFiles/yunetas-gobj.dir/src/00_http_parser.c.o" \
"CMakeFiles/yunetas-gobj.dir/src/ghttp_parser.c.o" \
"CMakeFiles/yunetas-gobj.dir/src/gobj.c.o" \
"CMakeFiles/yunetas-gobj.dir/src/istream.c.o" \
"CMakeFiles/yunetas-gobj.dir/src/parse_url.c.o" \
"CMakeFiles/yunetas-gobj.dir/src/gobj_environment.c.o" \
"CMakeFiles/yunetas-gobj.dir/src/log_udp_handler.c.o" \
"CMakeFiles/yunetas-gobj.dir/src/helpers.c.o"

# External object files for target yunetas-gobj
yunetas__gobj_EXTERNAL_OBJECTS =

gobj/libyunetas-gobj.a: gobj/CMakeFiles/yunetas-gobj.dir/src/00_http_parser.c.o
gobj/libyunetas-gobj.a: gobj/CMakeFiles/yunetas-gobj.dir/src/ghttp_parser.c.o
gobj/libyunetas-gobj.a: gobj/CMakeFiles/yunetas-gobj.dir/src/gobj.c.o
gobj/libyunetas-gobj.a: gobj/CMakeFiles/yunetas-gobj.dir/src/istream.c.o
gobj/libyunetas-gobj.a: gobj/CMakeFiles/yunetas-gobj.dir/src/parse_url.c.o
gobj/libyunetas-gobj.a: gobj/CMakeFiles/yunetas-gobj.dir/src/gobj_environment.c.o
gobj/libyunetas-gobj.a: gobj/CMakeFiles/yunetas-gobj.dir/src/log_udp_handler.c.o
gobj/libyunetas-gobj.a: gobj/CMakeFiles/yunetas-gobj.dir/src/helpers.c.o
gobj/libyunetas-gobj.a: gobj/CMakeFiles/yunetas-gobj.dir/build.make
gobj/libyunetas-gobj.a: gobj/CMakeFiles/yunetas-gobj.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/yuneta/development/yuneta/yunetas/yunos/CMakeFiles --progress-num=$(CMAKE_PROGRESS_9) "Linking C static library libyunetas-gobj.a"
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && $(CMAKE_COMMAND) -P CMakeFiles/yunetas-gobj.dir/cmake_clean_target.cmake
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/yunetas-gobj.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
gobj/CMakeFiles/yunetas-gobj.dir/build: gobj/libyunetas-gobj.a
.PHONY : gobj/CMakeFiles/yunetas-gobj.dir/build

gobj/CMakeFiles/yunetas-gobj.dir/clean:
	cd /yuneta/development/yuneta/yunetas/yunos/gobj && $(CMAKE_COMMAND) -P CMakeFiles/yunetas-gobj.dir/cmake_clean.cmake
.PHONY : gobj/CMakeFiles/yunetas-gobj.dir/clean

gobj/CMakeFiles/yunetas-gobj.dir/depend:
	cd /yuneta/development/yuneta/yunetas/yunos && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /yuneta/development/yuneta/yunetas /yuneta/development/yuneta/yunetas/gobj /yuneta/development/yuneta/yunetas/yunos /yuneta/development/yuneta/yunetas/yunos/gobj /yuneta/development/yuneta/yunetas/yunos/gobj/CMakeFiles/yunetas-gobj.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : gobj/CMakeFiles/yunetas-gobj.dir/depend

