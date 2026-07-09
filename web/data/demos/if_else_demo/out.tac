# Micro C three-address code
# idx | instruction
000 | function main:
001 | decl int score
002 | score = 86
003 | decl int pass
004 | pass = 0
005 | t0 = score >= 60
006 | ifz t0 goto L0
007 | pass = 1
008 | goto L1
009 | L0:
010 | pass = 0
011 | L1:
012 | t1 = pass == 1
013 | t2 = score > 80
014 | t3 = t1 && t2
015 | ifz t3 goto L2
016 | return 42
017 | goto L3
018 | L2:
019 | L3:
020 | return 0
021 | end function
