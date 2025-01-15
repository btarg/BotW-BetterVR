[BetterVR_Misc_V208]
moduleMatches = 0x6267BFD0

.origin = codecave

; this forces the model bind function to never try to bind it to a specific bone of the actor
; 0x31258A4 = jumpLocation:
; 0x0312578C = b jumpLocation

; remove binded weapon
; 0x03125880 = nop



# 0x020661B8 = cmpwi r1, 0
#
# 0x024AC5B0 = nop
#
# 0x024AC588 = nop
#
# 0x024AC8C4 = nop
#
# 0x024AC8D0 = nop
#
# 0x024AC8D4 = nop
#
# 0x024AC8DC = cmpw r3, r3
#
# 0x024AC7B4 = nop




# 0x02C18754 = nop

# 0x02C18764 = nop

;0x034B69C0 = li r0, 1


#0x02C196A4 = li r3, 1

#0x024B6F40 = li r12, 0


# disables all collisions
0x030E47CC = li r3, 0
0x030E47E4 = li r3, 0