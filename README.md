NFC Seal of Authenticity
========================

Telegram chat for project discussion: https://t.me/nfc_seal

Introduction
------------

The technology being used here is not new, but nobody has come up yet
with an open standard for traceability and on-chain verification. This
project aims to propose an open standard, so that many vendors and
manufactures would be able to build their products and become
interoperable.


The goal of this project is to build a framework around the following
workflow:


* An issuer of seals is initializing NXP NTAG213/215/216 NFC chips
  with unique signatures (called label signatures in this document),
  and optionally registers them on EOSIO blockchain. The issuer's
  public key is already known on the blockchain.


* The NFC chip is attached to a physical object that needs proof of
  authenticity: it could be a container of goods, or an individual
  piece of luxury, or a door in a compartment or cabinet. Whenever
  someone tries to remove the chip, it's physically damaged or stops
  functioning, indicating that there was an attemt to tamper the
  protected object.


* It costs too much to replicate and fake the seal and make it appear
  exactly like the original one.

* The receiver of goods can easily verify the authenticity by reading
  the data on the chip and comparing it with the data on blockchain.


The NTAG chips by NXP Semiconductors are cheap and reliable
microdevices produced in various forms and embedded in various kinds
of seals, labels, stickers, cards and keyfobs.

Such a chip contains 144 or more bytes of memory that can be protected
with a 4-byte password from writing or reading+writing. Also it has a
7-byte UID which is guaranteed to be unique by NXP. Also it has a
read-only area containing a vendor ECC signature of the UID.

Modern smartphones equipped with an NFC reader can read and write on
such chips. The maximum distance of communication is only a few
millimeters.


The issuer is marking the chips with a 128-bit ID (64-bit issuer
identifier and 64-bit sequence number). The issuer also reads the UID
and vendor signature, signs this whole data with their private key,
and the label signature and the ID are written on the chip. The chip
is then protected from writing with a password. The issuer is able to
erase the chip contents and re-write if the seal is reusable.

The issuer may register the hash of the label signature on the
blockchain, thus preventing from insider attacks: if someone tries to
produce a seal knowing the private key, the sequence number is already
registered on the blockchain.

It is worth mentioning that the design is only secure when the
validator observes the physical state of the seal and makes sure it
looks untampered. All related data on the chip is readable without a
password, so one can, in theory, construct a device that responds
exactly like the original seal, including the vendor
signature. However it's very expensive to make a counterfreight
microchip that simulates the original NXP NTAG and is made in the same
form as the original seal.



Protocol
--------

The protocol is applicable to NXP NTAG213/215/216 chips.

The issuer has a secp256k1 or secp256r1 private key for issuing the
seals.

The NFC tag data is compliant with NDEF format, as follows:

* TNF (Type Name Format Field): 0x05 (Unknown Record)

* IL (ID LENGTH Field): 16

* SR (Short Record Bit): 1

* CF (Chunk Flag bit): 0

* ME (Message End bit): 1

* MB (Message Begin bit): 1

* ID: 64-bit issuer identifier, followed by 64-bit sequence number
  assigned by the issuer

* Payload:

  * 2 bytes of format version (0x0001)

  * 64 bytes of label signature.



The label signature is built as follows:

* 7 bytes UID of the NFC chip

* 8 bytes of issuer ID 

* 8 bytes of sequence number assigned by the issuer

* 32 bytes of NXP vendor signarture on the chip

* on all of the above concatenated, sha256 hash is signed by issuer's key.


The memory is protected from writing with a 4-byte password. There are
several options, depending on usage of the chip. If a chip is
reusable, it can be re-labeled using the password.

* If it's a single-use chip, such as a physical seal that gets
  destroyed after opening, the password is taken as a random number,
  and immediately discarded.


* If the private key for signing the chips is available in raw form,
  the label signature is concatenated with the first 4 bytes of the
  private key, hashed with sha256, and the first 4 bytes of resulting
  hash are the password for write protection.

* If the private key is stored on a hardwarre wallet, it's impossible
  to retrieve in raw form, so the password is derived from a
  predefined passphrase, concatenated with the label signature, hashed
  with sha256, and the first 4 bytes of the hash are taken as a
  password.



SHA256 hash of the label signature is optionally published on the
blockchain if tracking is needed.


Usage of NXP NTAG 424 DNA chips
-------------------------------

NXP has recently released a new NFC chip, NTAG 424 DNA. It's slightly
more expensive than the older NTAG213/215/216, but it allows defining
an AES password that is securely written on the chip, and the chip is
keeping a counter of NFC sessions. On every NFC communication, the
counter is increased, and it can be retrieved in encrypted form, using
the stored password.

The encrypted counter is then transmitted to some online validation
service, and this service can confirm the validity of the password -
for example, by signing a transaction on the blockchain.

Detailed specifications and usage workflow are yet to be designed.

These chips would allow intermediate verifications: a container of
goods would travel between checkpoints, and every checkpoint would
register a unique proof of receipt.


Validation
----------

Read the chip contents, read the public key from the blockchain,
validate the signature.



License and copyright
---------------------

Copyright 2020 cc32d9@gmail.com

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
