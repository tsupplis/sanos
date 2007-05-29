;-----------------------------------------------------------------------------
; fpreset.asm - floating point unit reset
; Ported from Al Maromaty's free C Runtime Library
;-----------------------------------------------------------------------------
                .386
_TEXT           segment use32 para public 'CODE'
                public  __fpreset
                
__fpreset       proc    near
                assume  cs:_TEXT
                finit                           ; Initialize the FPU
                ret
__fpreset       endp

_TEXT           ends
                end
