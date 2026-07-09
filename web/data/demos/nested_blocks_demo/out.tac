# Micro C three-address code
# idx | instruction
000 | function main:
001 | decl int value
002 | value = 3
003 | decl int value
004 | value = 7
005 | decl int extra
006 | extra = 12
007 | t0 = value + extra
008 | value = t0
009 | return value
010 | end function
