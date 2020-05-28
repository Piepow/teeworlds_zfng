# Teeworlds zfng

FNG mod with infection and nukes, based on
[fng2](https://github.com/Jupeyy/teeworlds-fng2-mod). This mod runs on Teeworlds
0.6.

## Building

You need a valid C++11 compiler.

### Linux

```sh
bam server_release
```

## Configuration

Copy `autoexec_default.cfg` to `autoexec.cfg`. For detailed explanations of
server options, see `variables.h`.

## Usage

```sh
./zfng_server
```

## Notes

- Caveats specific to `zfng2`
  - `sv_warmup` is not implemented. Instead use `sv_zfng_infection_delay`
  - `sv_tournament_mode` is not implemented

- Gametypes from the original `fng2` mod (such as `fng2`, `fng2solo`,
  `fng2boom`) should still work with a few minor changes
  - Sacrifice sound has been changed
  - Respawn delay has been lengthened slightly
  - Frozen players on `fng24teams` are indistinguishable
  - Spectators are kept in the spectator slots after changing the map
