{
  description = "A flake to build firmwares for RIOT OS";

  inputs.nixpkgs.url = github:NixOS/nixpkgs/nixos-20.09;
  inputs.fetchSWH.url = github:sisyphe-re/fetchSWH;

  outputs = { self, nixpkgs, fetchSWH }: {
    packages.x86_64-linux.gnrc_border_router =
      with import nixpkgs { system = "x86_64-linux"; };
      callPackage ./firmware_builder { fetchSWH = fetchSWH.lib.fetchSWH; firmware_name = "gnrc_border_router"; firmware_path = ./src; outputHash = "0sx5n6m4a7k91mr2djf6k8ppmqil88kiaxy5a5qaw6hiydr4gn9a"; };

    packages.x86_64-linux.gnrc_networking =
      with import nixpkgs { system = "x86_64-linux"; };
      callPackage ./firmware_builder { fetchSWH = fetchSWH.lib.fetchSWH; firmware_name = "gnrc_networking"; firmware_path = ./src; outputHash = "11g0dfm7hs8aa2s6kmqa1wja8bhcq7xsa1ky4kkjfylhcxwqfa49"; };

    packages.x86_64-linux.all-the-firmwares =
      with import nixpkgs { system = "x86_64-linux"; };
      buildEnv {
        name = "all-the-firmwares";
        paths = [
          self.packages.x86_64-linux.gnrc_networking
          self.packages.x86_64-linux.gnrc_border_router
        ];
      };
    defaultPackage.x86_64-linux = self.packages.x86_64-linux.all-the-firmwares;
  };

}
