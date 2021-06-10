{ stdenv, fetchSWH, lib, git, bash, python3, which, binutils, curl, gcc-arm-embedded, unzip, cacert, firmware_name, firmware_path, outputHash }:

stdenv.mkDerivation rec {
  inherit outputHash;

  name = firmware_name;
  srcs = [
    (firmware_path + ("/" + firmware_name))
    (fetchSWH {
      swhid = "2d14aee3b8b21563a0900f4ac7e0c8f935a9449b";
      sha256 = "1dzhnn6jqwpd5np24zss6wd7s4malq0fixwxjshm8p973bh5i865";
      name = "RIOT";
    })
  ];

  sourceRoot = "RIOT";

  APPLICATION = name;
  buildInputs = [
    git
    bash
    python3
    which
    curl
    binutils
    gcc-arm-embedded
    unzip
    cacert
  ];

  postPatch = ''
    for x in $(find . -executable -type f); do
      patchShebangs $x;
    done
    sed -i "s/\/usr\/bin\/env //g" makefiles/color.inc.mk
  '';

  buildPhase = ''
    export OBJCOPY="arm-none-eabi-objcopy";
    export RIOTBASE=../../
    cp -r ../${name} examples/;
    make -C examples/${name}/;
  '';

  installPhase = ''
    mkdir -p $out
    cp examples/${name}/bin/*/${APPLICATION}.elf $out/
    ls -alhR examples/${name}/
  '';

  outputHashAlgo = "sha256";
  outputHashMode = "recursive";
}
