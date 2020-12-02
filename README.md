FONTO.io: NFC Seal of Authenticity
==================================

Telegram chat for project discussion: https://t.me/nfc_seal

Introduction
------------

This project aims to build a solution for fighting counterfreight. A
manufacturer will be able to attach an electronic seal to their goods
before shipping, and the recipient will be able to verify that the
seal is original and not tampered with.

Both sealing and verification will be done with mobile phones or
specialized handheld devices. Verification of authenticity can be done
either with the use of a public blockchain, or offline. The protocol
is open, which means any third party can build their solutions and
make them interoperable.

The technology being used here is not new, but nobody has come up yet
with an open standard for traceability and on-chain verification. Open
standard is what distinguishes this project from others.


Functional overview
-------------------

The following workflow is being implemented in this project:

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

64-bit identifiers are stored as big-endian unsigned integers.

The issuer has a secp256k1 or secp256r1 private key for issuing the
seals.

The data is written to an NFC chip in NDEF format. The first record of
matching TNF and project ID is considered as a label. It is possible
to add a record with an URL, and it's preferable to put it in front of
the seal label record.

NDEF record format:

* TNF (Type Name Format Field): 0x05 (Unknown Record)

* IL (ID LENGTH Field): 16

* SR (Short Record Bit): 1

* CF (Chunk Flag bit): 0

* ME (Message End bit): 1

* MB (Message Begin bit): 1

* ID: 64-bit issuer identifier, followed by 64-bit sequence number
  assigned by the issuer

* Payload:

  * 8 bytes of project ID: `0x076cf31a2b7f35a3`. This value is fixed
    for all compatible implementations.
  
  * 2 bytes of format version (0x0001)

  * 8 bytes of issuer ID 
  
  * 8 bytes of sequence number assigned by the issuer
        
  * 65 bytes of label signature in format compatible with EOSIO
    signature serialization: the first byte indicates a type (0 for
    secp256k1, 1 for secp256r1), and the rest is 64 bytes of ECC
    signature.

  * 2 bytes of checksum: two first bytes of sha256 hash over the above
    payload fields.

The label signature is built as follows:

* 7 bytes UID of the NFC chip

* 8 bytes of issuer ID 

* 8 bytes of sequence number assigned by the issuer

* 32 bytes of NXP vendor signarture on the chip

* on all of the above concatenated, sha256 hash is signed by issuer's key.


The memory is protected from writing with a 4-byte password, as
follows. The 7-byte chip UID is added at the end of the issuer's
passphrase. A resulting bytes array is hashed by SHA256. First 4 bytes
of the hash are used as the password, and 2 following bytes are used
as PACK for authentication validation. This allows the issuer to erase
the label and re-write the chip if needed.



SHA256 hash of the 65-byte label signature is optionally published on
the blockchain if tracking is needed.



Supplementary QR code
---------------------

NFC labels require a very close contact, and sometimes there is a need
to find the right label among a number of them. For example, if
multiple containers are placed in a warehouse, scanning every NFC
label in order to find the right one is impractical.

QR codes can be read from a distance, and typically the scanning time
is much shorter than that of NFC. QR labels cannot be used for
authenticity checks because they are too easy to copy. But they can be
of great help as a supplementary means of identifying the NFC labels.

The QR code that identifies a label is formatted as an alphanumeric
string with the following fields separated by slash (`/`):

* project ID: `076CF31A2B7F35A3`;

* format version: `0` without the signature hash, `1` with it;

* issuer ID as decimal integer, zero-padded to 20 characters;

* sequence number as decimal integer, zero-padded to 20 characters;

* 32-byte SHA256 hash of the label signature as uppercase hex string.

The code is printed at high error correction level (H) because it has
to be read in harsh conditions and from a significant distance.

Test printouts have been made using 180dpi Brother P-Touch PT-P750W
label printer.  Format 0 labels printed on 18mm tape were difficult to
scan, but still readable. If using 24mm tape, both format 0 and 1
labels were readable with a mobile phone. Format 0 labels were
recognized quickly, while format 1 labels needed additional time to
focus the camera.






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


Reference implementation and demo
---------------------------------

The project repository contains a fully functional demo that can be
used as a reference for validating third-party software for
compatibility.

The tools require a physical Ubuntu computer with an USB interface
where you attach an NFC reader (tested only with ACR122U, available on
AliExpress for about $25). Ubuntu 18.04 and 19.10 has been tested, and
it should work on future releases too.

The smart contract for registering the seals and public keys on the
blockchain is deployed on Telos testnet under `sealtest1111`
account. You may use it for your own tests, or deploy the contract in
a new account.

In the example below, the following accounts have been created:
`sealissuer11` for the issuing party, `sealtransit1` for the
transportation company, and `sealrcpnt111` as the recipient of the
goods.

All private keys are cut off in this demo.

The seal writing command uses the signer's private key directly in the
command line argument. This is not safe for production, but OK for
demo and testing purposes.In production environment, the client
software should take care of secure key management.

```
# install eosio software (we only need cleos from it)

cd /tmp
wget https://github.com/EOSIO/eos/releases/download/v2.0.0/eosio_2.0.0-1-ubuntu-18.04_amd64.deb
sudo apt install ./eosio_2.0.0-1-ubuntu-18.04_amd64.deb

# install nodejs 12.x
curl -sL https://deb.nodesource.com/setup_12.x | sudo -E bash -
sudo apt-get install -y nodejs

# additional packages
sudo apt-get install -y git libpcsclite1 libpcsclite-dev pcscd make gcc g++

# fix NFC driver compatibility issues
sudo -i
cat >/etc/modprobe.d/rfid-blacklist.conf <<'EOT'
blacklist pn533_usb
blacklist pn533
blacklist nfc
EOT

reboot




# get a copy of the project in your home directory, or choose your preferred path
cd
git clone https://github.com/cc32d9/nfc-seal.git
cd nfc-seal/nodejs/
# this command will end with an error, but the update command will fix it
npm install
npm update

# You need to create your own accounts using testnet faucet:
# https://app.telos.net/testnet/developers
# Further instructions assume the user accounts
# sealissuer11, sealtransit1, sealrcpnt111, but you need
# to create your own.

# create private keys for each account and one more for the seal
# signing. Save them in a safe place.

cleos create key --to-console

# this command will create a wallet file in "eosio-wallet" folder in
# your home directory. It generates an encryption password that you
# need to save in a safe place.

cleos wallet create -n sealtest --to-console

# try to lock it and unlock with the password that you saved:

cleos wallet lock -n sealtest
cleos wallet unlock -n sealtest --password PW5K3xxxxxx

# import private keys of your 3 accounts into the wallet:

cleos wallet import -n sealtest

# create an alias for the testnet API endpoint:

alias tTcleos='cleos -u https://testnet-telos-21zephyr.maltablock.org'

# you need to pick an issuerid number that is not used yet. This will
# show all userid numbers used by the smart contract:

tTcleos get table sealtest1111 0 issuerids

# in this example, we use issuerid=100

tTcleos push action sealtest1111 addiid '["sealissuer11", 100]' -p sealissuer11@active

# you need to create one or several workflows. A workflow defines the
# transport company account and the recipient, and gives a short
# explanation of the delivery. Here we add a workflow number 55:

tTcleos push action sealtest1111 addwflow '[100, 55, "Alice ships potatos to Bob", "sealtransit1", "sealrcpnt111"]' -p sealissuer11@active

# this will register the public key that the issuer will use for
# signing the labels. This is one of the keys you created and saved
# using "cleos create key" command.  This will allow us sign up to
# 10000 unique seals with our signing key.  If you need to sign more,
# you will need to generate a new key. The number 10000 is not strict,
# it should be a reasonable number to cover your normal production
# workflow for a year or longer.

tTcleos push action sealtest1111 addkey '[100, 10000, "EOS7SiRqonkPhqrEshkVSTSJuCnedppwxTSVq1RdXyAFB1zLhu6kh"]' -p sealissuer11@active

# In the commands below, --loop option makes the program wait for more
# seals to process. Without the --loop option, the program exists
# after processing one NFC chip.

# Attach the USB NFC reader to the computer and get the NFC chips
# ready. This command will start writing the seals in a loop, until
# stopped, with sequence numbers starting from 10. It's up to you to
# keep track of numbers and to make sure they are not reused. In
# production, the client software should take care of number
# management for your labels.

~/nfc-seal/nodejs/bin/write_label --issuer=100 --seq=10 --key=5xxxTHEPRIVATEKEYxxxxxxx --loop

# Protect the seals from overwriting. Choose a secure and long enough
#  passphrase and store it safely.

~/nfc-seal/nodejs/bin/manage_protection --pass 1r84r89fhkrhcew7cyweyvwr784cyvewrc --protect --loop


# Now we can optionally register the seals on blockchain. By doing so,
# we prevent repeating sequence numbers. It is also possible to verify
# the seals without registering each on the blockchain. By default,
# seals have 1 year expiration time, and this can be changed by
# specifying --expires option and number of days after which the seal
# becomes invalid

~/nfc-seal/nodejs/bin/eosio_publish --url=https://testnet-telos-21zephyr.maltablock.org \
 --account=sealissuer11 --key=5xxxx_PRIVATEKEY_OF_sealissuer11 --contract=sealtest1111 \
 --workflow=55 --loop

# Now anyone along the transition path of the seal can validate its
# authenticity. If the seal is registered on the blockchain, its hash
# is verified. otherwise, only the validity of on-chip signature is
# verified:

~/nfc-seal/nodejs/bin/eosio_verify --url=https://testnet-telos-21zephyr.maltablock.org --contract=sealtest1111 --loop

# issuer, transit company, and the recipient can modify the status of the seal"

tTcleos push action sealtest1111 setstatus '["sealtransit1", 100, 10, "checkpoint1", "Shipping container 23124134533"]' -p sealtransit1@active

tTcleos push action sealtest1111 setstatus '["sealrcpnt111", 100, 10, "received", "goods received"]' -p sealrcpnt111@active

# Receiver can wipe out the seal from the blockchain if it's no longer in use:

tTcleos push action sealtest1111 delseal '[100, 10, "done"]' -p sealrcpnt111@active

# issuer can delete a workflow if it dosn't contain any seals:

tTcleos push action sealtest1111 delwflow '[100, 55]' -p sealissuer11@active

```




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
