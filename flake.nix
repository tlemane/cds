{
  inputs = {
    nixpkgs = {
      url = "github:nixos/nixpkgs/nixos-unstable";
    };
    flake-utils = {
      url = "github:numtide/flake-utils";
    };

  };
  outputs = { self, nixpkgs, flake-utils, ... }: flake-utils.lib.eachSystem [
    "x86_64-linux"
  ] (system:
    let
      pkgs = import nixpkgs {
        inherit system;
      };

      BuildInputs = [
        pkgs.gcc15
        pkgs.llvmPackages_19.clang-tools
        pkgs.cmake
        pkgs.pre-commit
      ];

    in {
      devShell = pkgs.mkShell {
        name = "cds";
        buildInputs = BuildInputs;
      };
    }
  );
}
