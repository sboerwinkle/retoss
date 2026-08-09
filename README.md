# Retoss

This project is an excuse for me to learn about many little things that go into game engines / game design. It is kind of a game, but if things seem a bit weird that's because it's more meant to be "fun to work on" than, like "profitable" or something.

Basically it's a team deathmatch shooter to be played with friends, but I'm not going to explain more because then it would get out of date. I legit forgot this project even had a `README.md` for, uhh, a while.

If you think this is kind of neat, check out [Bittoss](https://github.com/sboerwinkle/bittoss), which is kind of this project's predecessor before I decided to rework some foundational design decisions.

## Server

The server is one python script, at `server/server.py`. Defaults to serving on port 15000, or you can pass an argument.

## Compiling

On Linux, `build.sh` should see you through. You'll probably need to install some dependencies, but if you're familiar with how development packages are named for your distro it shouldn't be too bad.

On Windows, it will probably be a pain. I'll probably put out pre-compiled Windows releases more regularly if this ever becomes a "real" game. Instructions can be found in `building_for_windows.txt`. Good luck.

## Running

The executable (`game`, or `game.exe`) takes two arguments:

- Mandatory: Server IP. The Linux build also accepts hostnames here if you have one.
- Optional: Server Port. Defaults to 15000.

Once it connects, it should pop open a browser where you can do basic player setup (like name and team) and set some settings.

## Contributors

sboerwinkle: Yours truly, primary author

mboerwinkle:

- IPv6 support, server hardening, and host resolution for networking
- Starter graphics code
- Some linear algebra functions
- JSON reading/writing
- Maybe more, IDK
