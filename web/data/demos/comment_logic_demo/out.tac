# Micro C three-address code
# idx | instruction
000 | function main:
001 | decl int a
002 | a = 10
003 | decl int b
004 | b = 12
005 | t0 = a < b
006 | t1 = a == 0
007 | t2 = !t1
008 | t3 = t0 && t2
009 | t4 = b == 12
010 | t5 = t3 || t4
011 | ifz t5 goto L0
012 | t6 = a + b
013 | return t6
014 | goto L1
015 | L0:
016 | L1:
017 | return 0
018 | end function
