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
            '';
          };
        });
    };
}
