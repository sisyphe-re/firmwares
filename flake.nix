
{
  description = "A flake to build firmwares for RIOT OS";

  inputs.nixpkgs.url = github:NixOS/nixpkgs/nixos-20.09;
  inputs.RIOT.url = github:sisyphe-re/RIOT;

  outputs = { self, nixpkgs, RIOT }: {
    packages.x86_64-linux.gnrc_border_router =
      with import nixpkgs { system = "x86_64-linux"; };
      callPackage ./firmware_builder { RIOT = RIOT.packages.x86_64-linux.RIOT; firmware_name = "gnrc_border_router"; firmware_path = ./src; };

    packages.x86_64-linux.gnrc_networking =
      with import nixpkgs { system = "x86_64-linux"; };
      callPackage ./firmware_builder { RIOT = RIOT.packages.x86_64-linux.RIOT; firmware_name = "gnrc_networking"; firmware_path = ./src; };
  };
}