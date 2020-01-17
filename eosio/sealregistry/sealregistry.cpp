/*
Copyright 2019 cc32d9@gmail.com

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <eosio/eosio.hpp>
#include <eosio/multi_index.hpp>
#include <eosio/crypto.hpp>
#include <eosio/time.hpp>


using namespace eosio;

CONTRACT sealregistry : public eosio::contract {
 public:
  sealregistry( name self, name code, datastream<const char*> ds ):
    contract(self, code, ds)
    {}
  
  ACTION addiid(name owner, uint64_t issuerid)
  {
    require_auth(owner);
    issuerids _ids(_self, 0);
    auto iiditr = _ids.find(issuerid);
    check( iiditr == _ids.end(), "This issuer ID already exists");
    _ids.emplace(owner, [&]( auto& item ) {
                          item.id = issuerid;
                          item.owner = owner;
                        });
  }

  ACTION addkey(uint64_t issuerid, uint64_t seq_start, uint64_t seq_end, public_key pubkey)
  {
    check(seq_start <= seq_end, "start number should not be greater than end");
    issuerids _ids(_self, 0);
    auto iiditr = _ids.find(issuerid);
    check(iiditr != _ids.end(), "Unknown issuer ID");
    name owner = iiditr->owner;
    require_auth(owner);

    pubkeys _pubkeys(_self, 0);

    /* search for an overlapping interval */    
    auto endidx = _pubkeys.get_index<name("seqend")>();
    auto enditr = endidx.lower_bound(get_seq_key(issuerid, seq_start));
    if( enditr != endidx.end() ) {
      check(enditr->seq_start > seq_end, "Overlapping intervals");
    }

    _pubkeys.emplace(owner, [&]( auto& item ) {
                              item.id = _pubkeys.available_primary_key();
                              item.issuerid = issuerid;
                              item.seq_start = seq_start;
                              item.seq_end = seq_end;
                              item.pubkey = pubkey;
                            });
  }


  ACTION addwflow(uint64_t issuerid, uint64_t workflow, name transit, name recipient)
  {
    issuerids _ids(_self, 0);
    auto iiditr = _ids.find(issuerid);
    check(iiditr != _ids.end(), "Unknown issuer ID");
    name owner = iiditr->owner;
    require_auth(owner);

    check(is_account(transit), "transit account does not exist");
    check(is_account(recipient), "recipient account does not exist");

    workflows _workflows(_self, 0);
    auto wfidx = _workflows.get_index<name("workflow")>();
    check(wfidx.find(get_seq_key(issuerid, workflow)) != wfidx.end(),
          "This workflow ID already exists");
    
    _workflows.emplace(owner, [&]( auto& item ) {
                                item.id = _workflows.available_primary_key();
                                item.issuerid = issuerid;
                                item.workflow = workflow;
                                item.transit = transit;
                                item.recipient = recipient;
                                item.sealscnt = 0;
                              });
  }

  
  ACTION delwflow(uint64_t issuerid, uint64_t workflow)
  {
    issuerids _ids(_self, 0);
    auto iiditr = _ids.find(issuerid);
    check(iiditr != _ids.end(), "Unknown issuer ID");
    name owner = iiditr->owner;
    require_auth(owner);

    workflows _workflows(_self, 0);
    auto wfidx = _workflows.get_index<name("workflow")>();
    auto wfitr = wfidx.find(get_seq_key(issuerid, workflow));
    check(wfitr != wfidx.end(), "Cannot find workflow ID");
    check(wfitr->sealscnt == 0, "This workflow has active seals, cannot delete");
    wfidx.erase(wfitr);
  }


  
    
    

      
    
  
  
 private:

  static inline uint128_t get_seq_key(uint64_t issuerid, uint64_t seq) {
    return ((uint128_t) issuerid << 64) & ((uint128_t) seq);
  }
    
  struct [[eosio::table("issuerids")]] issuerid {
    uint64_t       id;             /* unique ID assigned by issuer */
    name           owner;
    auto primary_key()const { return id; }
  };
    
  typedef eosio::multi_index<
    name("issuerids"), issuerid
    > issuerids;


  struct [[eosio::table("pubkeys")]] pubkey {
    uint64_t       id;             /* autoincrement */
    uint64_t       issuerid;       /* issuer account may have multiple issuer IDs */
    uint64_t       seq_start;      /* first sequence number of seals signed with key */
    uint64_t       seq_end;        /* last sequence number of seals signed with key */
    public_key     pubkey;
    auto primary_key()const { return id; }
    uint128_t get_seqstart()const { return get_seq_key(issuerid, seq_start); }
    uint128_t get_seqend()const { return get_seq_key(issuerid, seq_end); }
  };
  EOSLIB_SERIALIZE(pubkey, (id)(issuerid)(seq_start)(seq_end)(pubkey));
  
  typedef eosio::multi_index<
    name("pubkeys"), pubkey,
    indexed_by<name("seqstart"), const_mem_fun<pubkey, uint128_t, &pubkey::get_seqstart>>,
    indexed_by<name("seqend"), const_mem_fun<pubkey, uint128_t, &pubkey::get_seqend>>
    > pubkeys;
  
  
  struct [[eosio::table("workflows")]] workflow {
    uint64_t       id;             /* autoincrement */
    uint64_t       issuerid;  
    uint64_t       workflow;       /* workflow ID assigned by issuer */
    name           transit;        /* account name of transport company */
    name           recipient;      /* account name of recipient */
    uint64_t       sealscnt;       /* number of seals in the workflow */
    auto primary_key()const { return id; }
    uint128_t get_workflow()const { return get_seq_key(issuerid, workflow); }
  };

  typedef eosio::multi_index<
    name("workflowes"), workflow,
    indexed_by<name("workflow"), const_mem_fun<workflow, uint128_t, &workflow::get_workflow>>
    > workflows;


  
  struct [[eosio::table("seals")]] seal {
    uint64_t       id;             /* autoincrement */
    uint64_t       issuerid;
    uint64_t       seqnum;         /* unique sequence number within this issuer ID */
    checksum256    sighash;        /* sha256 of on-chip signature */
    time_point_sec expires;        /* before expiration date, only the recipient can delete an entry */
    uint64_t       workflow;
    name           status;         /* status field that is modified by issuer, transit and recipient */
    auto primary_key()const { return id; }
    uint128_t get_seq()const { return get_seq_key(issuerid, seqnum); }
    uint64_t get_expires()const { return expires.utc_seconds; }
  };

  typedef eosio::multi_index<
    name("seals"), seal,
    indexed_by<name("seq"), const_mem_fun<seal, uint128_t, &seal::get_seq>>,
    indexed_by<name("expires"), const_mem_fun<seal, uint64_t, &seal::get_expires>>
    > seals;  
};

  
