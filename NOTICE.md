# Credits and third-party content

DeadWarrior 2 is an independent project to port Stick War 2 to C++. It is not affiliated with Max Games or the original game's creators.

The Stick War 2 name, characters, levels, artwork, music, sound effects, and other original content belong to their respective owners. The source tree does not include the original SWF or decompiled AS3 scripts. The Windows game download includes prepared native resources required by the port. Optional resource-regeneration tools operate locally on a compatible SWF supplied by the developer.

The DeadWarrior 2 logo and helmet icon were supplied by the project owner. The supplied Max Games logo is retained for the startup non-affiliation notice.

Stick Engine 2 provides geometry and pixel blending used by the native port. The corresponding headers carry the [Stick Engine 2 MIT license](third_party/stick-engine-2/LICENSE). The Stick Engine 2 logo was supplied by the project owner.

Preparation dependencies — JPEXS FFDec, Java, Pillow, NumPy, JPype, and imageio-ffmpeg — are obtained separately and retain their own licenses.

A distribution license for this project's code has not yet been selected. This notice does not apply a new license to original game resources or trademarks.

## Portable platform libraries

The macOS and Linux frontend uses SDL 3 (zlib license) and stb_truetype (MIT/public domain). Their license texts are included with the downloads. Noto Sans is distributed under the SIL Open Font License 1.1; see `art/fonts/OFL.txt`. These libraries do not replace the native C++ campaign simulation.

Lossless cinematic decoding uses libwebp 1.6.0 (BSD license and accompanying patent grant). Its license text is included as `licenses/libwebp.txt` on every platform. Asset conversion retains the original artwork and audio attribution.
