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
        stdenv = pkgs.clangStdenv;
        in {
          default = pkgs.mkShell.override { inherit stdenv; } {
            packages = with pkgs; [
              cmake ninja
              stdenv.cc lld llvm clang-tools mold
              rustc cargo
              git pkg-config
              python3
              just

              autoconf automake libtool autoconf-archive m4 
              gperf flex bison gettext

              util-linux
              mesa libGL
              libx11 libxcursor libxinerama libxi
              openssl
            ];

            shellHook = ''
              export VCPKG_ROOT="$PWD/vcpkg"
              export VCPKG_DOWNLOADS="$PWD/.vcpkg/downloads"
              export VCPKG_DEFAULT_BINARY_CACHE="$PWD/.vcpkg/bincache"
              export PATH=$VCPKG_ROOT:$PATH
              
              export VCPKG_FORCE_SYSTEM_BINARIES=1
              export STDCXX_PATH="${pkgs.stdenv.cc.cc.lib}/lib"
              export LD_LIBRARY_PATH="$STDCXX_PATH:$LD_LIBRARY_PATH"
              export NIX_LDFLAGS="-L$STDCXX_PATH $NIX_LDFLAGS"
              mkdir -p "$VCPKG_DOWNLOADS" "$VCPKG_DEFAULT_BINARY_CACHE"
              if [ ! -f "$VCPKG_ROOT/bootstrap-vcpkg.sh" ]; then
                echo "ERROR: $VCPKG_ROOT/bootstrap-vcpkg.sh not found."
                echo "Did you forget to checkout the vcpkg submodule?"
                exit 1
              fi
              
              if [ ! -f "$VCPKG_ROOT/vcpkg" ]; then
                echo "Bootstrapping vcpkg submodule..."
                chmod +x "$VCPKG_ROOT/bootstrap-vcpkg.sh"
                "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
              fi

              PRESET_BUILD_DIR="build/nix-dev"

              echo "Dev shell ready. Use 'just reconfigure' to start the build."
            '';
          };
        });
    };
}
