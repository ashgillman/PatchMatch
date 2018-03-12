with import <nixpkgs> { };

stdenv.mkDerivation {
  name = "patch-match";
  src = ./.;
  buildInputs = [ cmakeWithGui boost itk vtk eigen ];
}
