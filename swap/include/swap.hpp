#include <helpers.hpp>
#include <eosio/singleton.hpp>
#include <safemath.hpp>
#include <math.h>

CONTRACT swap : public contract {
    public:
        using contract::contract;
        static constexpr name MANAGER_ACCOUNT { name("admin.ttr") };
        static constexpr name FEE_ACCOUNT { name("fees.ttr") };
        static constexpr name MINE_CONTRACT { name("mine.ttr") };
        static constexpr name LP_TOKEN_CONTRACT { name("lptoken.ttr") };
        static constexpr name LOG_CONTRACT { name("logs.ttr") };
        static constexpr name ORACLE_CONTRACT { name("oracle.ttr") };

        static const uint64_t MINIMUM_LIQUIDITY = 10000;

        swap(name receiver, name code, datastream<const char *> ds): contract(receiver, code, ds),
            _pairs(_self, _self.value),
            _globals(_self, _self.value) {
            if (_globals.exists()) {
                _global = _globals.get();
            } else {
                _global = { 0, 0, 20, 0 };
            }
        }

        ACTION modifystatus(uint8_t contract_status, uint8_t mine_status);
        ACTION modifyfees(uint8_t trade_fee, uint8_t protocol_fee);

        ACTION createpair(name creator, extended_symbol token0, extended_symbol token1);
        ACTION removepair(uint64_t pair_id);

        ACTION addliquidity(name owner, uint64_t pair_id);
        ACTION refund(name owner, uint64_t pair_id);

        ACTION checkbalance(const extended_symbol &token);
        
        void handle_transfer(name from, name to, asset quantity, string memo, name code);

    private:
        TABLE pair {
            uint64_t id;
            symbol_code code;
            extended_symbol token0;
            extended_symbol token1;
            asset reserve0;
            asset reserve1;
            uint64_t total_liquidity;
            block_timestamp created_time;
            block_timestamp updated_time;
            uint64_t primary_key() const { return id; }
            uint64_t hash() const { return str_hash(token0, token1); }
            EOSLIB_SERIALIZE(pair, (id)(code)(token0)(token1)(reserve0)(reserve1)(total_liquidity)(created_time)(updated_time))
        };

        TABLE deposit {
            name owner;
            asset quantity0;
            asset quantity1;
            uint64_t primary_key() const { return owner.value; }
            EOSLIB_SERIALIZE(deposit, (owner)(quantity0)(quantity1))
        };

        TABLE global {
            uint8_t contract_status;
            uint8_t mine_status;
            uint8_t trade_fee; 
            uint8_t protocol_fee;
            EOSLIB_SERIALIZE(global, (contract_status)(mine_status)(trade_fee)(protocol_fee))
        };

        TABLE reserve {
            name contract;
            asset    balance;
            uint64_t primary_key() const { return balance.symbol.code().raw(); }
            EOSLIB_SERIALIZE(reserve, (balance))
        };

        typedef multi_index<"pairs"_n, pair, indexed_by<"byhash"_n, const_mem_fun<pair, uint64_t, &pair::hash>>> pairs_mi;
        typedef multi_index<"deposits"_n, deposit> deposits_mi;
        typedef multi_index<"reserves"_n, reserve> reserves_mi;
        typedef singleton<"global"_n, global> global_sig;

        global_sig _globals;
        global _global;
        pairs_mi _pairs;
        
        
        extended_asset swap_token(uint64_t pair_id, name from, name contract, asset quantity);
        void update_pair(uint64_t pair_id, uint64_t balance0, uint64_t balance1, uint64_t reserve0, uint64_t reserve1);
        
        void handle_swap(std::vector<uint64_t> ids, name from, name contract, asset quantity, uint64_t min_out);
        // void handle_swap_support_fees(std::vector<uint64_t> ids, name from, name contract, asset quantity, uint64_t min_out);

        void handle_deposit(uint64_t pair_id, name owner, name contract, asset quantity);
        
        void handle_rmliquidity(name owner, name contract, asset quantity);
        void remove_liquidity(name owner, uint64_t pair_id, uint64_t amount);
        
        uint64_t get_output(uint64_t amount_in, uint64_t reserve_in, uint64_t reserve_out, uint64_t fee);

        uint128_t get_price_cumulative(uint128_t price_cumulative_last, uint128_t price, uint128_t bloks_elapsed) {
            uint128_t add_price_cumulative = price * bloks_elapsed;
            if (add_price_cumulative / price == bloks_elapsed) {
                uint128_t price_cumulative = price_cumulative_last + (add_price_cumulative / 2); // 500ms / block
                if (price_cumulative >= price_cumulative_last) {
                    return price_cumulative;
                } else {
                    return 0;
                }
            }
            return price_cumulative_last;
        }


        void check_currency_balance(const extended_symbol &token) {
            auto data = std::make_tuple(token);
            action(permission_level{_self, "active"_n}, _self, "checkbalance"_n, data).send();
        }

        void add_currency_balance(const name &contract, const asset &quantity, bool is_check=false) {
            reserves_mi reserves(_self, contract.value);
            auto itr = reserves.find(quantity.symbol.code().raw());
            if (itr == reserves.end()) {
                itr = reserves.emplace(_self, [&]( auto& s) {
                    s.balance = quantity;
                });
            } else {
                reserves.modify(itr, same_payer, [&]( auto& s) {
                    s.balance += quantity;
                });
            }
            if (is_check) {
                asset balance = get_balance(contract, _self, quantity.symbol);
                check(itr->balance == balance, "Balance check: wrong amount");
            }
        }

        // 出账时减少币种余额
        void sub_currency_balance(const name &contract, const asset &quantity, bool is_check=false) {
            reserves_mi reserves(_self, contract.value);
            auto itr = reserves.find(quantity.symbol.code().raw());
            if (itr != reserves.end()) {
                if (itr->balance == quantity) {
                    reserves.erase(itr);
                } else {
                    reserves.modify(itr, same_payer, [&]( auto& s) {
                        s.balance -= quantity;
                    });
                }
            } else {
             check(false, "Cannot find the quantity symbol");
            }
            if (is_check) {
                asset balance = get_balance(contract, _self, quantity.symbol);
                check(itr->balance == balance, "Balance check: wrong amount");
            }
        }

        void transfer_to(const name &contract, const name &to, const asset &quantity, string memo) {
            inline_transfer(contract, _self, to, quantity, memo);
            sub_currency_balance(contract, quantity);
        }

};