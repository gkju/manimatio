{
  description = "Manimatio flake";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f system);
    in {
      devShells = forAllSystems (system:
        let pkgs = import nixpkgs { inherit system; };
        in {
          default = pkgs.mkShell {
            packages = with pkgs; [
              cmake ninja
              clang lld llvm clang-tools
              rustc cargo
              git pkg-config
              python3
              vcpkg
            ];

            shellHook = ''
              echo "Dev shell: clang=$(clang --version | head -n1)"
              echo "Dev shell: rustc=$(rustc -V)"
              echo "Use: cmake --preset nix-release-lto"
              export CMAKE_EXPORT_COMPILE_COMMANDS=1

              export VCPKG_ROOT="${pkgs.vcpkg}/share/vcpkg"
              export VCPKG_DOWNLOADS="$PWD/.vcpkg/downloads"
              export VCPKG_DEFAULT_BINARY_CACHE="$PWD/.vcpkg/bincache"

              PRESET_BUILD_DIR="build/nix-release-lto" 
  
              if [ ! -f compile_commands.json ]; then
                echo "First-time setup: Bootstrapping CMake with vcpkg for clangd..."
                
                cmake --preset nix-release-lto 
                
                if [ -f "$PRESET_BUILD_DIR/compile_commands.json" ]; then
                  ln -sf "$PRESET_BUILD_DIR/compile_commands.json" .
                  echo "compile_commands.json linked to project root!"
                fi
              fi
            '';
          };
        });
    };
}
