# name: attributes for -march=armv6zt2
# source: blank.s
# as: -march=armv6zt2
# readelf: -A
# This test is only valid on ELF based ports.
# not-target: *-*-*coff *-*-pe *-*-wince *-*-*aout* *-*-netbsd *-*-riscix*

Attribute Section: aeabi
File Attributes
  Tag_CPU_name: "6ZT2"
  Tag_CPU_arch: v6T2
  Tag_ARM_ISA_use: Yes
  Tag_THUMB_ISA_use: Thumb-2