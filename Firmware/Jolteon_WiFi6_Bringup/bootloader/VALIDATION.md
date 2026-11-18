# Bootloader validation - revision 0.1.1

See [repository review](../docs/REVIEW_AND_VALIDATION.md) and firmware/manifest.json.

Reproducible 5296-byte image; SHA256 6f4b21576c798172ecfc299986c4202a044cf9eedd4820319c324c2389c42b42; CRC32 3fc39799.
Application vectors, naked handoff assembly, GPIO sequencing and generated profile hashes checked. Application starts at 0x2000; custom profile MSP is 0x20007FF8.
No hardware was flashed or physically tested.
