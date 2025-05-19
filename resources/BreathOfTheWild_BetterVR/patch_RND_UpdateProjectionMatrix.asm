[BetterVR_UpdateProjectionMatrix_V208]
moduleMatches = 0x6267BFD0

.origin = codecave


updateCameraPositionAndTarget:
; repeat instructions from either branches
lfs f0, 0xEC0(r31)
stfs f0, 0x5CC(r31)
lfs f13, 0xEB8(r31)
stfs f13, 0x5C4(r31)

; function prologue
mflr r0
stwu r1, -0x58(r1)
stw r0, 0x5C(r1)

bl import.coreinit.hook_updateCameraOLD

exit_updateCameraPositionAndTarget:
; function epilogue
lwz r0, 0x5C(r1)
mtlr r0
addi r1, r1, 0x58

addi r3, r31, 0xE78
blr


0x02C054FC = bla updateCameraPositionAndTarget
0x02C05590 = bla updateCameraPositionAndTarget
