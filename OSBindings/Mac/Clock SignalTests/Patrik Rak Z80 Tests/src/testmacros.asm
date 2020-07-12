; Macros for defining the test vectors.
;
; Copyright (C) 2012 Patrik Rak (patrik@raxoft.cz)
;
; This source code is released under the MIT license, see included license.txt.

            macro   db8 b7,b6,b5,b4,b3,b2,b1,b0
            db      (b7<<7)|(b6<<6)|(b5<<5)|(b4<<4)|(b3<<3)|(b2<<2)|(b1<<1)|b0
            endm
            
            macro   ddbe n
            db      (n>>24)&0xff
            db      (n>>16)&0xff
            db      (n>>8)&0xff
            db      n&0xff
            endm

            macro   inst op1,op2,op3,op4,tail
            ; Unfortunately, elseifidn doesn't seem to work properly.
            ifidn   op4,stop
            db      op1,op2,op3,tail,0
            else
            ifidn   op3,stop
            db      op1,op2,tail,op4,0
            else
            ifidn   op2,stop
            db      op1,tail,op3,op4,0
            else
            db      op1,op2,op3,op4,tail
            endif
            endif
            endif
            endm

            macro   flags sn,s,zn,z,f5n,f5,hcn,hc,f3n,f3,pvn,pv,nn,n,cn,c
            if      maskflags
            db8     s,z,f5,hc,f3,pv,n,c
            else
            db      0xff
            endif
            endm

.veccount := 0

            macro   vec op1,op2,op3,op4,memn,mem,an,a,fn,f,bcn,bc,den,de,hln,hl,ixn,ix,iyn,iy,spn,sp

            if      postccf

            if      ( .@veccount % 3 ) == 0
            inst    op1,op2,op3,op4,tail
.@areg      :=      0
            else
            db      op1,op2,op3,op4,0
.@areg      :=      .@areg | a
            endif

            else
            db      op1,op2,op3,op4
            endif

            db      f

            if      postccf & ( ( .veccount % 3 ) == 2 )
            db      a | ( ( ~ .@areg ) & 0x28 )
            else
            db      a
            endif

            dw      bc,de,hl,ix,iy
            dw      mem
            dw      sp

.@veccount := .@veccount+1

            endm

            macro   crcs allflagsn,allflags,alln,all,docflagsn,docflags,docn,doc,ccfn,ccf,mptrn,mptr
            if      postccf
            ddbe    ccf
            elseif    memptr
            ddbe    mptr
            else
            if      maskflags
            if      onlyflags
            ddbe   docflags
            else
            ddbe   doc
            endif
            else
            if      onlyflags
            ddbe   allflags
            else
            ddbe   all
            endif
            endif
            endif
            endm
            
            macro   name n
            dz      n
            endm

; EOF ;
