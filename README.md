TinyFugue - Rebirth
=====================

[![CI](https://github.com/ingwarsw/tinyfugue/actions/workflows/build.yml/badge.svg)](https://github.com/ingwarsw/tinyfugue/actions/workflows/build.yml)
[![GitHub Release](https://img.shields.io/github/release/ingwarsw/tinyfugue.svg?style=flat)](https://github.com/ingwarsw/tinyfugue/releases/latest)  

This project is meant to give rebirth to TinyFugue MUD client.

Because Ken Keys is not activelly developing it from 6 years and i gathered a lot of pathes over that time in thet time i decided to push them together.

# New features

### Python support

To enable:
```
./configure --enable-python
```

### Lua support

To enable:
```
./configure --enable-lua
```

### Widechar support

Widehar is enabled by default.
Widechar requires icu libraries to be installed on system (libicu-dev on ubuntu).

To disable:
```
./configure --disable-widechar
```

### New telnet options

	- ATCP
	- GMCP
	- option 102


```
To enable compile with:
	--enable-atcp		enable ATCP support	
	--enable-gmcp		enable GMCP support	
	--enable-option102	enable telnet option 102 support	
```

### New logging options

	- Timestamp logging
	- Ansi logging

## New versioning scheme

Because version was not changed ower last 14 years I have decided to go with normal versioning to allow easy distinguish different new versions.
I started with version 5.1.0 and will use try to follow [Semantic Versioning](https://semver.org/)

## Copyright

Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys (kenkeys@users.sourceforge.net)

http://tinyfugue.sourceforge.net/

[Oryginal README](README.orig)
