# Micro C three-address code
# idx | instruction
000 | function main:
001 | decl int i
002 | i = 1
003 | decl int total
004 | total = 0
005 | L0:
006 | t0 = i <= 5
007 | ifz t0 goto L1
008 | t1 = total + i
009 | total = t1
010 | t2 = i + 1
011 | i = t2
012 | goto L0
013 | L1:
014 | return total
015 | end function
