{ stdenv
, fetchSWH
, lib
, git
, bash
, python3
, which
, binutils
, curl
, gcc-arm-embedded
, unzip
, cacert
, firmware_name
, firmware_path
, fetchgit
}:

stdenv.mkDerivation rec {

  name = firmware_name;
  srcs = [
    (firmware_path + ("/" + firmware_name))
    (fetchSWH {
      swhid = "2d14aee3b8b21563a0900f4ac7e0c8f935a9449b";
      sha256 = "1dzhnn6jqwpd5np24zss6wd7s4malq0fixwxjshm8p973bh5i865";
      name = "RIOT";
    })
    (fetchgit {
      url = "https://github.com/STMicroelectronics/cmsis_device_f1.git";
      rev = "2948138428461c0621fd53b269862c6e6bb043ce";
      sha256 = "sha256-orGO9ol2yDoEjQyGsUzQQjix+sK2qJBeOOx/5aR0myU=";
      name = "cmsis_device_f1";
      leaveDotGit = true;
    })
  ];

  sourceRoot = "RIOT";

  APPLICATION = name;
  buildInputs =
    [ git bash python3 which curl binutils gcc-arm-embedded unzip cacert ];

  patches = [ ./issue_16359.patch ./deter.patch ];

  postPatch = ''
    for x in $(find . -executable -type f); do
      patchShebangs $x;
    done
    sed -i "s/\/usr\/bin\/env //g" makefiles/color.inc.mk
    mkdir -p cpu/stm32/include/vendor/cmsis/f1/;
    cp -a ../cmsis_device_f1/. cpu/stm32/include/vendor/cmsis/f1/;
    chmod +w -R cpu/stm32/include/vendor/cmsis/f1/
    echo '2948138428461c0621fd53b269862c6e6bb043ce' > cpu/stm32/include/vendor/cmsis/f1/.pkg-state.git-downloaded
    find ./cpu/stm32/include/vendor/ -type f -exec md5sum {} \; &> log_orig.txt
  '';

  buildPhase = ''
    export OBJCOPY="arm-none-eabi-objcopy";
    export RIOTBASE=../../
    cp -r ../${name} examples/;
    make -C examples/${name}/;
    find ./cpu/stm32/include/vendor/ -type f -exec md5sum {} \; &> log.txt
  '';

  installPhase = ''
    mkdir -p $out
    cp examples/${name}/bin/*/${APPLICATION}.elf $out/
    ls -alhR examples/${name}/
  '';

}
