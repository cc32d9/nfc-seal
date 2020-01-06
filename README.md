NFC Seal of Authenticity
========================

Protocol
--------

The protocol is applicable to NXP NTAG213/215/216 chips.

The issuer has a secp256k1 or secp256r1 private key for issuing the
seals.

The NFC tag data is compliant with NDEF format, as follows:

* TNF (Type Name Format Field): 0x05 (Unknown Record)

* IL (ID LENGTH Field): 8

* SR (Short Record Bit): 1

* CF (Chunk Flag bit): 0

* ME (Message End bit): 1

* MB (Message Begin bit): 1

* ID: 64-bit sequence number assigned by the issuer

* Payload:

  * 2 bytes of format version (0x0001)

  * 64 bytes of issuer's ECC signature.



The signature is built as follows:

* 7 bytes UID of the NFC chip

* 8 bytes of ID assigned by the issuer

* 32 bytes of NXP vendor signarture of the chip

* on all of the above concatenated, sha256 hash is signed by issuer's key.


SHA256 hash of the signature is published on the blockchain if
tracking is needed.



Validation
----------

Read the tag contents, read the public key from the blockchain,
validate the signature.

