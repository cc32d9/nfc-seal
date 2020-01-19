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




















```