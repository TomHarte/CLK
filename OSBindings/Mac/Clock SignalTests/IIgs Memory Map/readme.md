# Apple IIgs Memory Map Tests

## Sample test:

    {
    	"hires": false,
    	"lcw": false,
    	"80store": true,
    	"shadow": 12,
    	"state": 183,
    	"read": [
    		[0, 192, 256, 448],
    		[193, 208, 65473, 65488],
    		[209, 256, 465, 512],
    		[257, 448, 0, 0],
    		[449, 464, 65473, 65488],
    		[465, 512, 465, 512],
    		[513, 57537, 0, 0],
    		[57538, 57552, 65474, 65488],
    		[57553, 57600, 57553, 57600],
    		[57601, 57793, 0, 0],
    		[57794, 57808, 65474, 65488],
    		[57809, 57856, 57809, 57856],
    		[57857, 65536, 0, 0]
    	],
    	"write": [
    		[0, 192, 256, 448],
    		[193, 256, 65473, 65536],
    		[257, 448, 0, 0],
    		[449, 512, 65473, 65536],
    		[513, 57537, 0, 0],
    		[57538, 57600, 65474, 65536],
    		[57601, 57793, 0, 0],
    		[57794, 57856, 65474, 65536],
    		[57857, 65536, 0, 0]
    	],
    	"shadowed": [4, 12, 32, 64, 260, 268, 288, 320, 352, 416, 65535],
    	"io": [192, 193, 448, 449, 57536, 57537, 57792, 57793, 65535]
    }

## Application

Perform, in the order listed:

    	"hires": false,

If `hires` is true, access IO address `0x57`; otherwise access IO address `0x56`.

    	"lcw": false,

If `lcw` is true, write any value to IO address `0x81` twice; otherwise write to IO address `0x80` at least once.

    	"80store": true,

If `80store` is true, access IO address `0x01`; otherwise access IO address `0x00`.

    	"shadow": 12,

Store the value of `shadow` to IO address `0x35`.

    	"state": 183,

Store the value of `state` to IO address `0x68`.

## Test

**Only memory areas which are subject to paging are recorded with valid physical addresses.**

### `read` and `write`

Each entry looks like:

    [0, 192, 256, 448]

Which is of the form:

    [logical start, logical end, physical start, physical end]

Where all numbers are page numbers, i.e. address / 256.

So e.g. the entry above means that between logical addresses `$00:0000` and `$00:C000` you should find the physical RAM located between addresses `$01:0000` and `$01:C0000`.

If physical end == physical start then the same destination is used for all logical addresses; if that destination is `0` then the area is unmapped.

### `shadowed ` and `io`

An example chain looks like:

    [4, 12, 32, 64, 260, 268, 288, 320, 352, 416, 65535]

Starting from a default value of `false`, that means:

* memory remained un-[shadowed/IO] for until page 4 (i.e. pages 0–3);
* it was then marked as [shadowed/IO] until page 12 (i.e. pages 4–11);
* it was then un-[shadowed/IO] to page 32;
* ...etc.