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
        let 
          pkgs = import nixpkgs { inherit system; };
          stdenv = pkgs.clangStdenv;

	        clangScanDepsWrapper = pkgs.writeShellScriptBin "clang-scan-deps" ''
            # Dynamically extract the hidden includes at RUNTIME so Ninja can't strip them
            INCLUDES=$(clang++ -E -x c++ - -v 2>&1 </dev/null | awk '/#include <...>/ {flag=1; next} /End of search/ {flag=0} flag {printf "-isystem %s ", $1}')
            
            # Forward everything to the real scanner
            exec ${pkgs.clang-tools}/bin/clang-scan-deps "$@" $INCLUDES
          '';

          # Base packages
          commonPackages = with pkgs; [
            cmake ninja
            stdenv.cc lld llvm clang-tools
            clangScanDepsWrapper
            rustc cargo
            git pkg-config
            python3
            just
            autoconf automake libtool autoconf-archive m4 
            gperf flex bison gettext
            openssl
          ];

          # Linux-only packages
          linuxPackages = with pkgs; lib.optionals stdenv.isLinux [
            mold
            util-linux
            mesa libGL
            xorg.libX11 xorg.libXcursor xorg.libXinerama xorg.libXi
          ];

          # macOS-only packages
	        darwinPackages = with pkgs; lib.optionals stdenv.isDarwin [
            apple-sdk
          ];
        in {
          default = pkgs.mkShell.override { inherit stdenv; } {
            packages = commonPackages ++ linuxPackages ++ darwinPackages;

            shellHook = ''
              export VCPKG_ROOT="$PWD/vcpkg"
              export VCPKG_DOWNLOADS="$PWD/.vcpkg/downloads"
              export VCPKG_DEFAULT_BINARY_CACHE="$PWD/.vcpkg/bincache"
              export PATH=$VCPKG_ROOT:$PATH
              
              export VCPKG_FORCE_SYSTEM_BINARIES=1
              
              mkdir -p "$VCPKG_DOWNLOADS" "$VCPKG_DEFAULT_BINARY_CACHE"
              
              # Isolate the standard library hack to Linux only
              ${pkgs.lib.optionalString pkgs.stdenv.isLinux ''
                export STDCXX_PATH="${pkgs.stdenv.cc.cc.lib}/lib"
                export LD_LIBRARY_PATH="$STDCXX_PATH:$LD_LIBRARY_PATH"
                export NIX_LDFLAGS="-L$STDCXX_PATH $NIX_LDFLAGS"
              ''}

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

              mkdir -p .direnv
              ln -sf "${pkgs.clang-tools}/bin/clangd" .direnv/clangd

              PRESET_BUILD_DIR="build/nix-dev"

              echo "Dev shell ready. Use 'just reconfigure' to start the build."
            '';
          };
        });
    };
}
