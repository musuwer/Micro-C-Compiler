# Micro C three-address code
# idx | instruction
000 | function main:
001 | decl int a
002 | a = 0
003 | L0:
004 | t0 = a < 3
005 | ifz t0 goto L1
006 | t1 = a + 1
007 | a = t1
008 | goto L0
009 | L1:
010 | return a
011 | end function
