# Micro C three-address code
# idx | instruction
000 | function main:
001 | decl int a
002 | a = 3
003 | decl int b
004 | b = 7
005 | decl int c
006 | c = 0
007 | t0 = a < b
008 | t1 = b != 0
009 | t2 = t0 && t1
010 | t3 = t2 || c
011 | ifz t3 goto L0
012 | t4 = b * 4
013 | t5 = a + t4
014 | return t5
015 | goto L1
016 | L0:
017 | L1:
018 | return 0
019 | end function
