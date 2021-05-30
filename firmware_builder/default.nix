{ stdenv, lib, git, bash, python3, which, binutils, curl, gcc-arm-embedded, unzip, cacert, firmware_name, firmware_path, RIOT }:

stdenv.mkDerivation rec {
  name = firmware_name;
  src = firmware_path + ("/" + firmware_name);
  APPLICATION = name;
  RIOTBASE = RIOT;
  buildInputs = [
    RIOT
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

  buildPhase = ''
    OBJCOPY="arm-none-eabi-objcopy";
    make
  '';

  installPhase = ''
    mkdir -p $out
    cp bin/*/${APPLICATION}.elf $out/
  '';
}
