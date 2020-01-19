```
alias tTcleos='cleos -u http://testnet.eossweden.eu'

tTcleos system buyram sealtest1111 sealtest1111 -k 400

tTcleos set contract sealtest1111 ./ sealregistry.wasm sealregistry.abi



tTcleos push action sealtest1111 addiid '["sealissuer11", 11]' -p sealissuer11@active
# success
# repeat again -> error: This issuer ID already exists

tTcleos get table sealtest1111 0 issuerids

tTcleos push action sealtest1111 addiid '["sealissuer11", 12]' -p sealissuer11@active


tTcleos push action sealtest1111 addkey '[11, 0, 99, "EOS4x2xG59GxFK2NbyPnFnZ7EvA9w8Thib9ruh8VZPzvZby1TRUyA"]' -p sealissuer11@active
# success
tTcleos get table sealtest1111 0 pubkeys

# repeat 0-99, error: Overlapping intervals
# 99-0, error: start number should not be greater than end
# 99-199, error: Overlapping intervals
# 110-199, success
# 100-110, error: Overlapping intervals
# 100-109, success

tTcleos -v push action sealtest1111 addkey '[12, 0, 199, "EOS4x2xG59GxFK2NbyPnFnZ7EvA9w8Thib9ruh8VZPzvZby1TRUyA"]' -p sealissuer11@active

tTcleos -v push action sealtest1111 addkey '[12, 300, 399, "EOS4x2xG59GxFK2NbyPnFnZ7EvA9w8Thib9ruh8VZPzvZby1TRUyA"]' -p sealissuer11@active

tTcleos push action sealtest1111 addkey '[11, 200, 399, "EOS4x2xG59GxFK2NbyPnFnZ7EvA9w8Thib9ruh8VZPzvZby1TRUyA"]' -p sealissuer11@active



tTcleos push action sealtest1111 addwflow '[11, 100, "Alice ships potatos to Bob", "alice1111111", "alice1111111"]' -p sealissuer11@active
# error: transit account does not exist

tTcleos push action sealtest1111 addwflow '[11, 100, "Alice ships potatos to Bob", "sealtransit1", "sealrcpnt111"]' -p sealissuer11@active
tTcleos get table sealtest1111 0 workflows

#repeat, error: This workflow ID already exists


tTcleos push action sealtest1111 delwflow '[11, 100]' -p sealissuer11@active
tTcleos get table sealtest1111 0 workflows
# repeat delwflow, error: Cannot find workflow ID


tTcleos push action sealtest1111 addwflow '[11, 100, "Alice ships potatos to Bob", "sealtransit1", "sealrcpnt111"]' -p sealissuer11@active
tTcleos push action sealtest1111 addwflow '[11, 200, "Alice ships tomatos to Bob", "sealtransit1", "sealrcpnt111"]' -p sealissuer11@active
tTcleos push action sealtest1111 addwflow '[11, 300, "Alice ships bananas to Bob", "sealtransit1", "sealrcpnt111"]' -p sealissuer11@active


tTcleos push action sealtest1111 addwflow '[11, 500, "blah", "sealtransit1", "sealrcpnt111"]' -p sealtransit1@active
# error: missing authority of sealissuer11


tTcleos push action sealtest1111 addseal '[11, 1, "db384f2d658e7e49a9b3c5ae9a356c83c0edb705e7a38db02103bb0647aaad03", 10, 100, "new"]' -p sealtransit1@active
# error: missing authority of sealissuer11

tTcleos push action sealtest1111 addseal '[11, 1, "db384f2d658e7e49a9b3c5ae9a356c83c0edb705e7a38db02103bb0647aaad03", 10, 100, "new"]' -p sealissuer11@active
# repead, error: Duplicate sequence number

tTcleos get table sealtest1111 0 seals
# Expire date is correct, set to +10 days

tTcleos push action sealtest1111 addseal '[11, 2, "db384f2d658e7e49a9b3c5ae9a356c83c0edb705e7a38db02103bb0647aaad03", 0, 100, "new"]' -p sealissuer11@active


tTcleos push action sealtest1111 delwflow '[11, 100]' -p sealissuer11@active
# error: This workflow has active seals, cannot delete

tTcleos push action sealtest1111 wipeexpired '[10]' -p sealrcpnt111@active
tTcleos get table sealtest1111 0 seals
# only one remains in the table

tTcleos push action sealtest1111 delseal '[11, 2, "done"]' -p sealissuer11@active
# error: Cannot find the sequence number

tTcleos push action sealtest1111 delseal '[11, 1, "done"]' -p sealissuer11@active
# error:  Only recepient can delete the seal


























```