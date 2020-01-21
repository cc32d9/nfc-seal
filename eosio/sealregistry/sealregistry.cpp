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
#include <eosio/system.hpp>
#include <eosio/string.hpp>

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
    if( enditr != endidx.end() && enditr->issuerid == issuerid ) {
      check(enditr->seq_start > seq_end, "Overlapping intervals");
    }

    _pubkeys.emplace(owner, [&]( auto& item ) {
                              item.id = _pubkeys.available_primary_key();
                              item.issuerid = issuerid;
                              item.seq_start = seq_start;
                              item.seq_end = seq_end;
                              item.pubkey = pubkey;
                              item.revoked = false;
                              item.revoked_at.utc_seconds = 0;
                              item.revoke_reason = "";
                            });
  }


  ACTION revokekey(uint64_t issuerid, uint64_t seqnum, string reason)
  {
    issuerids _ids(_self, 0);
    auto iiditr = _ids.find(issuerid);
    check(iiditr != _ids.end(), "Unknown issuer ID");
    name owner = iiditr->owner;
    require_auth(owner);

    pubkeys _pubkeys(_self, 0);
    /* search for a matching interval */    
    auto endidx = _pubkeys.get_index<name("seqend")>();
    auto enditr = endidx.lower_bound(get_seq_key(issuerid, seqnum));
    check( enditr != endidx.end() && enditr->issuerid == issuerid && enditr->seq_start <= seqnum,
           "Cannot find matching public key");
    _pubkeys.modify( *enditr, same_payer, [&]( auto& item ) {
                                            item.revoked = true;
                                            item.revoked_at = time_point_sec(current_time_point());
                                            item.revoke_reason = reason;
                                          });
  }
  
  
  ACTION wipekeys()
  {
    require_auth(_self);
    pubkeys _pubkeys(_self, 0);
    auto itr = _pubkeys.lower_bound(0);
    while( itr != _pubkeys.end() ) {
      itr = _pubkeys.erase(itr);
    }
  }
      
  

  ACTION addwflow(uint64_t issuerid, uint64_t workflow, string description, name transit, name recipient)
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
    check(wfidx.find(get_seq_key(issuerid, workflow)) == wfidx.end(),
          "This workflow ID already exists");
    
    _workflows.emplace(owner, [&]( auto& item ) {
                                item.id = _workflows.available_primary_key();
                                item.issuerid = issuerid;
                                item.workflow = workflow;
                                item.description = description;
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


  
  ACTION addseal(uint64_t issuerid, uint64_t seqnum, checksum256 sighash,
                 uint32_t expires_days, uint64_t workflow, name status)
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
    
    seals _seals(_self, 0);
    auto slidx = _seals.get_index<name("seq")>();
    check(slidx.find(get_seq_key(issuerid, seqnum)) == slidx.end(),
          "Duplicate sequence number");

    pubkeys _pubkeys(_self, 0);
    /* search for a matching interval */    
    auto endidx = _pubkeys.get_index<name("seqend")>();
    auto enditr = endidx.lower_bound(get_seq_key(issuerid, seqnum));
    check( enditr != endidx.end() && enditr->issuerid == issuerid && enditr->seq_start <= seqnum,
           "Cannot find matching public key");

    check(!enditr->revoked, "The matching public key is revoked");
    
    _workflows.modify( *wfitr, same_payer, [&]( auto& item ) {
                                             item.sealscnt++;
                                           });
    
    _seals.emplace(owner,
                   [&]( auto& item ) {
                     item.id = _seals.available_primary_key();
                     item.issuerid = issuerid;
                     item.seqnum = seqnum;
                     item.sighash = sighash;
                     item.expires = time_point_sec(current_time_point() +
                                                   microseconds(1000000ll * expires_days * 86400));
                     item.workflow = workflow;
                     item.status = status;
                     item.updated_by = owner;
                     item.updated_at = time_point_sec(current_time_point());
                   });
  }

  
  ACTION setstatus(name updated_by, uint64_t issuerid, uint64_t seqnum, name status, string memo)
  {
    require_auth(updated_by);
    issuerids _ids(_self, 0);
    auto iiditr = _ids.find(issuerid);
    check(iiditr != _ids.end(), "Unknown issuer ID");
    name owner = iiditr->owner;

    seals _seals(_self, 0);
    auto slidx = _seals.get_index<name("seq")>();
    auto slitr = slidx.find(get_seq_key(issuerid, seqnum));
    check(slitr != slidx.end(), "Cannot find the sequence number");

    workflows _workflows(_self, 0);
    auto wfidx = _workflows.get_index<name("workflow")>();
    auto wfitr = wfidx.find(get_seq_key(issuerid, slitr->workflow));
    check(wfitr != wfidx.end(), "This must never happen 1");

    check((updated_by == owner || updated_by == wfitr->transit || updated_by == wfitr->recipient),
          "Only issuer, transit or recepient can modify the status");

    check(slitr->status != status, "This seal has already this status");
    
    _seals.modify( *slitr, same_payer, [&]( auto& item ) {
                                         item.status = status;
                                         item.updated_by = updated_by;
                                         item.updated_at = time_point_sec(current_time_point());
                                       });
  }

  
  ACTION delseal(uint64_t issuerid, uint64_t seqnum, string memo)
  {
    seals _seals(_self, 0);
    auto slidx = _seals.get_index<name("seq")>();
    auto slitr = slidx.find(get_seq_key(issuerid, seqnum));
    check(slitr != slidx.end(), "Cannot find the sequence number");

    workflows _workflows(_self, 0);
    auto wfidx = _workflows.get_index<name("workflow")>();
    auto wfitr = wfidx.find(get_seq_key(issuerid, slitr->workflow));
    check(wfitr != wfidx.end(), "This must never happen 2");

    check(has_auth(wfitr->recipient), "Only recepient can delete the seal");

    _workflows.modify( *wfitr, same_payer, [&]( auto& item ) {
                                             item.sealscnt--;
                                           });
    
    slidx.erase(slitr);
  }


  ACTION wipeexpired(uint16_t count)
  {
    bool done_something = false;
    auto _now = time_point_sec(current_time_point());

    workflows _workflows(_self, 0);
    auto wfidx = _workflows.get_index<name("workflow")>();
    
    seals _seals(_self, 0);
    auto slidx = _seals.get_index<name("expires")>();
    auto slitr = slidx.lower_bound(0);
    while( count-- > 0 && slitr != slidx.end() && slitr->expires <= _now ) {

      auto wfitr = wfidx.find(get_seq_key(slitr->issuerid, slitr->workflow));
      check(wfitr != wfidx.end(), "This must never happen 3");
      _workflows.modify( *wfitr, same_payer, [&]( auto& item ) {
                                               item.sealscnt--;
                                             });
      slitr = slidx.erase(slitr);
      done_something = true;
    }

    check(done_something, "There are no expired seals at the moment");
  }

    
    
    
    
  
 private:

  static inline uint128_t get_seq_key(uint64_t issuerid, uint64_t seq) {
    return ((uint128_t) issuerid << 64) | ((uint128_t) seq);
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
    bool           revoked;
    time_point_sec revoked_at;     /* timestamp of key invalidation */
    string         revoke_reason;
    auto primary_key()const { return id; }
    uint128_t get_seqend()const { return get_seq_key(issuerid, seq_end); }
  };
  EOSLIB_SERIALIZE(pubkey, (id)(issuerid)(seq_start)(seq_end)(pubkey)(revoked)(revoked_at)(revoke_reason));
  
  typedef eosio::multi_index<
    name("pubkeys"), pubkey,
    indexed_by<name("seqend"), const_mem_fun<pubkey, uint128_t, &pubkey::get_seqend>>
    > pubkeys;
  
  
  struct [[eosio::table("workflows")]] workflow {
    uint64_t       id;             /* autoincrement */
    uint64_t       issuerid;  
    uint64_t       workflow;       /* workflow ID assigned by issuer */
    string         description;
    name           transit;        /* account name of transport company */
    name           recipient;      /* account name of recipient */
    uint64_t       sealscnt;       /* number of seals in the workflow */
    auto primary_key()const { return id; }
    uint128_t get_workflow()const { return get_seq_key(issuerid, workflow); }
  };

  typedef eosio::multi_index<
    name("workflows"), workflow,
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
    name           updated_by;     /* account that made latest status update */ 
    time_point_sec updated_at;     /* tiumestamp of latest update */
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

  
