# Micro C three-address code
# idx | instruction
000 | function main:
001 | decl int a
002 | a = 5
003 | decl int b
004 | t0 = -a
005 | t1 = t0 + 14
006 | b = t1
007 | t2 = b != 9
008 | t3 = !t2
009 | ifz t3 goto L0
010 | return b
011 | goto L1
012 | L0:
013 | L1:
014 | return 0
015 | end function
