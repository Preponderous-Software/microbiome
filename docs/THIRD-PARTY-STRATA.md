# Third-party notices (the Strata build)

This covers the Linux download of `mb_strata`, the Strata micro-environment
([STRATA.md](STRATA.md)). Microbiome itself is under the Preponderous
Non-Commercial License (`LICENSE`). `mb_strata` links in the Strata engine
and, through it, the components below, and its own vendored copy of
usd-recorder's replay writer; every licence here is permissive. Strata is
fetched at the commit in `strata/strata.ref` (`dd65ba2`), and the Strata
sections below are Strata's own `LICENSE` and `THIRD_PARTY_NOTICES.md` at
that commit.

## In mb_strata itself

| Component | Version | Licence | How it is used |
|---|---|---|---|
| [usd-recorder](https://github.com/Preponderous-Software/usd-recorder) replay writer | v0.2.0 (`7e67946`) | MIT | `--record` writes `usd-replay/1` replays, in `strata/vendor/usd_recorder` (its `LICENSE` is there). |

## Strata (MIT)

```
MIT License

Copyright (c) 2026 Daniel McCoy Stephenson

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Components linked in through Strata

| Component | Version | Licence | How it is used |
|---|---|---|---|
| [SDL](https://github.com/libsdl-org/SDL) | 3.4.16 | zlib | Window, GL context and input. Fetched at configure time (pinned by SHA-256) and linked statically into libstrata. |
| [glm](https://github.com/g-truc/glm) | 1.0.2 | MIT (or the Happy Bunny licence) | Maths, header-only. Fetched at configure time (pinned by SHA-256). |
| [glad](https://github.com/Dav1dde/glad) | 2.0.8 generator output | MIT (generated code); Khronos headers Apache-2.0 / MIT | GL 4.5 core and EGL 1.5 loaders, in Strata's `third_party/glad`. |
| [stb_image, stb_image_write](https://github.com/nothings/stb) | 2.30 / 1.16 | Public domain or MIT | PNG reading and writing, in Strata's `third_party/stb`. |
| [font8x8](https://github.com/dhepper/font8x8) | commit 8e279d2 (`font8x8_basic.h`) | Public domain | The overlay text font, in Strata's `third_party/font8x8`; Strata's `src/render/font8x8.hpp` is generated from it. |

## SDL (zlib)

```
Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

## glm (MIT)

```
Copyright (c) 2005 - G-Truc Creation

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

## glad (MIT, generated code)

```
The MIT License (MIT)

Copyright (c) 2013-2022 David Herberth

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

The Khronos headers under Strata's `third_party/glad/include/KHR` and
Strata's `third_party/glad/include/EGL` carry their licences in their own file headers.

## stb (public domain or MIT)

See the licence section at the end of Strata's `third_party/stb/stb_image.h`.

## font8x8 (public domain)

```
8x8 monochrome bitmap fonts for rendering
Author: Daniel Hepper <daniel@hepper.net>
License: Public Domain

Based on font8x8.h by Marcel Sondaar and the public domain IBM VGA fonts.
```
