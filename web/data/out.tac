# Micro C three-address code
# idx | instruction
000 | function main:
001 | decl int total
002 | total = 0
003 | decl int i
004 | i = 0
005 | L0:
006 | t0 = i < 4
007 | ifz t0 goto L1
008 | t1 = i == 2
009 | ifz t1 goto L2
010 | t2 = total + 10
011 | total = t2
012 | goto L3
013 | L2:
014 | t3 = total + i
015 | total = t3
016 | L3:
017 | t4 = i + 1
018 | i = t4
019 | goto L0
020 | L1:
021 | decl int j
022 | j = 0
023 | L4:
024 | t5 = j < 3
025 | ifz t5 goto L5
026 | decl int add
027 | t6 = j + 1
028 | add = t6
029 | t7 = total + add
030 | total = t7
031 | t8 = j + 1
032 | j = t8
033 | goto L4
034 | L5:
035 | t9 = total > 15
036 | t10 = j == 3
037 | t11 = t9 && t10
038 | ifz t11 goto L6
039 | return total
040 | goto L7
041 | L6:
042 | return 0
043 | L7:
044 | end function
