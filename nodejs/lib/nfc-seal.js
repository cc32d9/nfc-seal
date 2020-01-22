'use strict'
const ecc               = require('eosjs-ecc');
const { NFC }           = require('nfc-pcsc');
const ndef              = require('@taptrack/ndef');
const shajs             = require('sha.js');

let myexports = {};

myexports.projsig = Buffer.from('076cf31a2b7f35a3', 'hex');
myexports.formatver = Buffer.from('0001', 'hex');

myexports.nfc_loop = (callback, only_once) => {
    const nfc = new NFC();
    
    nfc.on('reader', async reader => {
        console.info(`device attached`, { reader: reader.name });
        
        reader.on('card', async card => {
            if (card.type === 'TAG_ISO_14443_3') {
                card.cap = {};
                try {
                    const cc = await reader.read(3, 4);
                    switch(cc[2]) {
                    case 0x12:
                        // NTAG213
                        card.cap.configOffset = 0x29; 
                        card.cap.passwordOffset = 0x2b; 
                        card.cap.packOffset = 0x2c;
                        card.cap.chip = 'NTAG213'; 
                        break;
                    case 0x3E:
                        // NTAG215
                        card.cap.configOffset = 0x83; 
                        card.cap.passwordOffset = 0x85; 
                        card.cap.packOffset = 0x86;
                        card.cap.chip = 'NTAG215'; 
                        break;
                    case 0x6D:
                        // NTAG216
                        card.cap.configOffset = 0xe3; 
                        card.cap.passwordOffset = 0xe5; 
                        card.cap.packOffset = 0xe6;
                        card.cap.chip = 'NTAG216'; 
                        break;
                    default:
                        console.error('Unknown card');
                        return;
                    }
                    
                    // CMD: READ_SIG via Direct Transmit (ACR122U) and Data Exchange (PN533)
                    const authcmd = Buffer.from([
                        0xff, // Class
                        0x00, // Direct Transmit (see ACR122U docs)
                        0x00, // ...
                        0x00, // ...
                        0x04, // Length of Direct Transmit payload
                        // Payload (4 bytes)
                        0xd4, // Data Exchange Command (see PN533 docs)
                        0x42, // InCommunicateThru
                        0x3c, // READ_SIG
                        0x00, // addr=0
                    ]);
                    
                    const response = await reader.transmit(authcmd, 37);
                    
                    if (response.length == 37) {
                        card.cap.vendorsig = response.slice(3, (32+3)).toString('hex'); 
                    }
                    else {
                        console.error('Cannot read vendor signature from the tag');
                        return;
                    }
                }
                catch (e) {
                    console.error('ERROR: ' + e);
                    return;
                }
                
                await callback(reader, card).catch( (err) => {
                    console.error(`error:`, { reader: reader.name, card, err });
                });

                if( only_once ) {
                    process.exit();
                }
            }
            else {
                console.warn(`unknown card`, { reader: reader.name, card });            
            }
        });

        reader.on('card.off', async card => {
            console.info(`card removed`);
        });
    
        reader.on('error', err => {
            console.error(`an error occurred`, { reader: reader.name, err });        
        });

        reader.on('end', () => {
            console.info(`device removed`, { reader: reader.name });
        });
    });
    
    nfc.on('error', err => {
        console.error(`an error occurred`, err);
    });
};


myexports.read_seed = (reader, card, issuerid, seqnum) => {
    let offset = 0;
    let seed = Buffer.allocUnsafe(7+8+8+32);
    Buffer.from(card.uid, 'hex').copy(seed, offset, 0, 7);
    offset += 7;
    issuerid.copy(seed, offset, 0, 8);
    offset += 8;
    seqnum.copy(seed, offset, 0, 8);
    offset += 8;
    Buffer.from(card.cap.vendorsig, 'hex').copy(seed, offset, 0, 32);
    return seed;
}


myexports.write_label = async (reader, card, issuerid, seqnum, privkey) => {
    let seed = myexports.read_seed(reader, card, issuerid, seqnum);
    // console.log('Seed: ' + seed.toString('hex'));

    let labelsig_str = ecc.sign(seed, privkey);
    let labelsig_obj = ecc.Signature.fromString(labelsig_str);
    let labelsig = labelsig_obj.toBuffer();
    
    let id = Buffer.allocUnsafe(8+8);
    issuerid.copy(id, 0, 0, 8);
    seqnum.copy(id, 8, 0, 8);

    let offset = 0;
    let payload = Buffer.allocUnsafe(8+2+8+8+65+2);
    myexports.projsig.copy(payload, offset, 0, 8);
    offset += 8;
    myexports.formatver.copy(payload, offset, 0, 2);
    offset += 2;
    issuerid.copy(payload, offset, 0, 8);
    offset += 8;
    seqnum.copy(payload, offset, 0, 8);
    offset += 8;
    labelsig.copy(payload, offset, 0, 65);
    offset += 65;
    
    let chksum = shajs('sha256').update(payload.subarray(0, offset)).digest();
    chksum.copy(payload, offset, 0, 2);    
    
    // console.log('Writing ' + payload.length + ' bytes of payload');    
    let record = new ndef.Record(false,
                                ndef.Record.TNF_UNKNOWN,
                                new Uint8Array([]), // record type (unised in UNKNOWN)
                                id,
                                payload);

    let message = new ndef.Message([record]);
        
    // add TLV (0x03, len), termination (0xFE)
    // and pad the NDEF data to 4-byte frames
    
    let ndefdata = Buffer.from(message.toByteArray());
    if( ndefdata.length > 254 ) {
        throw new Error('NDEF data too long');
    }
    
    let wrlen = Math.ceil((3+ndefdata.length)/4)*4;
    let wrbuf = Buffer.allocUnsafe(wrlen);
    wrbuf.fill(0);
    wrbuf.writeUInt8(0x03, 0);
    wrbuf.writeUInt8(ndefdata.length, 1);
    ndefdata.copy(wrbuf, 2);
    wrbuf.writeUInt8(0xFE, ndefdata.length+2);
    
    console.log('Writing ' + wrbuf.length + ' bytes');
    await reader.write(4, wrbuf, 4);

    return labelsig;
};


myexports.read_label = async (reader, card) => {
    try {
        let rawdata = await reader.read(4, 144);
        if( rawdata.readUInt8(0) != 0x03 ) {
            throw('Unexpected type in TLV: ' + rawdata.readUInt8(0));
        }
        
        let msglen = rawdata.readUInt8(1);
        if( rawdata.readUInt8(2+msglen) != 0xFE ) {
            throw('Missing TLV terminator');
        }
        
        let message = ndef.Message.fromBytes(Buffer.from(rawdata, 2, msglen));
        let record = message.getRecords()[0];
        let payload = Buffer.from(record.payload);
        // console.log('Read ' + payload.length + ' bytes of payload');
        let offset = 0;
        
        let ps = payload.subarray(offset, offset+8);
        offset += 8;
        if( !ps.equals(myexports.projsig) ) {
            throw('Wrong project signature: ' + ps.toString('hex'));
        }
        
        let fv = payload.subarray(offset, offset+2);
        offset += 2;
        if( !fv.equals(myexports.formatver) ) {
            throw('Wrong format version: ' + fv.toString('hex'));
        }
        
        let issuerid = Buffer.allocUnsafe(8);
        payload.copy(issuerid, 0, offset, offset+8);
        offset += 8;
        
        let seqnum = Buffer.allocUnsafe(8);
        payload.copy(seqnum, 0, offset, offset+8);
        offset += 8;
        
        let labelsig = Buffer.allocUnsafe(65);
        payload.copy(labelsig, 0, offset, offset+65);
        offset += 65;
        
        let chksum = shajs('sha256').update(payload.subarray(0, offset)).digest();
        if( !payload.subarray(offset, offset+2).equals(chksum.subarray(0, 2)) ) {
            console.error('Cecksum on chip: ' + payload.subarray(offset, offset+2).toString('hex') +
                          ', should be: ' + chksum.subarray(0, 2).toString('hex'));
            throw('Incorrect label checksum');
        }
        
        return {'seed': myexports.read_seed(reader, card, issuerid, seqnum),
                'issuerid': issuerid,
                'seqnum': seqnum,
                'labelsig': labelsig
               };
    }
    catch(e) {
        console.error(e);
        return;
    }
}




myexports.verify_labelsig = (label, pubkey) => {
    let sig = ecc.Signature.fromBuffer(label.labelsig);
    return(sig.verify(label.seed, pubkey));
}
    

//      // Configuration page 1
//      config[0]
//      config[1]
//      config[2]
//      config[3] // AUTH0 (default: 0xff)
//
//      // Configuration page 2
//      config[4] // ACCESS
//      config[5] // VCTID (default: 0x05)
//      config[6]
//      config[7]


    
myexports.protect_label = async (reader, card, passphrase) => {
    try {
        // read current configuration
        let config = await reader.read(card.cap.configOffset, 8);
        if( config[3] != 0xFF ) {
            throw('The tag is already password-protected');
        }
        
        let pwhash = shajs('sha256')
            .update(passphrase)
            .update(Buffer.from(card.uid, 'hex'))
            .digest();
        
        // set the PASSWORD (4 bytes)
        await reader.write(card.cap.passwordOffset, pwhash.subarray(0,4));
        
        // set PACK (2 bytes)
        let packframe = Buffer.allocUnsafe(4).fill(0);
        pwhash.subarray(4,6).copy(packframe);
        await reader.write(card.cap.packOffset, packframe);
        
        // Protect everything (start with block 4)
        config[3] = 0x04;
        
        // set ACCESS bits
        // bit 7: PROT One bit inside the ACCESS byte defining the memory protection
        //          0b ... write access is protected by the password verification
        //          1b ... read and write access is protected by the password verification
        // bit 6: CFGLCK Write locking bit for the user configuration
        //        - 0b ... user configuration open to write access
        //        - 1b ... user configuration permanently locked against write access
        // bits 5-3: reserved
        // bits 2-0: AUTHLIM
        // bit number-76543210
        //            ||||||||
        config[4] = 0b00000000;
    
        // set access rules
        await reader.write(card.cap.configOffset, config);
        return true;
    }
    catch(e) {
        console.error(e);
        return false;
    }
}    


                      
myexports.erase_label = async (reader, card, passphrase) => {
    try {
        // read current configuration
        const config = await reader.read(card.cap.configOffset, 8);
        if( config[3] != 0xFF ) {
            console.log('The card is protected. Authenticating');
            let pwhash = shajs('sha256')
                .update(passphrase)
                .update(Buffer.from(card.uid, 'hex'))
                .digest();
            
            await myexports.card_auth(reader, card, {password: pwhash.subarray(0,4),
                                                     pack: pwhash.subarray(4,6)});
            
            // Unprotect everything
            config[3] = 0xFF;
            config[4] = 0b00000000;
            
            // set access rules
            await reader.write(card.cap.configOffset, config);
        }
        
        console.log('Erasing contents');
        let erasebuf = Buffer.allocUnsafe(144).fill(0);
        await reader.write(4, erasebuf);
        return true;
    }
    catch(e) {
        console.error(e);
        return false;
    }
};

    

myexports.card_auth = async (reader, card, pw) => {
    // CMD: PWD_AUTH via Direct Transmit (ACR122U) and Data Exchange (PN533)
    const authcmd = Buffer.from([
	0xff, // Class
	0x00, // Direct Transmit (see ACR122U docs)
	0x00, // ...
	0x00, // ...
	0x07, // Length of Direct Transmit payload
	// Payload (7 bytes)
	0xd4, // Data Exchange Command (see PN533 docs)
	0x42, // InCommunicateThru
	0x1b, // PWD_AUTH
	...pw.password,
    ]);
    
    const response = await reader.transmit(authcmd, 7);
    
    if (response.length < 5) {
	throw('Cannot authenticate: invalid response length');
    }
    
    if (response[2] !== 0x00 || response.length < 7) {
	throw('Authentication failed');
    }
    if (!response.slice(3, 5).equals(pw.pack)) {
        throw('PACK mismatch. Expected: ' + pw.pack.toString('hex') +
              ', received: ' + response.slice(3, 5).toString('hex'));
    }
}

        


module.exports = myexports;
