# Micro C three-address code
# idx | instruction
000 | function main:
001 | decl int sum
002 | sum = 0
003 | decl int i
004 | i = 0
005 | L0:
006 | t0 = i < 5
007 | ifz t0 goto L1
008 | t1 = sum + i
009 | sum = t1
010 | t2 = i + 1
011 | i = t2
012 | goto L0
013 | L1:
014 | t3 = sum == 10
015 | ifz t3 goto L2
016 | t4 = sum + 3
017 | return t4
018 | goto L3
019 | L2:
020 | return 0
021 | L3:
022 | end function
