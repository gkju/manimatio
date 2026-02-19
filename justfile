# Set the default preset used for local dev
preset := "nix-dev"
build_dir := "build/" + preset

# List all available commands
default:
    @just --list

# Build everything
build:
    cmake --build --preset {{preset}}

# Build and run all unit tests
test:
    cmake --build --preset {{preset}} --target tests
    ctest --test-dir {{build_dir}} --output-on-failure

# Build and run a specific module's test (e.g., `just test-mod math`)
test-mod module:
    cmake --build --preset {{preset}} --target test_cxxlib_{{module}}
    ctest --test-dir {{build_dir}} -R "test_cxxlib_{{module}}" --output-on-failure

# Build all snippets
snippets:
    cmake --build --preset {{preset}} --target snippets

# Build and run a specific snippet (e.g., `just snippet math test`)
snippet module name:
    cmake --build --preset {{preset}} --target snippet_cxxlib_{{module}}_{{name}}
    ./{{build_dir}}/snippets/snippet_cxxlib_{{module}}_{{name}}

# Reconfigure the CMake project
reconfigure:
    cmake --preset {{preset}}

# Clean the build directory
clean:
    rm -rf {{build_dir}}