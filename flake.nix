{
  description = "A flake to build firmwares for RIOT OS";

  inputs.nixpkgs.url = github:NixOS/nixpkgs/nixos-21.05;
  inputs.fetchSWH.url = github:sisyphe-re/fetchSWH;

  outputs = { self, nixpkgs, fetchSWH }: {
    packages.x86_64-linux.gnrc_border_router =
      with import nixpkgs { system = "x86_64-linux"; };
      callPackage ./firmware_builder { fetchSWH = fetchSWH.lib.fetchSWH; firmware_name = "gnrc_border_router"; firmware_path = ./src; outputHash = "sha256-fejWaUj+YzooTsIQ4qB+ZhJ/lZgMhaZa1FppNqoAyNQ="; };

    packages.x86_64-linux.gnrc_networking =
      with import nixpkgs { system = "x86_64-linux"; };
      callPackage ./firmware_builder { fetchSWH = fetchSWH.lib.fetchSWH; firmware_name = "gnrc_networking"; firmware_path = ./src; outputHash = "sha256-Se6Dji2eLrdgGayRqJzzdKIsTKsom/bGLZVB5virhRQ="; };

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
