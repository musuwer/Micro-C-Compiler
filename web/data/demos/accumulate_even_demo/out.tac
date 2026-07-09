# Micro C three-address code
# idx | instruction
000 | function main:
001 | decl int i
002 | i = 0
003 | decl int total
004 | total = 0
005 | i = 0
006 | L0:
007 | t0 = i <= 10
008 | ifz t0 goto L1
009 | t1 = i % 2
010 | t2 = t1 == 0
011 | ifz t2 goto L2
012 | t3 = total + i
013 | total = t3
014 | goto L3
015 | L2:
016 | L3:
017 | t4 = i + 1
018 | i = t4
019 | goto L0
020 | L1:
021 | return total
022 | end function
