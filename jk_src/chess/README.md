# jk_chess

A software-rendered chess application written from scratch in C. It supports x86_64 Windows, arm64
macOS, and the web. You can download Windows and macOS executables on the
[Releases](https://github.com/jeremiahkellick/jk_repo/releases) page, or
[try it in your browser.](https://chess.jeremiahkellick.com/)

![Demo Gif](/jk_assets/chess/chess.gif)

At runtime, the Windows and macOS ports depend on only the C standard library and APIs provided by
their respective operating systems. The web version doesn't even depend on a C standard library
implementation, so none of the platform-independent code can use it either. This took me down some
fun rabbit holes.

For floating-point rounding and square root, I used x86 intrinsics when compiling with MSVC and
builtins when using Clang or GCC. There weren't such intrinsics, however, for sine and arccosine.
Instead, I used Mathematica to compute constants for
[polynomial approximations.](https://github.com/jeremiahkellick/jk_repo/blob/v0.1/jk_src/jk_lib/jk_lib.c#L544)

I also wrote a printf replacement. The most interesting part was the
[function that converts a floating-point number to a string,](https://github.com/jeremiahkellick/jk_repo/blob/v0.1/jk_src/jk_lib/jk_lib.c#L326)
accurate to about eight decimal places.

The app is resolution-independent. I wrote a
[vector graphics rasterizer](https://github.com/jeremiahkellick/jk_repo/blob/v0.1/jk_src/jk_shapes/jk_shapes.c#L514)
to draw the chess pieces and text. For the chess pieces, I
[parse](https://github.com/jeremiahkellick/jk_repo/blob/v0.1/jk_src/chess/chess_assets_pack.c#L73) a
[subset of the SVG path format.](https://github.com/jeremiahkellick/jk_repo/blob/v0.1/jk_assets/chess/paths.txt)
For the text, I use stb_truetype to to extract data from a font file and convert it to the format my
rasterizer understands, but I do that in a separate program that runs as a build step. The
executable does not contain any stb_truetype code.

Still, much credit to Sean Barrett, author of the stb libraries. His
[writeup on the stb_truetype anti-aliasing algorithm](https://nothings.org/gamedev/rasterize/)
taught me enough about vector graphics rasterization to write it myself.

The next most interesting part of the project was the AI. As a challenge to myself, I did not read
up on how to write a chess bot. I wanted to see how far I would get "on my own steam." I'm
reasonably happy with the result. It's better than I am at chess, though I can still eke out a win
against it occasionally.

Still, now that I've made the AI about as good as I can on my own, I'm looking forward to reading up
on state-of-the-art techniques and implementing some to see if I can't write an AI that I will never
checkmate.
