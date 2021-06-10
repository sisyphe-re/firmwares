{
  description = "A flake to build firmwares for RIOT OS";

  inputs.nixpkgs.url = github:NixOS/nixpkgs/nixos-20.09;
  inputs.fetchSWH.url = github:sisyphe-re/fetchSWH;

  outputs = { self, nixpkgs, fetchSWH }: {
    packages.x86_64-linux.gnrc_border_router =
      with import nixpkgs { system = "x86_64-linux"; };
      callPackage ./firmware_builder { fetchSWH = fetchSWH.lib.fetchSWH; firmware_name = "gnrc_border_router"; firmware_path = ./src; outputHash = "sha256-1kQMis5kXJEelVyBI0ZHbtDGfWVvcHGv4bxsDHto1Z4="; };

    packages.x86_64-linux.gnrc_networking =
      with import nixpkgs { system = "x86_64-linux"; };
      callPackage ./firmware_builder { fetchSWH = fetchSWH.lib.fetchSWH; firmware_name = "gnrc_networking"; firmware_path = ./src; outputHash = "sha256-RSmEAqQ5Ohr9xkbHwAr/HpFAYlSzyAcIbQYvwmw7R0E="; };
  };
}
