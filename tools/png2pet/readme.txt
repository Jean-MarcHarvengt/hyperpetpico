Require libpng!!

make

./png2pet <image> <HRES> <VRES>
image as png (without '.png' extension)

HRES is 256 or 320 (according to GFX mode selected on HYPERPET)
VRES is 200 normally (or smaller)

Will create:
<image>.cru => pucrunched image 
(ready to embed in your assembly code, to be transferred to hyperpet's GFX memory)

(and some other intermediate format as gif, raw (before pucrunch compression), raw2 (after pucrunch decompression) for verification


source code from:
- libpng is open source
- gif enc/dec from: 
https://github.com/lecram/gifdec
https://github.com/lecram/gifenc
- pucrunch (great compressor for 8bit CPUs!) from
https://github.com/mist64/pucrunch

